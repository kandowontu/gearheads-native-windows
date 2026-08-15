#include "computer_ai.hpp"

#include <algorithm>

namespace gh {
namespace {

std::size_t clock_index(ComputerTier tier) {
    switch (tier) {
        case ComputerTier::Low: return 0;
        case ComputerTier::Medium: return 1;
        case ComputerTier::High: return 2;
        case ComputerTier::None: return 0;
    }
    return 0;
}

}  // namespace

ComputerTier original_computer_tier(int difficulty, int roll_tenth) {
    const int roll = std::clamp(roll_tenth, 0, 9);
    switch (difficulty) {
        case 1: return ComputerTier::Low;
        case 2: return roll <= 0 ? ComputerTier::Medium : ComputerTier::Low;
        case 3: return roll <= 1 ? ComputerTier::Medium : ComputerTier::Low;
        case 4: return roll <= 2 ? ComputerTier::Medium : ComputerTier::Low;
        case 5: return ComputerTier::Medium;
        case 6: return roll <= 2 ? ComputerTier::High : ComputerTier::Medium;
        case 7: return roll <= 5 ? ComputerTier::High : ComputerTier::Medium;
        case 8: return roll <= 8 ? ComputerTier::High : ComputerTier::Medium;
        case 9: return ComputerTier::High;
        default: return ComputerTier::None;
    }
}

std::uint32_t original_ai_tick60(std::uint32_t milliseconds) {
    return static_cast<std::uint32_t>(milliseconds * 6U) / 100U;
}

bool original_ai_cursor_step_due(std::uint32_t now_tick, std::uint32_t last_tick) {
    return static_cast<std::uint32_t>(now_tick - last_tick) > 40U;
}

int original_low_ai_launch_threshold(bool alternate_rules) {
    // Segment 6:2198 starts at 0x12c0 and adds 0x01f4 when DS:676c is set.
    return 4800 + (alternate_rules ? 500 : 0);
}

int original_next_available_toy(
    int current_definition,
    int step,
    std::span<const bool> available
) {
    if (available.empty()) return current_definition;
    const int count = static_cast<int>(available.size());
    int candidate = std::clamp(current_definition, 0, count - 1);
    const int increment = std::clamp(step, -1, 1);
    for (int attempts = 0; attempts < 100; ++attempts) {
        candidate += increment;
        if (candidate < 0) candidate = count - 1;
        if (candidate >= count) candidate = 0;
        if (available[static_cast<std::size_t>(candidate)]) return candidate;
    }
    return current_definition;
}

void ComputerAiCadence::reset() {
    last_cursor_tick_.fill(0U);
}

ComputerCadenceDecision ComputerAiCadence::update(
    int difficulty,
    int roll_tenth,
    int gauge_ms,
    int decay_time_ms,
    std::uint32_t elapsed_ms,
    bool alternate_rules
) {
    ComputerCadenceDecision result;
    result.tier = original_computer_tier(difficulty, roll_tenth);
    if (result.tier == ComputerTier::None) return result;

    const std::uint32_t now_tick = original_ai_tick60(elapsed_ms);
    std::uint32_t& last_tick = last_cursor_tick_[clock_index(result.tier)];

    if (result.tier == ComputerTier::Low) {
        if (gauge_ms > original_low_ai_launch_threshold(alternate_rules)) {
            result.attempt_launch = true;
            result.cycle_after_launch = true;
            return result;
        }
    } else if (gauge_ms < decay_time_ms) {
        return result;
    } else if (gauge_ms > decay_time_ms) {
        // Medium/high still enter their analysis at equality, but the common
        // release routine performs the same strict > DecayTime check as input.
        result.attempt_launch = true;
    }

    if (original_ai_cursor_step_due(now_tick, last_tick)) {
        result.cursor_step_due = true;
        last_tick = now_tick;
    }
    return result;
}

}  // namespace gh
