#include "cheats.hpp"

#include <cassert>

int main() {
    using namespace gh;

    CheatSettings settings;
    assert(!settings.enabled(Cheat::NeverLose));
    settings.toggle(Cheat::NeverLose);
    settings.toggle(Cheat::InfiniteWinding);
    assert(settings.never_lose && settings.infinite_winding);
    settings.toggle(Cheat::NeverLose);
    assert(!settings.never_lose && settings.infinite_winding);
    settings.reset();
    assert(!settings.never_lose && !settings.infinite_winding &&
           !settings.instant_launch && !settings.all_toys &&
           !settings.powerup_party);

    assert(cheat_from_action(L">cheat$alltoys") == Cheat::AllToys);
    assert(!cheat_from_action(L">cheat$reset").has_value());
    assert(cheat_menu_chord(true, true, true, true));
    assert(!cheat_menu_chord(false, true, true, true));
    assert(!cheat_menu_chord(true, true, false, true));
    assert(!cheat_menu_chord(true, false, true, true));
    assert(human_cheat_applies(true, false));
    assert(!human_cheat_applies(true, true));
    assert(cheat_blocks_match_point(true, 0, 0, {true, false}));
    assert(!cheat_blocks_match_point(true, 0, 0, {false, false}));
    assert(!cheat_blocks_match_point(true, 0, -1, {true, false}));
    assert(cheat_launch_winding(true, false, 1500, 6000) == 6000);
    assert(cheat_launch_winding(true, true, 1500, 6000) == 1500);
    assert(cheat_powerup_seconds(true, 15, 3) == 3);
    assert(cheat_powerup_seconds(false, 15, 3) == 15);
}
