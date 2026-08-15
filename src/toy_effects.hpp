#pragma once

#include <cstdint>

namespace gh {

inline constexpr int kOriginalBombyType = 16;
inline constexpr int kOriginalDisasteroidType = 21;

struct BlastTarget {
    int original_type = 0;
    int x16 = 0;
    int y16 = 0;
    int winding = 0;
    bool physical = true;
};

// Returns the winding value produced by segment 10:1f21-2028. The comparison
// is strict: an object's center exactly on the radius is not affected.
int original_bomby_blast_winding(
    int bomb_x16,
    int bomb_y16,
    int radius_pixels,
    const BlastTarget& target
);

bool original_target_is_in_front(int owner, int source_x16, int target_x16);
int original_zappa_drain(int target_winding, int energy_drain);
int original_kanga_impulse_x16(
    int owner,
    int punch_distance_pixels,
    int puncher_mass,
    int target_mass
);
int take_original_horizontal_impulse_step(int& remaining_x16);
bool inside_original_radius(
    int source_x16,
    int source_y16,
    int target_x16,
    int target_y16,
    int radius_pixels,
    bool inclusive
);

}  // namespace gh
