#include "powerups.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    using gh::PowerupKind;

    require(gh::original_powerup_random_bound(15, 55) == 273, "15-second bound");
    require(gh::original_powerup_random_bound(6, 55) == 109, "six-second bound");
    require(gh::original_powerup_random_bound(0, 55) == 0, "disabled powerups");

    require(
        gh::original_powerup_effect_recipient(PowerupKind::CenterRelease, 0) == 1,
        "beneficial pickup recipient"
    );
    require(
        gh::original_powerup_effect_recipient(PowerupKind::OpponentLockout, 0) == 0,
        "lockout recipient"
    );
    require(
        gh::original_powerup_blocks_release(PowerupKind::OpponentLockout),
        "lockout behavior"
    );
    require(
        gh::original_powerup_starts_at_center(PowerupKind::CenterRelease),
        "center behavior"
    );

    require(
        gh::original_powerup_extra_lanes(PowerupKind::FiveLaneRelease, 2, 5) ==
            std::vector<int>({0, 1, 3, 4}),
        "five-lane release"
    );
    require(
        gh::original_powerup_extra_lanes(PowerupKind::AdjacentRelease, 0, 5) ==
            std::vector<int>({1}),
        "top adjacent release"
    );
    require(
        gh::original_powerup_extra_lanes(PowerupKind::AdjacentRelease, 3, 5) ==
            std::vector<int>({2}),
        "other adjacent release"
    );
    return 0;
}
