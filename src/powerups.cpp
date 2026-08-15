#include "powerups.hpp"

#include <algorithm>

namespace gh {

int original_powerup_random_bound(int mean_seconds, int frame_step_ms) {
    if (mean_seconds <= 0 || frame_step_ms <= 0) return 0;
    const long long numerator = static_cast<long long>(mean_seconds) * 1000;
    return std::max(
        1,
        static_cast<int>((numerator + frame_step_ms / 2) / frame_step_ms)
    );
}

int original_powerup_effect_recipient(PowerupKind kind, int key_owner) {
    const int owner = key_owner & 1;
    return kind == PowerupKind::OpponentLockout ? owner : 1 - owner;
}

bool original_powerup_blocks_release(PowerupKind kind) {
    return kind == PowerupKind::OpponentLockout;
}

bool original_powerup_starts_at_center(PowerupKind kind) {
    return kind == PowerupKind::CenterRelease;
}

std::vector<int> original_powerup_extra_lanes(
    PowerupKind kind, int selected_lane, int lane_count
) {
    std::vector<int> result;
    if (lane_count <= 0) return result;
    selected_lane = std::clamp(selected_lane, 0, lane_count - 1);
    if (kind == PowerupKind::FiveLaneRelease) {
        result.reserve(static_cast<std::size_t>(std::max(0, lane_count - 1)));
        for (int lane = 0; lane < lane_count; ++lane) {
            if (lane != selected_lane) result.push_back(lane);
        }
    } else if (kind == PowerupKind::AdjacentRelease && lane_count > 1) {
        result.push_back(selected_lane == 0 ? 1 : selected_lane - 1);
    }
    return result;
}

}  // namespace gh
