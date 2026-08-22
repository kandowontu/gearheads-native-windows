#include "cheats.hpp"

namespace gh {

bool CheatSettings::enabled(Cheat cheat) const {
    switch (cheat) {
        case Cheat::NeverLose: return never_lose;
        case Cheat::InfiniteWinding: return infinite_winding;
        case Cheat::InstantLaunch: return instant_launch;
        case Cheat::AllToys: return all_toys;
        case Cheat::PowerupParty: return powerup_party;
    }
    return false;
}

void CheatSettings::toggle(Cheat cheat) {
    switch (cheat) {
        case Cheat::NeverLose: never_lose = !never_lose; break;
        case Cheat::InfiniteWinding: infinite_winding = !infinite_winding; break;
        case Cheat::InstantLaunch: instant_launch = !instant_launch; break;
        case Cheat::AllToys: all_toys = !all_toys; break;
        case Cheat::PowerupParty: powerup_party = !powerup_party; break;
    }
}

void CheatSettings::reset() {
    *this = {};
}

std::optional<Cheat> cheat_from_action(std::wstring_view action) {
    if (action == L">cheat$neverlose") return Cheat::NeverLose;
    if (action == L">cheat$infinitewinding") return Cheat::InfiniteWinding;
    if (action == L">cheat$instantlaunch") return Cheat::InstantLaunch;
    if (action == L">cheat$alltoys") return Cheat::AllToys;
    if (action == L">cheat$powerupparty") return Cheat::PowerupParty;
    return std::nullopt;
}

bool cheat_menu_chord(
    bool on_main_menu,
    bool f1_pressed,
    bool control_held,
    bool alt_held
) {
    return on_main_menu && f1_pressed && control_held && alt_held;
}

bool human_cheat_applies(bool enabled, bool computer_controlled) {
    return enabled && !computer_controlled;
}

bool cheat_blocks_match_point(
    bool never_lose,
    int scoring_player,
    int candidate_winner,
    const std::array<bool, 2>& computer_controlled
) {
    if (!never_lose || scoring_player < 0 || scoring_player > 1 ||
        candidate_winner != scoring_player) {
        return false;
    }
    const int opponent = 1 - scoring_player;
    return computer_controlled[static_cast<std::size_t>(scoring_player)] &&
           !computer_controlled[static_cast<std::size_t>(opponent)];
}

int cheat_launch_winding(
    bool instant_launch,
    bool computer_controlled,
    int gauge_winding,
    int gauge_time
) {
    return human_cheat_applies(instant_launch, computer_controlled)
               ? gauge_time
               : gauge_winding;
}

int cheat_powerup_seconds(bool powerup_party, int normal_seconds, int party_seconds) {
    return powerup_party ? party_seconds : normal_seconds;
}

}  // namespace gh
