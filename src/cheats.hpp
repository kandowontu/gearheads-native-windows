#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace gh {

enum class Cheat {
    NeverLose,
    InfiniteWinding,
    InstantLaunch,
    AllToys,
    PowerupParty,
};

struct CheatSettings {
    bool never_lose = false;
    bool infinite_winding = false;
    bool instant_launch = false;
    bool all_toys = false;
    bool powerup_party = false;

    bool enabled(Cheat cheat) const;
    void toggle(Cheat cheat);
    void reset();
};

std::optional<Cheat> cheat_from_action(std::wstring_view action);
bool cheat_menu_chord(
    bool on_main_menu,
    bool f1_pressed,
    bool control_held,
    bool alt_held
);
bool human_cheat_applies(bool enabled, bool computer_controlled);
bool cheat_blocks_match_point(
    bool never_lose,
    int scoring_player,
    int candidate_winner,
    const std::array<bool, 2>& computer_controlled
);
int cheat_launch_winding(
    bool instant_launch,
    bool computer_controlled,
    int gauge_winding,
    int gauge_time
);
int cheat_powerup_seconds(bool powerup_party, int normal_seconds, int party_seconds);

}  // namespace gh
