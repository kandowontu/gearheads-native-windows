#include "match_rules.hpp"

namespace gh {

int original_match_winner(int left_score, int right_score, int winning_score) {
    if (left_score >= winning_score && left_score > right_score + 1) return 0;
    if (right_score >= winning_score && right_score > left_score + 1) return 1;
    return kNoMatchWinner;
}

}  // namespace gh
