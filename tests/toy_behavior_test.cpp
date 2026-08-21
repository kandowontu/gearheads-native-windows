#include "toy_behavior.hpp"

#include <cassert>

int main() {
    using namespace gh;

    assert(original_animation_key(0) == L"w");
    assert(original_animation_key(1) == L"wd");
    assert(original_animation_key(2) == L"wu");
    assert(original_animation_key(9) == L"z");
    assert(original_animation_key(12) == L"f");
    assert(original_animation_key(15) == L"x");
    assert(original_animation_key(18) == L"y");
    assert(original_animation_key(0, true) == L"wh");
    assert(original_directional_animation_state(15, 8, true, true) == 16);
    assert(original_directional_animation_state(15, 40, true, true) == 17);
    assert(original_directional_animation_state(15, 40, true, false) == 15);
    assert(original_directional_animation_state(15, 32, true, true) == 15);
    assert(original_animation_is_transient(10));
    assert(!original_animation_is_transient(3));
    assert(original_kanga_can_punch(true, 500, 500));
    assert(!original_kanga_can_punch(true, 499, 500));
    assert(original_kanga_can_punch(false, 1, 900));
    assert(!original_small_fry_should_hatch(1, 15));
    assert(original_small_fry_should_hatch(1, 0));
    assert(!original_small_fry_should_hatch(0, 0));
    assert(original_rocket_warning_winding(1199, 1200, 55) == 110);
    assert(original_rocket_warning_winding(110, 1200, 55) == 110);
    assert(original_rocket_warning_winding(1200, 1200, 55) == 1200);
    assert(original_orbit_contact_impulse(0, -10, 1201, 1200) == -160);
    assert(original_orbit_contact_impulse(0, 10, 1200, 1200) == 0);
    assert(original_orbit_contact_impulse(32, -10, 1201, 1200) == 32);
    assert(original_orbit_choose_behavior(20, 0, 400, 40, 1) == 10);
    assert(original_orbit_choose_behavior(380, 0, 400, 40, 0) == -10);
    assert(original_orbit_choose_behavior(200, 0, 400, 40, 0) == 10);
    assert(original_orbit_choose_behavior(200, 0, 400, 40, 1) == -10);

    const PrestoStep presto_attack = original_presto_step(9, 8, 50, 0);
    assert(presto_attack.behavior_state == 8);
    assert(presto_attack.begin_action);
    assert(!presto_attack.jump);
    const PrestoStep presto_jump = original_presto_step(1, 8, 50, 49);
    assert(presto_jump.jump);
    assert(presto_jump.behavior_state == 55);

    const KrushStep krush_armed = original_krush_step(0, 8, 40);
    assert(krush_armed.behavior_state == 40);
    const KrushStep krush_action = original_krush_step(8, 8, 40);
    assert(krush_action.begin_action && krush_action.behavior_state == 7);
    const KrushStep krush_sound = original_krush_step(2, 8, 40);
    assert(krush_sound.play_roar && !krush_sound.reverse_targets);
    const KrushStep krush_effect = original_krush_step(1, 8, 40);
    assert(!krush_effect.play_roar && krush_effect.reverse_targets);
}
