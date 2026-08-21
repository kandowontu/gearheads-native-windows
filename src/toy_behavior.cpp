#include "toy_behavior.hpp"

#include <algorithm>

namespace gh {

int original_animation_base(int state) {
    if (state < 0) return kAnimationWalk;
    return (state / 3) * 3;
}

std::wstring original_animation_key(int state, bool rocket) {
    const int base = original_animation_base(state);
    wchar_t key = L'w';
    switch (base) {
        case kAnimationWindDown: key = L'e'; break;
        case kAnimationDeath: key = L'd'; break;
        case kAnimationZap: key = L'z'; break;
        case kAnimationFlip: key = L'f'; break;
        case kAnimationAction: key = L'x'; break;
        case kAnimationAlternate: key = L'y'; break;
        default: break;
    }
    std::wstring result(1, key);
    const int direction = state - base;
    if (direction == 1) result.push_back(L'd');
    if (direction == 2) result.push_back(L'u');
    if (rocket && direction == 0) result.push_back(L'h');
    return result;
}

int original_directional_animation_state(
    int base_state,
    int heading,
    bool has_down_state,
    bool has_up_state
) {
    const int base = original_animation_base(base_state);
    const int normalized_heading = heading & 63;
    if (normalized_heading > 4 && normalized_heading < 28 && has_down_state) {
        return base + 1;
    }
    if (normalized_heading > 36 && normalized_heading < 60 && has_up_state) {
        return base + 2;
    }
    return base;
}

bool original_animation_is_transient(int state) {
    const int base = original_animation_base(state);
    return base == kAnimationZap || base == kAnimationFlip ||
           base == kAnimationAction;
}

bool original_kanga_can_punch(
    bool target_is_kanga,
    int source_winding,
    int target_winding
) {
    return !target_is_kanga || source_winding >= target_winding;
}

bool original_small_fry_should_hatch(int collision_layer, int animation_state) {
    return collision_layer != 0 &&
           original_animation_base(animation_state) != kAnimationAction;
}

int original_rocket_warning_winding(int winding, int decay_time, int frame_step) {
    const int warning_value = 2 * frame_step;
    return winding < decay_time && winding > warning_value ? warning_value : winding;
}

int original_orbit_contact_impulse(
    int current_impulse_y16,
    int behavior_state,
    int winding,
    int decay_time
) {
    if (current_impulse_y16 != 0 || winding <= decay_time) return current_impulse_y16;
    return behavior_state * 16;
}

int original_orbit_choose_behavior(
    int y,
    int stage_top,
    int stage_bottom,
    int margin,
    int random_bit
) {
    if (y < stage_top + margin) return 10;
    if (y > stage_bottom - margin) return -10;
    return (random_bit & 1) == 0 ? 10 : -10;
}

PrestoStep original_presto_step(
    int behavior_state,
    int action_duration,
    int primary_extra,
    int random_wait
) {
    PrestoStep result;
    if (behavior_state > 0) --behavior_state;
    result.begin_action = behavior_state == std::max(1, action_duration);
    result.jump = behavior_state == 0;
    if (result.jump) {
        behavior_state = std::max(1, primary_extra / 8 + random_wait);
    }
    result.behavior_state = behavior_state;
    return result;
}

KrushStep original_krush_step(
    int behavior_state,
    int action_duration,
    int primary_extra
) {
    KrushStep result;
    if (behavior_state <= 0) {
        result.behavior_state = std::max(1, primary_extra);
        return result;
    }
    result.begin_action = behavior_state == std::max(1, action_duration);
    result.play_roar = behavior_state == 2;
    --behavior_state;
    result.reverse_targets = behavior_state == 0;
    result.behavior_state = behavior_state;
    return result;
}

}  // namespace gh
