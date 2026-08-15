#include "levels.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected gearhead.ini path");
        const gh::LevelDatabase database{std::filesystem::path(argv[1])};
        require(database.levels().size() == 105, "expected all 105 original level sections");
        for (wchar_t theme : {L'K', L'G', L'P', L'F', L'B'}) {
            const gh::LevelDefinition* level = database.first_for_theme(theme);
            require(level != nullptr, "a shipped level theme is missing");
            require(!level->background.empty(), "level background conversion failed");
            require(!level->music.empty(), "level music conversion failed");
            require(level->stage.right > level->stage.left, "invalid stage width");
            require(level->stage.bottom > level->stage.top, "invalid stage height");
            require(!level->gauge_archive.empty(), "level gauge archive is missing");
            require(level->right_gauge.right > level->right_gauge.left,
                    "right gauge rectangle is missing");
            if (theme != L'B') {
                require(level->left_toybox.right > level->left_toybox.left,
                        "left toybox rectangle is missing");
                require(level->right_toybox.right > level->right_toybox.left,
                        "right toybox rectangle is missing");
            }
        }
        for (int number = 1; number <= 50; ++number) {
            const gh::LevelDefinition* level = database.find_tournament(number);
            require(level != nullptr, "a numbered tournament level is missing");
            require(level->tournament_intelligence >= 1, "tournament intelligence is missing");
            require(level->tournament_intelligence <= 9, "invalid tournament intelligence");
        }
        require(database.find_tournament(0) == nullptr, "level zero must not be a tournament level");
        require(database.find_tournament(51) == nullptr, "level 51 must not exist");
        std::size_t obstacle_count = 0;
        for (const auto& level : database.levels()) obstacle_count += level.obstacles.size();
        require(obstacle_count > 100, "obstacle records were not recovered");
        std::cout << "validated 105 levels, 50 tournament lookups, and " << obstacle_count
                  << " obstacle records\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
