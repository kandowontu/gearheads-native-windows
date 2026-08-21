#pragma once

#include <string>

namespace gh {

constexpr int kAnimationWalk = 0;
constexpr int kAnimationWindDown = 3;
constexpr int kAnimationDeath = 6;
constexpr int kAnimationZap = 9;
constexpr int kAnimationFlip = 12;
constexpr int kAnimationAction = 15;
constexpr int kAnimationAlternate = 18;

int original_animation_base(int state);
std::wstring original_animation_key(int state, bool rocket = false);
int original_directional_animation_state(
    int base_state,
    int heading,
    bool has_down_state,
    bool has_up_state
);
bool original_animation_is_transient(int state);
bool original_kanga_can_punch(
    bool target_is_kanga,
    int source_winding,
    int target_winding
);
bool original_small_fry_should_hatch(int collision_layer, int animation_state);
int original_rocket_warning_winding(int winding, int decay_time, int frame_step);
int original_orbit_contact_impulse(
    int current_impulse_y16,
    int behavior_state,
    int winding,
    int decay_time
);
int original_orbit_choose_behavior(
    int y,
    int stage_top,
    int stage_bottom,
    int margin,
    int random_bit
);

struct PrestoStep {
    int behavior_state = 0;
    bool begin_action = false;
    bool jump = false;
};

PrestoStep original_presto_step(
    int behavior_state,
    int action_duration,
    int primary_extra,
    int random_wait
);

struct KrushStep {
    int behavior_state = 0;
    bool begin_action = false;
    bool play_roar = false;
    bool reverse_targets = false;
};

KrushStep original_krush_step(
    int behavior_state,
    int action_duration,
    int primary_extra
);

}  // namespace gh
