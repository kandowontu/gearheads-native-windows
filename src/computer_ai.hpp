#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace gh {

enum class ComputerTier {
    None,
    Low,
    Medium,
    High,
};

// GEAR_EN segment 6:6994 dispatches the nine visible difficulty settings by
// blending three concrete routines with a rand() % 10 roll.
ComputerTier original_computer_tier(int difficulty, int roll_tenth);

// The original computer routines convert timeGetTime() to 60 Hz ticks with
// (milliseconds * 6) / 100 and require a strict gap of more than 40 ticks.
std::uint32_t original_ai_tick60(std::uint32_t milliseconds);
bool original_ai_cursor_step_due(std::uint32_t now_tick, std::uint32_t last_tick);

int original_low_ai_launch_threshold(bool alternate_rules);

// The toy-selection helper walks numeric original types 15..26 and skips
// types not present in the player's box. Native definitions use indices 0..11.
int original_next_available_toy(
    int current_definition,
    int step,
    std::span<const bool> available
);

struct ComputerCadenceDecision {
    ComputerTier tier = ComputerTier::None;
    bool attempt_launch = false;
    bool cursor_step_due = false;
    bool cycle_after_launch = false;
};

class ComputerAiCadence {
public:
    void reset();
    ComputerCadenceDecision update(
        int difficulty,
        int roll_tenth,
        int gauge_ms,
        int decay_time_ms,
        std::uint32_t elapsed_ms,
        bool alternate_rules = false
    );

private:
    std::array<std::uint32_t, 3> last_cursor_tick_{};
};

}  // namespace gh
