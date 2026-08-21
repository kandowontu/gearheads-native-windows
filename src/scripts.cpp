#include "scripts.hpp"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <unordered_set>

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
    if (length <= 0) throw std::runtime_error("script.ini is not valid UTF-8");
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

int integer(const std::wstring& value) {
    std::size_t consumed = 0;
    const int result = std::stoi(value, &consumed, 10);
    if (consumed != value.size()) throw std::invalid_argument("trailing data");
    return result;
}

std::wstring archive_stem(std::wstring value) {
    value = lower(value);
    std::replace(value.begin(), value.end(), L'\\', L'/');
    const std::size_t slash = value.find_last_of(L'/');
    if (slash != std::wstring::npos) value = value.substr(slash + 1);
    const std::size_t dot = value.find_last_of(L'.');
    if (dot != std::wstring::npos) value.erase(dot);
    return value;
}

}  // namespace

ScriptDatabase::ScriptDatabase(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open assets/data/script.ini");
    const std::string bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()
    );
    const std::wstring text = utf8_to_wide(bytes);
    static const std::wregex indexed_key(LR"(^([a-z]+)([0-9]+)$)");

    ScriptSection* current = nullptr;
    std::vector<AnimationFrame> raw_frames;
    std::unordered_set<std::wstring> closed_states;
    std::wstring last_state;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find(L'\n', start);
        std::wstring line = trim(text.substr(
            start, newline == std::wstring::npos ? std::wstring::npos : newline - start
        ));
        if (!line.empty() && line.front() != L';') {
            if (line.front() == L'[' && line.back() == L']') {
                const std::wstring name = lower(trim(line.substr(1, line.size() - 2)));
                if (name.empty() || index_.contains(name)) {
                    throw std::runtime_error("Invalid or duplicate script section");
                }
                index_[name] = sections_.size();
                sections_.push_back({name, {}, {}, {}});
                current = &sections_.back();
                raw_frames.clear();
                closed_states.clear();
                last_state.clear();
            } else if (current != nullptr) {
                const std::size_t equals = line.find(L'=');
                if (equals == std::wstring::npos) throw std::runtime_error("Malformed script entry");
                const std::wstring key = lower(trim(line.substr(0, equals)));
                const auto arguments = words(trim(line.substr(equals + 1)));
                if (key == L"ar" && !arguments.empty()) {
                    current->archive = archive_stem(arguments.front());
                } else {
                    std::wsmatch match;
                    if (!std::regex_match(key, match, indexed_key) || arguments.empty()) {
                        if (newline == std::wstring::npos) break;
                        start = newline + 1;
                        continue;
                    }
                    const std::wstring state = match[1].str();
                    const int ordinal = integer(match[2].str());
                    if (state == L"s") {
                        if (arguments.front().empty() || arguments.front().front() != L'@') {
                            if (newline == std::wstring::npos) break;
                            start = newline + 1;
                            continue;
                        }
                        const std::wstring resource = lower(arguments.front().substr(1));
                        current->sounds[ordinal] =
                            std::filesystem::path(L"sounds") / (resource + L".wav");
                    } else if (!current->archive.empty()) {
                        AnimationFrame frame;
                        if (!arguments.front().empty() && arguments.front().front() == L'@') {
                            const std::wstring resource = lower(arguments.front().substr(1));
                            frame.image = std::filesystem::path(L"sprites") / current->archive /
                                          (resource + L".png");
                        } else {
                            int relative = 0;
                            try {
                                relative = integer(arguments.front());
                            } catch (const std::exception&) {
                                relative = 0;
                            }
                            const long long referenced =
                                static_cast<long long>(raw_frames.size()) + relative;
                            if (relative >= 0 || referenced < 0 ||
                                referenced >= static_cast<long long>(raw_frames.size())) {
                                if (newline == std::wstring::npos) break;
                                start = newline + 1;
                                continue;
                            }
                            frame = raw_frames[static_cast<std::size_t>(referenced)];
                        }
                        if (arguments.size() > 1) {
                            try {
                                frame.ticks = std::max(1, integer(arguments[1]));
                            } catch (const std::exception&) {
                                frame.ticks = 1;
                            }
                        }
                        for (std::size_t index = 2; index < arguments.size(); ++index) {
                            try {
                                frame.parameters.push_back(integer(arguments[index]));
                            } catch (const std::exception&) {
                                break;
                            }
                        }
                        raw_frames.push_back(frame);
                        if (!last_state.empty() && state != last_state) {
                            closed_states.insert(last_state);
                        }
                        if (!closed_states.contains(state)) {
                            current->animations[state].push_back(std::move(frame));
                        }
                        last_state = state;
                    }
                }
            }
        }
        if (newline == std::wstring::npos) break;
        start = newline + 1;
    }
    if (sections_.empty()) throw std::runtime_error("script.ini contains no sections");
}

const ScriptSection* ScriptDatabase::find(const std::wstring& name) const {
    const auto found = index_.find(lower(name));
    return found == index_.end() ? nullptr : &sections_[found->second];
}

const std::vector<AnimationFrame>& ScriptDatabase::animation(
    const std::wstring& section, const std::wstring& state
) const {
    static const std::vector<AnimationFrame> empty;
    const ScriptSection* definition = find(section);
    if (definition == nullptr) return empty;
    const auto found = definition->animations.find(lower(state));
    return found == definition->animations.end() ? empty : found->second;
}

const std::vector<AnimationFrame>& ScriptDatabase::locomotion(const std::wstring& section) const {
    const auto& walking = animation(section, L"w");
    if (!walking.empty()) return walking;
    // Clucketta's normal eight-frame walk is named y rather than w; her w
    // commands are numeric motion instructions, not sprite references.
    const auto& alternate = animation(section, L"y");
    if (!alternate.empty()) return alternate;
    return animation(section, L"x");
}

std::filesystem::path ScriptDatabase::sound(const std::wstring& section, int ordinal) const {
    const ScriptSection* definition = find(section);
    if (definition == nullptr) return {};
    const auto found = definition->sounds.find(ordinal);
    return found == definition->sounds.end() ? std::filesystem::path{} : found->second;
}

}  // namespace gh
