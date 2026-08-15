#include "dynamic_obstacles.hpp"

#include <cstdlib>

namespace gh {
namespace {

int rounded_muldiv(int value, int multiplier, int divisor) {
    if (divisor == 0) return 0;
    const long long product = static_cast<long long>(value) * multiplier;
    const long long half = std::abs(divisor) / 2;
    if (product >= 0) return static_cast<int>((product + half) / divisor);
    return -static_cast<int>((-product + half) / divisor);
}

}  // namespace

BugSteerImpulse original_bug_separation_impulse(
    int bug_x16,
    int bug_y16,
    int target_x16,
    int target_y16,
    int stage_left16,
    int stage_top16,
    int stage_right16,
    int stage_bottom16,
    int random_x16,
    int random_y16
) {
    BugSteerImpulse result{
        (bug_x16 - target_x16) / 2,
        (bug_y16 - target_y16) / 2,
    };
    if (std::abs(result.x16) > 0x200) result.x16 = 0;
    if (std::abs(result.y16) > 0x200) result.y16 = 0;
    if (result.x16 == 0 && result.y16 == 0) {
        result.x16 = random_x16;
        result.y16 = random_y16;
    }
    if (bug_x16 + result.x16 < stage_left16) result.x16 = stage_left16 - bug_x16;
    if (bug_x16 + result.x16 > stage_right16) result.x16 = stage_right16 - bug_x16;
    if (bug_y16 + result.y16 < stage_top16) result.y16 = stage_top16 - bug_y16;
    if (bug_y16 + result.y16 > stage_bottom16) result.y16 = stage_bottom16 - bug_y16;
    return result;
}

int original_bug_transferred_impulse(int bug_velocity16, int bug_mass, int target_mass) {
    return rounded_muldiv(bug_velocity16, bug_mass, target_mass);
}

int original_bug_slow_velocity(int velocity16) {
    if (velocity16 == 0) return 0;
    return velocity16 > 0 ? 32 : -32;
}

}  // namespace gh
