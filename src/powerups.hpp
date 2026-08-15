#pragma once

#include <vector>

namespace gh {

enum class PowerupKind {
    FiveLaneRelease = 1,
    CenterRelease = 2,
    AdjacentRelease = 3,
    OpponentLockout = 4,
    Rocket = 5,
};

// GEAR_EN segment 10:1610-162c computes MulDiv(PuupProb, 1000,
// FrameStep), then spawns when Random(bound) is zero.
int original_powerup_random_bound(int mean_seconds, int frame_step_ms);

// The lockout flips the key's owner before the common recipient calculation,
// so it affects the side that the key came from. All other outcomes benefit
// the toy which collected the opponent's key.
int original_powerup_effect_recipient(PowerupKind kind, int key_owner);

bool original_powerup_blocks_release(PowerupKind kind);
bool original_powerup_starts_at_center(PowerupKind kind);

// Returns the additional board rows released by outcomes 1 and 3. The normal
// selected row is always released separately.
std::vector<int> original_powerup_extra_lanes(
    PowerupKind kind, int selected_lane, int lane_count
);

}  // namespace gh
