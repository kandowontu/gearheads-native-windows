#include "attract.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace gh {
namespace {

std::string trim(std::string value) {
    const auto content = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), content));
    value.erase(std::find_if(value.rbegin(), value.rend(), content).base(), value.end());
    return value;
}

int integer(const std::string& value, int line_number) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing data");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(
            "Invalid integer in anim.dat line " + std::to_string(line_number)
        );
    }
}

std::pair<int, int> pair_value(std::string token, int line_number) {
    const std::size_t comma = token.find(',');
    if (comma == std::string::npos) {
        // Four shipped records contain the original typo `07000`, meaning
        // the otherwise ubiquitous `0,7000` lane pair.
        if (token == "07000") return {0, 7000};
        throw std::runtime_error(
            "Missing comma in anim.dat line " + std::to_string(line_number)
        );
    }
    return {
        integer(token.substr(0, comma), line_number),
        integer(token.substr(comma + 1), line_number),
    };
}

std::wstring ascii_to_wide(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

}  // namespace

AttractDatabase::AttractDatabase(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Could not open assets/data/anim.dat");
    std::string line;
    int line_number = 0;
    int declared_sequences = -1;
    AttractSequence* current = nullptr;
    bool terminated = true;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(std::move(line));
        if (line.empty()) continue;
        if (declared_sequences < 0) {
            if (line.size() < 2 || line.front() != 'Q') {
                throw std::runtime_error("anim.dat does not begin with its Q count");
            }
            declared_sequences = integer(line.substr(1), line_number);
            continue;
        }
        if (line.rfind("A -", 0) == 0) {
            if (current != nullptr && !terminated) {
                throw std::runtime_error("anim.dat sequence lacks a terminator");
            }
            sequences_.push_back({ascii_to_wide(trim(line.substr(3))), {}});
            current = &sequences_.back();
            terminated = false;
            continue;
        }
        if (current == nullptr || terminated) {
            throw std::runtime_error(
                "anim.dat event appears outside a sequence on line " +
                std::to_string(line_number)
            );
        }
        std::vector<std::string> tokens;
        std::size_t cursor = 0;
        while (cursor < line.size()) {
            while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            const std::size_t start = cursor;
            while (cursor < line.size() &&
                   !std::isspace(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            if (cursor > start) tokens.push_back(line.substr(start, cursor - start));
        }
        if (tokens.size() != 6) {
            throw std::runtime_error(
                "anim.dat event needs one header and five lanes on line " +
                std::to_string(line_number)
            );
        }
        const auto header = pair_value(tokens[0], line_number);
        AttractEvent event;
        event.timestamp_ms = header.first;
        event.player = header.second;
        bool all_zero = event.timestamp_ms == 0 && event.player == 0;
        for (std::size_t lane = 0; lane < event.lanes.size(); ++lane) {
            const auto values = pair_value(tokens[lane + 1], line_number);
            event.lanes[lane] = {values.first, values.second};
            all_zero = all_zero && values.first == 0 && values.second == 0;
        }
        if (all_zero) {
            terminated = true;
            continue;
        }
        if (event.timestamp_ms < 0 || event.player < 0 || event.player > 1) {
            throw std::runtime_error("anim.dat event header is out of range");
        }
        for (const AttractLane& lane : event.lanes) {
            if (lane.definition < 0 || lane.definition > 12 || lane.winding < 0) {
                throw std::runtime_error("anim.dat lane is out of range");
            }
        }
        current->events.push_back(event);
    }
    if (current != nullptr && !terminated) {
        throw std::runtime_error("anim.dat final sequence lacks a terminator");
    }
    if (declared_sequences != static_cast<int>(sequences_.size())) {
        throw std::runtime_error("anim.dat Q count does not match its sequences");
    }
    for (const AttractSequence& sequence : sequences_) {
        if (sequence.events.empty() ||
            !std::is_sorted(
                sequence.events.begin(), sequence.events.end(),
                [](const AttractEvent& left, const AttractEvent& right) {
                    return left.timestamp_ms < right.timestamp_ms;
                }
            )) {
            throw std::runtime_error("anim.dat sequence is empty or not time ordered");
        }
    }
}

}  // namespace gh
