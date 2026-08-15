#pragma once

namespace gh {

constexpr int kNoMatchWinner = -1;

// GEAR_EN segment 12:1011-1078. Scores are signed words in the original,
// compared against Winningscore, and a lead of at least two is required.
int original_match_winner(int left_score, int right_score, int winning_score);

}  // namespace gh
