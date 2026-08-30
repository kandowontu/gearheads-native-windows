#include "match_rules.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() {
    try {
        using gh::kNoMatchWinner;
        using gh::original_match_winner;
        using gh::original_sudden_death_removes_obstacle;
        using gh::original_sudden_death_scores;

        require(original_match_winner(20, 0, 21) == kNoMatchWinner, "threshold ignored");
        require(original_match_winner(21, 19, 21) == 0, "left 21-19 should win");
        require(original_match_winner(19, 21, 21) == 1, "right 21-19 should win");
        require(original_match_winner(21, 20, 21) == kNoMatchWinner, "21-20 is not a win");
        require(original_match_winner(26, 25, 21) == kNoMatchWinner, "one-point overtime lead won");
        require(original_match_winner(27, 25, 21) == 0, "two-point overtime lead did not win");
        require(original_match_winner(25, 27, 21) == 1, "right overtime win failed");
        require(
            original_sudden_death_scores({0, 7}, 21) == std::array<int, 2>{19, 19},
            "sudden death did not raise both low scores"
        );
        require(
            original_sudden_death_scores({23, 18}, 21) == std::array<int, 2>{23, 19},
            "sudden death changed an overtime score"
        );
        for (const int code : {51, 52, 53, 54, 55, 58, 59, 73, 74, 75, 76, 77, 78}) {
            require(original_sudden_death_removes_obstacle(code), "cleanup type was retained");
        }
        require(!original_sudden_death_removes_obstacle(56), "mud should survive cleanup");
        require(!original_sudden_death_removes_obstacle(57), "oil should survive cleanup");

        std::cout << "validated recovered winning-score and win-by-two rules\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
