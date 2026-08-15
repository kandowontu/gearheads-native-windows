#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace gh {

struct Rectangle {
    int left = 42;
    int top = 50;
    int right = 600;
    int bottom = 410;
};

struct ObstacleDefinition {
    int code = 0;
    int ordinal = 0;
    bool facing_right = false;
    int x = 0;
    int y = 0;
};

struct LevelDefinition {
    std::wstring section;
    std::wstring name;
    wchar_t theme = L'X';
    int tournament_number = 0;
    int tournament_intelligence = 0;
    std::filesystem::path background;
    std::wstring gauge_archive;
    std::vector<std::filesystem::path> music;
    Rectangle stage;
    Rectangle left_gauge{0, 0, 0, 0};
    Rectangle right_gauge{0, 0, 0, 0};
    Rectangle left_toybox{0, 0, 0, 0};
    Rectangle right_toybox{0, 0, 0, 0};
    int friction = 5;
    bool ice = false;
    int powerup_probability = 0;
    std::wstring level_toys;
    std::vector<ObstacleDefinition> obstacles;
};

class LevelDatabase {
public:
    explicit LevelDatabase(const std::filesystem::path& path);

    const LevelDefinition* first_for_theme(wchar_t theme) const;
    const LevelDefinition* find_tournament(int number) const;
    const std::vector<LevelDefinition>& levels() const { return levels_; }

private:
    std::vector<LevelDefinition> levels_;
};

}  // namespace gh
