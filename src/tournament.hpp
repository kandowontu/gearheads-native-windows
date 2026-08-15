#pragma once

#include <optional>

namespace gh {

struct TournamentStart {
    int level = 1;
    int multiplier = 1;
};

struct TournamentState {
    int current_level = 1;
    int lives = 3;
    int total_score = 0;
    int multiplier = 1;
    int unlocked_level = 1;
};

struct TournamentResult {
    bool human_won = false;
    bool bonus_level = false;
    bool game_over = false;
    int points_awarded = 0;
};

std::optional<TournamentStart> decode_original_tournament_start(int encoded);
bool original_tournament_bonus_level(int level);
int original_tournament_points(
    int human_score,
    int computer_score,
    int multiplier
);
TournamentResult apply_original_tournament_result(
    TournamentState& state,
    bool human_won,
    int human_score,
    int computer_score
);

}  // namespace gh
