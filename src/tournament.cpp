#include "tournament.hpp"

#include <algorithm>

namespace gh {

std::optional<TournamentStart> decode_original_tournament_start(int encoded) {
    if (encoded < 100) return std::nullopt;
    const int level = encoded % 100;
    const int multiplier = encoded / 100;
    if (level < 1 || level > 50 || multiplier < 1) return std::nullopt;
    return TournamentStart{level, multiplier};
}

bool original_tournament_bonus_level(int level) {
    // segment 7:1cc2-1cda: divisible by three and below level 37.
    return level > 0 && level < 37 && level % 3 == 0;
}

int original_tournament_points(int human_score, int computer_score, int multiplier) {
    const int difference = std::max(0, human_score - computer_score);
    const int perfect_multiplier = computer_score == 0 ? 2 : 1;
    return difference * std::max(1, multiplier) * perfect_multiplier;
}

TournamentResult apply_original_tournament_result(
    TournamentState& state,
    bool human_won,
    int human_score,
    int computer_score
) {
    TournamentResult result;
    result.human_won = human_won;
    result.bonus_level = original_tournament_bonus_level(state.current_level);
    if (human_won) {
        result.points_awarded = original_tournament_points(
            human_score, computer_score, state.multiplier
        );
        state.total_score += result.points_awarded;
        if (result.bonus_level) state.lives = std::min(9, state.lives + 1);
        // segment 15:012b-0167 advances normally, but level 50 wraps to 45.
        state.current_level = state.current_level >= 50 ? 45 : state.current_level + 1;
        state.unlocked_level = std::max(state.unlocked_level, state.current_level);
    } else {
        state.lives = std::max(0, state.lives - 1);
        result.game_over = state.lives == 0;
    }
    return result;
}

}  // namespace gh
