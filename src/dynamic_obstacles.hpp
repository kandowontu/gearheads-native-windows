#pragma once

namespace gh {

struct BugSteerImpulse {
    int x16 = 0;
    int y16 = 0;
};

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
);

int original_bug_transferred_impulse(int bug_velocity16, int bug_mass, int target_mass);
int original_bug_slow_velocity(int velocity16);

}  // namespace gh
