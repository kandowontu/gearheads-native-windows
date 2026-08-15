#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace gh {

struct Champion {
    int score = 0;
    std::wstring name;
};

class ChampionTable {
public:
    explicit ChampionTable(
        const std::filesystem::path& path,
        std::filesystem::path save_path = {}
    );

    const std::vector<Champion>& entries() const { return entries_; }
    std::wstring placeholder(const std::wstring& token) const;
    bool qualifies(int score) const;
    void insert(int score, std::wstring name);
    void save() const;

private:
    std::filesystem::path save_path_;
    std::vector<Champion> entries_;
};

}  // namespace gh
