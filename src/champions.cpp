#include "champions.hpp"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <format>
#include <stdexcept>

namespace gh {
namespace {

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
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), length
    );
    return result;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr
    );
    if (length <= 0) throw std::runtime_error("Champion name cannot be encoded as UTF-8");
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), length, nullptr, nullptr
    );
    return result;
}

int integer(const std::wstring& value) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing data");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid tournament champion score");
    }
}

}  // namespace

ChampionTable::ChampionTable(
    const std::filesystem::path& path,
    std::filesystem::path save_path
) : save_path_(std::move(save_path)) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open assets/data/gearhead.ini");
    const std::string bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()
    );
    const std::wstring text = utf8_to_wide(bytes);
    bool in_score_section = false;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find(L'\n', start);
        std::wstring line = trim(text.substr(
            start, newline == std::wstring::npos ? std::wstring::npos : newline - start
        ));
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.size() >= 2 && line.front() == L'[' && line.back() == L']') {
            std::wstring section = line.substr(1, line.size() - 2);
            std::transform(section.begin(), section.end(), section.begin(), towlower);
            in_score_section = section == L"score";
        } else if (in_score_section && line.size() >= 4 && line.front() == L'#') {
            const std::size_t equals = line.find(L'=');
            const std::size_t space = line.find(L' ', equals + 1);
            if (equals == std::wstring::npos || space == std::wstring::npos) {
                throw std::runtime_error("Malformed tournament champion record");
            }
            entries_.push_back({
                integer(trim(line.substr(equals + 1, space - equals - 1))),
                trim(line.substr(space + 1)),
            });
        }
        if (newline == std::wstring::npos) break;
        start = newline + 1;
    }
    if (entries_.size() != 7 ||
        !std::is_sorted(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
            return left.score > right.score;
        })) {
        throw std::runtime_error("Expected seven ordered tournament champions");
    }
    if (!save_path_.empty() && std::filesystem::is_regular_file(save_path_)) {
        std::ifstream saved(save_path_, std::ios::binary);
        std::string header;
        std::getline(saved, header);
        if (header == "GEARHEADS-NATIVE-CHAMPIONS-1") {
            std::vector<Champion> overlay;
            std::string saved_line;
            while (std::getline(saved, saved_line)) {
                const std::size_t tab = saved_line.find('\t');
                if (tab == std::string::npos) continue;
                try {
                    std::size_t consumed = 0;
                    const int score = std::stoi(saved_line.substr(0, tab), &consumed, 10);
                    if (consumed != tab || score < 0) continue;
                    std::wstring name = utf8_to_wide(saved_line.substr(tab + 1));
                    if (name.empty()) name = L"?";
                    if (name.size() > 15) name.resize(15);
                    overlay.push_back({score, std::move(name)});
                } catch (const std::exception&) {
                }
            }
            if (overlay.size() == 7 &&
                std::is_sorted(overlay.begin(), overlay.end(), [](const auto& left, const auto& right) {
                    return left.score > right.score;
                })) {
                entries_ = std::move(overlay);
            }
        }
    }
}

std::wstring ChampionTable::placeholder(const std::wstring& token) const {
    if (token.size() != 3 || token.front() != L'#' ||
        !iswdigit(token[1]) || !iswdigit(token[2])) {
        return token;
    }
    const int field = (token[1] - L'0') * 10 + token[2] - L'0';
    if (field < 0 || field >= static_cast<int>(entries_.size() * 2)) return token;
    const Champion& entry = entries_[static_cast<std::size_t>(field / 2)];
    return field % 2 == 0 ? entry.name : std::format(L"{:5}", entry.score);
}

bool ChampionTable::qualifies(int score) const {
    return entries_.empty() || score >= entries_.back().score;
}

void ChampionTable::insert(int score, std::wstring name) {
    if (!qualifies(score)) return;
    if (name.empty()) name = L"?";
    if (name.size() > 15) name.resize(15);
    entries_.push_back({score, std::move(name)});
    std::stable_sort(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
        return left.score > right.score;
    });
    if (entries_.size() > 7) entries_.resize(7);
}

void ChampionTable::save() const {
    if (save_path_.empty()) return;
    std::filesystem::create_directories(save_path_.parent_path());
    const std::filesystem::path temporary = save_path_.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not write tournament champions");
        output << "GEARHEADS-NATIVE-CHAMPIONS-1\n";
        for (const Champion& entry : entries_) {
            output << entry.score << '\t' << wide_to_utf8(entry.name) << '\n';
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, save_path_, error);
    if (error) {
        std::filesystem::remove(save_path_, error);
        error.clear();
        std::filesystem::rename(temporary, save_path_, error);
    }
    if (error) throw std::runtime_error("Could not replace tournament champion save");
}

}  // namespace gh
