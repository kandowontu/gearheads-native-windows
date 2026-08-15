#include "defaults.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace gh {
namespace {

std::string trim(std::string value) {
    const auto content = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), content));
    value.erase(std::find_if(value.rbegin(), value.rend(), content).base(), value.end());
    return value;
}

std::string folded(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

int parse_integer(const std::string& value, std::size_t line_number) {
    try {
        std::size_t consumed = 0;
        const long parsed = std::stol(value, &consumed, 10);
        if (consumed != value.size() || parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("out of range or trailing data");
        }
        return static_cast<int>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "Invalid integer in runtime-defaults.ini line " + std::to_string(line_number)
        );
    }
}

std::vector<int> parse_vector(const std::string& value, std::size_t line_number) {
    std::vector<int> result;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor]))) {
            ++cursor;
        }
        const std::size_t start = cursor;
        while (cursor < value.size() &&
               !std::isspace(static_cast<unsigned char>(value[cursor]))) {
            ++cursor;
        }
        if (cursor > start) result.push_back(parse_integer(value.substr(start, cursor - start), line_number));
    }
    if (result.empty()) {
        throw std::runtime_error(
            "Empty numeric value in runtime-defaults.ini line " + std::to_string(line_number)
        );
    }
    return result;
}

}  // namespace

DefaultsDatabase::DefaultsDatabase(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open assets/data/runtime-defaults.ini");

    enum class Section { Header, Integers, Strings };
    Section section = Section::Header;
    bool valid_format = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty() || line.front() == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            const std::string name = folded(trim(line.substr(1, line.size() - 2)));
            if (name == "integers") {
                section = Section::Integers;
            } else if (name == "strings") {
                section = Section::Strings;
            } else {
                throw std::runtime_error("Unknown runtime-defaults.ini section");
            }
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error(
                "Malformed runtime-defaults.ini line " + std::to_string(line_number)
            );
        }
        const std::string name = folded(trim(line.substr(0, equals)));
        const std::string value = trim(line.substr(equals + 1));
        if (name.empty()) {
            throw std::runtime_error(
                "Empty name in runtime-defaults.ini line " + std::to_string(line_number)
            );
        }
        if (section == Section::Header) {
            if (name == "format") valid_format = value == "1";
            continue;
        }
        if (section == Section::Integers) {
            if (!integers_.emplace(name, parse_vector(value, line_number)).second) {
                throw std::runtime_error("Duplicate integer default: " + name);
            }
        } else {
            std::string decoded;
            decoded.reserve(value.size());
            for (std::size_t index = 0; index < value.size(); ++index) {
                if (value[index] == '\\') {
                    if (++index == value.size() || value[index] != '\\') {
                        throw std::runtime_error("Invalid string escape in runtime-defaults.ini");
                    }
                }
                decoded.push_back(value[index]);
            }
            if (!strings_.emplace(name, std::move(decoded)).second) {
                throw std::runtime_error("Duplicate string default: " + name);
            }
        }
    }

    if (!valid_format) throw std::runtime_error("Unsupported runtime-defaults.ini format");
    if (integers_.size() != 48 || strings_.size() != 3) {
        throw std::runtime_error("runtime-defaults.ini does not contain all 51 recovered values");
    }
}

const std::vector<int>& DefaultsDatabase::integers(std::string_view name) const {
    const auto found = integers_.find(folded(name));
    if (found == integers_.end()) throw std::runtime_error("Missing integer default: " + std::string(name));
    return found->second;
}

const std::string& DefaultsDatabase::text(std::string_view name) const {
    const auto found = strings_.find(folded(name));
    if (found == strings_.end()) throw std::runtime_error("Missing string default: " + std::string(name));
    return found->second;
}

ToyParameters DefaultsDatabase::toy(std::string_view name) const {
    const auto& raw = integers(name);
    if (raw.size() != 12) throw std::runtime_error("Toy parameter vector does not have 12 values");
    ToyParameters result;
    std::copy(raw.begin(), raw.end(), result.values.begin());
    return result;
}

int DefaultsDatabase::scalar(std::string_view name) const {
    const auto& raw = integers(name);
    if (raw.size() != 1) throw std::runtime_error("Scalar default does not have one value");
    return raw.front();
}

}  // namespace gh
