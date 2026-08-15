#include "toy_effects.hpp"

#include <algorithm>

namespace gh {

int original_bomby_blast_winding(
    int bomb_x16,
    int bomb_y16,
    int radius_pixels,
    const BlastTarget& target
) {
    if (!target.physical || target.winding <= 0 ||
        target.original_type == kOriginalDisasteroidType) {
        return target.winding;
    }
    const std::int64_t dx = static_cast<std::int64_t>(target.x16) - bomb_x16;
    const std::int64_t dy = static_cast<std::int64_t>(target.y16) - bomb_y16;
    const std::int64_t radius16 = static_cast<std::int64_t>(radius_pixels) * 16;
    if (dx * dx + dy * dy >= radius16 * radius16) return target.winding;
    if (target.original_type == kOriginalBombyType) return -100;
    return std::min(target.winding, 1);
}

bool original_target_is_in_front(int owner, int source_x16, int target_x16) {
    if (target_x16 < source_x16) return owner != 0;
    return owner == 0;
}

int original_zappa_drain(int target_winding, int energy_drain) {
    if (target_winding <= 0 || energy_drain <= 0) return target_winding;
    if (target_winding > energy_drain) return target_winding - energy_drain;
    return std::min(target_winding, 1);
}

int original_kanga_impulse_x16(
    int owner,
    int punch_distance_pixels,
    int puncher_mass,
    int target_mass
) {
    if (target_mass <= 0) return 0;
    const std::int64_t numerator =
        static_cast<std::int64_t>(punch_distance_pixels) * 16 * puncher_mass;
    const int magnitude = static_cast<int>((numerator + target_mass / 2) / target_mass);
    return owner == 0 ? magnitude : -magnitude;
}

int take_original_horizontal_impulse_step(int& remaining_x16) {
    const int step = std::clamp(remaining_x16, -1024, 1024);
    remaining_x16 -= step;
    return step;
}

bool inside_original_radius(
    int source_x16,
    int source_y16,
    int target_x16,
    int target_y16,
    int radius_pixels,
    bool inclusive
) {
    const std::int64_t dx = static_cast<std::int64_t>(target_x16) - source_x16;
    const std::int64_t dy = static_cast<std::int64_t>(target_y16) - source_y16;
    const std::int64_t radius16 = static_cast<std::int64_t>(radius_pixels) * 16;
    const std::int64_t distance_squared = dx * dx + dy * dy;
    const std::int64_t radius_squared = radius16 * radius16;
    return inclusive ? distance_squared <= radius_squared : distance_squared < radius_squared;
}

}  // namespace gh
