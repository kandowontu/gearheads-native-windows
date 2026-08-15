#include "computer_ai.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() {
    try {
        using gh::ComputerTier;
        using gh::original_computer_tier;

        const std::array<int, 9> expected_high{0, 0, 0, 0, 0, 3, 6, 9, 10};
        const std::array<int, 9> expected_medium{0, 1, 2, 3, 10, 7, 4, 1, 0};
        for (int difficulty = 1; difficulty <= 9; ++difficulty) {
            int low = 0;
            int medium = 0;
            int high = 0;
            for (int roll = 0; roll < 10; ++roll) {
                switch (original_computer_tier(difficulty, roll)) {
                    case ComputerTier::Low: ++low; break;
                    case ComputerTier::Medium: ++medium; break;
                    case ComputerTier::High: ++high; break;
                    case ComputerTier::None: break;
                }
            }
            require(high == expected_high[static_cast<std::size_t>(difficulty - 1)],
                    "high-tier blend changed");
            require(medium == expected_medium[static_cast<std::size_t>(difficulty - 1)],
                    "medium-tier blend changed");
            require(low + medium + high == 10, "difficulty roll did not dispatch");
        }

        require(gh::original_ai_tick60(666) == 39, "666 ms tick conversion changed");
        require(gh::original_ai_tick60(683) == 40, "683 ms must remain tick 40");
        require(gh::original_ai_tick60(684) == 41, "684 ms must reach tick 41");
        require(!gh::original_ai_cursor_step_due(40, 0), "40 ticks must be blocked");
        require(gh::original_ai_cursor_step_due(41, 0), "41 ticks must be accepted");
        require(gh::original_low_ai_launch_threshold(false) == 4800,
                "normal low threshold changed");
        require(gh::original_low_ai_launch_threshold(true) == 5300,
                "alternate low threshold changed");

        std::array<bool, 12> available{};
        available[0] = true;
        available[4] = true;
        available[11] = true;
        require(gh::original_next_available_toy(0, 1, available) == 4,
                "forward selection did not skip unavailable types");
        require(gh::original_next_available_toy(0, -1, available) == 11,
                "reverse selection did not wrap");
        require(gh::original_next_available_toy(4, 0, available) == 4,
                "zero step must retain the selected type");

        gh::ComputerAiCadence easy;
        auto decision = easy.update(1, 0, 4800, 1200, 0);
        require(!decision.attempt_launch, "easy AI launched at its threshold");
        decision = easy.update(1, 0, 4801, 1200, 0);
        require(decision.attempt_launch && decision.cycle_after_launch,
                "easy AI did not launch and cycle above its threshold");

        gh::ComputerAiCadence medium;
        decision = medium.update(5, 0, 1200, 1200, 683);
        require(!decision.attempt_launch && !decision.cursor_step_due,
                "medium AI accepted equality or stepped at tick 40");
        decision = medium.update(5, 0, 1201, 1200, 684);
        require(decision.attempt_launch && decision.cursor_step_due,
                "medium AI did not launch/step above the recovered boundaries");

        std::cout << "validated original computer difficulty blend, clocks, and easy cadence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
