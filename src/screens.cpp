#include "screens.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace gh {
namespace {

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
    );
    if (length <= 0) throw std::runtime_error("screens.ini is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            length
        ) != length) {
        throw std::runtime_error("Could not decode screens.ini");
    }
    return result;
}

std::wstring trim(std::wstring value) {
    const auto content = [](wchar_t character) { return !iswspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), content));
    value.erase(std::find_if(value.rbegin(), value.rend(), content).base(), value.end());
    return value;
}

std::vector<std::wstring> tokenize(const std::wstring& value, int source_line) {
    std::vector<std::wstring> result;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        while (cursor < value.size() && iswspace(value[cursor])) ++cursor;
        if (cursor == value.size()) break;
        std::wstring token;
        if (value[cursor] == L'"') {
            ++cursor;
            while (cursor < value.size() && value[cursor] != L'"') {
                token.push_back(value[cursor++]);
            }
            if (cursor == value.size()) {
                throw std::runtime_error(
                    "Unterminated quote in screens.ini line " + std::to_string(source_line)
                );
            }
            ++cursor;
        } else {
            const std::size_t start = cursor;
            while (cursor < value.size() && !iswspace(value[cursor])) ++cursor;
            token = value.substr(start, cursor - start);
        }
        result.push_back(std::move(token));
    }
    return result;
}

}  // namespace

ScreenDatabase::ScreenDatabase(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open assets/data/screens.ini");
    const std::string bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()
    );
    std::wstring text = utf8_to_wide(bytes);
    if (!text.empty() && text.front() == 0xfeff) text.erase(text.begin());

    ScreenDefinition* current = nullptr;
    std::size_t start = 0;
    int line_number = 1;
    while (start <= text.size()) {
        const std::size_t newline = text.find(L'\n', start);
        std::wstring line = text.substr(
            start, newline == std::wstring::npos ? std::wstring::npos : newline - start
        );
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = trim(std::move(line));
        if (!line.empty() && line.front() != L';') {
            if (line.front() == L'[' && line.back() == L']') {
                const std::wstring name = lower(trim(line.substr(1, line.size() - 2)));
                if (name.empty() || index_.contains(name)) {
                    throw std::runtime_error(
                        "Invalid or duplicate screen section on line " + std::to_string(line_number)
                    );
                }
                index_[name] = screens_.size();
                screens_.push_back({name, {}});
                current = &screens_.back();
            } else {
                if (current == nullptr) {
                    throw std::runtime_error("screens.ini command appears before a section");
                }
                const std::size_t equals = line.find(L'=');
                if (equals == std::wstring::npos) {
                    throw std::runtime_error(
                        "Missing '=' in screens.ini line " + std::to_string(line_number)
                    );
                }
                ScreenCommand command;
                command.key = lower(trim(line.substr(0, equals)));
                command.arguments = tokenize(line.substr(equals + 1), line_number);
                command.source_line = line_number;
                current->commands.push_back(std::move(command));
            }
        }
        if (newline == std::wstring::npos) break;
        start = newline + 1;
        ++line_number;
    }
    if (screens_.empty()) throw std::runtime_error("screens.ini contains no screen sections");
}

const ScreenDefinition* ScreenDatabase::find(const std::wstring& name) const {
    const auto found = index_.find(lower(name));
    return found == index_.end() ? nullptr : &screens_[found->second];
}

}  // namespace gh
