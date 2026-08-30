#pragma once

#include <array>

namespace gh {

constexpr int kNoMatchWinner = -1;

// GEAR_EN segment 12:1011-1078. Scores are signed words in the original,
// compared against Winningscore, and a lead of at least two is required.
int original_match_winner(int left_score, int right_score, int winning_score);

// The original scoreless-match deadline raises either score below
// Winningscore-2 to that value, then disables the deadline permanently.
std::array<int, 2> original_sudden_death_scores(
    std::array<int, 2> scores,
    int winning_score
);

// Original type list at DS:15cc, translated to public GEARHEAD.INI obstacle
// codes. Mud and oil deliberately survive the sudden-death cleanup.
bool original_sudden_death_removes_obstacle(int code);

}  // namespace gh
