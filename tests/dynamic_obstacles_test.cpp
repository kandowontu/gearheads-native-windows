#include "dynamic_obstacles.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    const gh::BugSteerImpulse separated = gh::original_bug_separation_impulse(
        1600, 1600, 1280, 1440, 0, 0, 6400, 4800, 3, 2
    );
    require(separated.x16 == 160 && separated.y16 == 80, "half separation");

    const gh::BugSteerImpulse distant = gh::original_bug_separation_impulse(
        1600, 1600, 0, 0, 0, 0, 6400, 4800, 3, 2
    );
    require(distant.x16 == 3 && distant.y16 == 2, "distant fallback jitter");

    const gh::BugSteerImpulse bounded = gh::original_bug_separation_impulse(
        16, 16, 80, 80, 0, 0, 6400, 4800, 3, 2
    );
    require(bounded.x16 == -16 && bounded.y16 == -16, "stage bounds");

    require(gh::original_bug_transferred_impulse(80, 30, 60) == 40, "mass transfer");
    require(gh::original_bug_transferred_impulse(-81, 30, 60) == -41, "signed rounding");
    require(gh::original_bug_transferred_impulse(80, 30, 0) == 0, "massless target");
    require(gh::original_bug_slow_velocity(200) == 32, "positive slow velocity");
    require(gh::original_bug_slow_velocity(-200) == -32, "negative slow velocity");
    return 0;
}
