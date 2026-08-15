#include "tournament.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() {
    try {
        const auto level1 = gh::decode_original_tournament_start(101);
        const auto level13 = gh::decode_original_tournament_start(413);
        const auto level25 = gh::decode_original_tournament_start(1025);
        require(level1 && level1->level == 1 && level1->multiplier == 1, "bad level 1 start");
        require(level13 && level13->level == 13 && level13->multiplier == 4, "bad level 13 start");
        require(level25 && level25->level == 25 && level25->multiplier == 10, "bad level 25 start");
        require(!gh::decode_original_tournament_start(99), "short start code should fail");

        require(gh::original_tournament_bonus_level(3), "level 3 should be a bonus");
        require(gh::original_tournament_bonus_level(36), "level 36 should be a bonus");
        require(!gh::original_tournament_bonus_level(37), "level 37 should not be a bonus");
        require(!gh::original_tournament_bonus_level(4), "level 4 should not be a bonus");
        require(gh::original_tournament_points(21, 12, 4) == 36, "score delta changed");
        require(gh::original_tournament_points(21, 0, 10) == 420, "perfect bonus changed");

        gh::TournamentState state{3, 8, 10, 4, 3};
        const auto bonus = gh::apply_original_tournament_result(state, true, 21, 10);
        require(bonus.bonus_level && bonus.points_awarded == 44, "bonus result changed");
        require(state.current_level == 4 && state.lives == 9 && state.total_score == 54,
                "bonus state changed");

        state = {50, 1, 0, 1, 50};
        gh::apply_original_tournament_result(state, true, 21, 20);
        require(state.current_level == 45, "level 50 should wrap to 45");
        require(state.unlocked_level == 50, "wrap must not lower the unlocked level");

        gh::TournamentState loss{13, 1, 0, 4, 13};
        const auto game_over = gh::apply_original_tournament_result(loss, false, 18, 21);
        require(game_over.game_over && loss.lives == 0 && loss.current_level == 13,
                "final loss behavior changed");
        std::cout << "validated recovered tournament starts, bonuses, scoring, lives, and wrap\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
