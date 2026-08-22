#include "hud_layout.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool valid(const gh::Rectangle& rectangle) {
    return rectangle.right > rectangle.left && rectangle.bottom > rectangle.top;
}

bool overlaps(const gh::Rectangle& left, const gh::Rectangle& right) {
    if (!valid(left) || !valid(right)) return false;
    return left.left < right.right && left.right > right.left &&
           left.top < right.bottom && left.bottom > right.top;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected gearhead.ini path");

        require(gh::needs_fallback_toy_preview({0, 0, 0, 0}, 12),
                "a hidden multi-toy roster needs a fallback preview");
        require(!gh::needs_fallback_toy_preview({0, 0, 0, 0}, 1),
                "a forced single-toy bonus should preserve the original hidden preview");
        require(!gh::needs_fallback_toy_preview({10, 10, 20, 20}, 12),
                "a configured toy preview must not get a fallback");

        const gh::LevelDatabase database{std::filesystem::path(argv[1])};
        int bonus_levels = 0;
        for (const gh::LevelDefinition& level : database.levels()) {
            if (level.theme != L'B') continue;
            ++bonus_levels;
            const gh::Rectangle left = gh::fallback_toy_preview_rectangle(level, 0);
            const gh::Rectangle right = gh::fallback_toy_preview_rectangle(level, 1);
            require(valid(left) && valid(right), "fallback preview rectangle is invalid");
            require(left.left >= 0 && left.right <= 640 && left.top >= 0 && left.bottom <= 480,
                    "left fallback preview is outside the canvas");
            require(right.left >= 0 && right.right <= 640 && right.top >= 0 && right.bottom <= 480,
                    "right fallback preview is outside the canvas");
            require(!overlaps(left, level.right_gauge),
                    "left fallback preview overlaps the bonus gauge");
            require(!overlaps(right, level.right_gauge),
                    "right fallback preview overlaps the bonus gauge");
            require(!overlaps(left, right), "fallback previews overlap each other");
        }
        require(bonus_levels == 12, "expected all twelve original bonus levels");
        std::cout << "validated fallback toy previews for " << bonus_levels
                  << " bonus levels\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
