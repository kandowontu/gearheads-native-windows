#include "match_rules.hpp"

#include <algorithm>

namespace gh {

int original_match_winner(int left_score, int right_score, int winning_score) {
    if (left_score >= winning_score && left_score > right_score + 1) return 0;
    if (right_score >= winning_score && right_score > left_score + 1) return 1;
    return kNoMatchWinner;
}

std::array<int, 2> original_sudden_death_scores(
    std::array<int, 2> scores,
    int winning_score
) {
    const int floor = std::max(0, winning_score - 2);
    scores[0] = std::max(scores[0], floor);
    scores[1] = std::max(scores[1], floor);
    return scores;
}

bool original_sudden_death_removes_obstacle(int code) {
    switch (code) {
        case 51:  // htrd
        case 52:  // utrd
        case 53:  // dtrd
        case 54:  // crak
        case 55:  // tele
        case 58:  // glue
        case 59:  // rock
        case 73:  // buggy
        case 74:  // block
        case 75:  // wall1
        case 76:  // wall2
        case 77:  // wall3
        case 78:  // wall4
            return true;
        default: return false;
    }
}

}  // namespace gh
