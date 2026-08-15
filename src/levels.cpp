#include "levels.hpp"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace gh {
namespace {

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::wstring trim(std::wstring value) {
    const auto content = [](wchar_t character) { return !iswspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), content));
    value.erase(std::find_if(value.rbegin(), value.rend(), content).base(), value.end());
    return value;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
    );
    if (length <= 0) throw std::runtime_error("gearhead.ini is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length
    );
    return result;
}

int integer(const std::wstring& value, const char* field) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing data");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("Invalid integer in level field ") + field);
    }
}

std::vector<std::wstring> words(const std::wstring& value) {
    std::vector<std::wstring> result;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        while (cursor < value.size() && iswspace(value[cursor])) ++cursor;
        const std::size_t start = cursor;
        while (cursor < value.size() && !iswspace(value[cursor])) ++cursor;
        if (cursor > start) result.push_back(value.substr(start, cursor - start));
    }
    return result;
}

std::vector<std::wstring> split(const std::wstring& value, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t next = value.find(delimiter, start);
        result.push_back(trim(value.substr(
            start, next == std::wstring::npos ? std::wstring::npos : next - start
        )));
        if (next == std::wstring::npos) break;
        start = next + 1;
    }
    return result;
}

std::filesystem::path convert_background(std::wstring value) {
    value = lower(value);
    std::replace(value.begin(), value.end(), L'\\', L'/');
    const std::size_t slash = value.find_last_of(L'/');
    if (slash != std::wstring::npos) value = value.substr(slash + 1);
    const std::size_t dot = value.find_last_of(L'.');
    if (dot != std::wstring::npos) value.erase(dot);
    if (value == L"missing" || value.empty()) return {};
    if (value.rfind(L"bonus", 0) == 0) {
        return std::filesystem::path(L"backgrounds") / (value + L".png");
    }
    return std::filesystem::path(L"backgrounds") / (value + L"_bmp.png");
}

std::filesystem::path convert_music(std::wstring value) {
    value = lower(value);
    std::replace(value.begin(), value.end(), L'\\', L'/');
    const std::size_t slash = value.find_last_of(L'/');
    if (slash != std::wstring::npos) value = value.substr(slash + 1);
    return std::filesystem::path(L"music") / value;
}

std::wstring convert_archive_stem(std::wstring value) {
    value = lower(value);
    std::replace(value.begin(), value.end(), L'\\', L'/');
    const std::size_t slash = value.find_last_of(L'/');
    if (slash != std::wstring::npos) value = value.substr(slash + 1);
    const std::size_t dot = value.find_last_of(L'.');
    if (dot != std::wstring::npos) value.erase(dot);
    return value;
}

Rectangle rectangle(const std::wstring& value, const char* field) {
    const auto values = words(value);
    if (values.size() != 4) {
        throw std::runtime_error(std::string("Level field needs four values: ") + field);
    }
    return {
        integer(values[0], field),
        integer(values[1], field),
        integer(values[2], field),
        integer(values[3], field),
    };
}

}  // namespace

LevelDatabase::LevelDatabase(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open assets/data/gearhead.ini");
    const std::string bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()
    );
    const std::wstring text = utf8_to_wide(bytes);
    static const std::wregex obstacle_pattern(LR"(^k([0-9]{2})n([0-9]{1,2})$)");
    LevelDefinition* current = nullptr;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find(L'\n', start);
        std::wstring line = trim(text.substr(
            start, newline == std::wstring::npos ? std::wstring::npos : newline - start
        ));
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty() && line.front() != L';') {
            if (line.front() == L'[' && line.back() == L']') {
                const std::wstring section = line.substr(1, line.size() - 2);
                const std::wstring folded = lower(section);
                current = nullptr;
                if (folded.rfind(L"level", 0) == 0) {
                    LevelDefinition level;
                    level.section = section;
                    if (section.size() > 5) level.theme = towupper(section[5]);
                    if (section.size() >= 9 && iswdigit(section[6]) && iswdigit(section[7]) &&
                        iswdigit(section[8])) {
                        level.tournament_number = integer(section.substr(6, 3), "section number");
                        if (iswdigit(section.back())) {
                            level.tournament_intelligence = section.back() - L'0';
                        }
                    }
                    levels_.push_back(std::move(level));
                    current = &levels_.back();
                }
            } else if (current != nullptr) {
                const std::size_t equals = line.find(L'=');
                if (equals == std::wstring::npos) {
                    throw std::runtime_error("Malformed entry in gearhead.ini");
                }
                const std::wstring key = lower(trim(line.substr(0, equals)));
                std::wstring value = trim(line.substr(equals + 1));
                if (key == L"name") {
                    current->name = value;
                } else if (key == L"bg") {
                    current->background = convert_background(value);
                } else if (key == L"song") {
                    auto songs = split(value, L';');
                    if (songs.size() > 1 && !songs.front().empty() &&
                        std::all_of(songs.front().begin(), songs.front().end(), iswdigit)) {
                        songs.erase(songs.begin());
                    }
                    for (const std::wstring& song : songs) {
                        if (!song.empty()) current->music.push_back(convert_music(song));
                    }
                } else if (key == L"stage") {
                    current->stage = rectangle(value, "stage");
                } else if (key == L"leftgauge") {
                    current->left_gauge = rectangle(value, "LeftGauge");
                } else if (key == L"rightgauge") {
                    current->right_gauge = rectangle(value, "RightGauge");
                } else if (key == L"lefttoybox") {
                    current->left_toybox = rectangle(value, "LeftToyBox");
                } else if (key == L"righttoybox") {
                    current->right_toybox = rectangle(value, "RightToyBox");
                } else if (key == L"gauge") {
                    current->gauge_archive = convert_archive_stem(value);
                } else if (key == L"friction") {
                    current->friction = integer(value, "friction");
                } else if (key == L"ice") {
                    current->ice = integer(value, "ice") != 0;
                } else if (key == L"pupprob") {
                    current->powerup_probability = integer(value, "pupprob");
                } else if (key == L"leveltoys") {
                    current->level_toys = value;
                } else {
                    std::wsmatch match;
                    if (std::regex_match(key, match, obstacle_pattern)) {
                        const auto values = words(value);
                        if (values.size() != 3) {
                            throw std::runtime_error("Obstacle entry needs three values");
                        }
                        current->obstacles.push_back({
                            integer(match[1].str(), "obstacle code"),
                            integer(match[2].str(), "obstacle ordinal"),
                            integer(values[0], "obstacle direction") != 0,
                            integer(values[1], "obstacle x"),
                            integer(values[2], "obstacle y"),
                        });
                    }
                }
            }
        }
        if (newline == std::wstring::npos) break;
        start = newline + 1;
    }
    if (levels_.empty()) throw std::runtime_error("gearhead.ini contains no levels");
}

const LevelDefinition* LevelDatabase::first_for_theme(wchar_t theme) const {
    const wchar_t desired = towupper(theme);
    const auto found = std::find_if(levels_.begin(), levels_.end(), [desired](const auto& level) {
        return level.theme == desired && !level.background.empty();
    });
    return found == levels_.end() ? nullptr : &*found;
}

const LevelDefinition* LevelDatabase::find_tournament(int number) const {
    if (number <= 0) return nullptr;
    const auto found = std::find_if(levels_.begin(), levels_.end(), [number](const auto& level) {
        return level.tournament_number == number;
    });
    return found == levels_.end() ? nullptr : &*found;
}

}  // namespace gh
