#include "toy_effects.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        constexpr int radius = 83;
        gh::BlastTarget ordinary{15, 82 * 16, 0, 900, true};
        require(gh::original_bomby_blast_winding(0, 0, radius, ordinary) == 1,
                "ordinary target should be reduced to one winding unit");
        ordinary.x16 = radius * 16;
        require(gh::original_bomby_blast_winding(0, 0, radius, ordinary) == 900,
                "blast-radius boundary must be strict");
        ordinary.x16 = 0;
        ordinary.winding = 1;
        require(gh::original_bomby_blast_winding(0, 0, radius, ordinary) == 1,
                "one-unit target should remain at one");
        gh::BlastTarget bomby{gh::kOriginalBombyType, 0, 0, 500, true};
        require(gh::original_bomby_blast_winding(0, 0, radius, bomby) == -100,
                "Bomby should chain-trigger at -100");
        gh::BlastTarget disasteroid{gh::kOriginalDisasteroidType, 0, 0, 500, true};
        require(gh::original_bomby_blast_winding(0, 0, radius, disasteroid) == 500,
                "Disasteroid must be immune");
        gh::BlastTarget decorative{15, 0, 0, 500, false};
        require(gh::original_bomby_blast_winding(0, 0, radius, decorative) == 500,
                "nonphysical object must be ignored");
        gh::BlastTarget expired{15, 0, 0, 0, true};
        require(gh::original_bomby_blast_winding(0, 0, radius, expired) == 0,
                "expired object must be ignored");
        require(gh::original_target_is_in_front(0, 100, 101),
                "player-one front should face right");
        require(!gh::original_target_is_in_front(0, 100, 99),
                "player-one rear should face left");
        require(gh::original_target_is_in_front(1, 100, 99),
                "player-two front should face left");
        require(gh::original_zappa_drain(1000, 150) == 850, "Zap drain mismatch");
        require(gh::original_zappa_drain(150, 150) == 1, "Zap terminal drain mismatch");
        require(gh::original_zappa_drain(1, 150) == 1, "Zap should not underflow winding");
        require(gh::original_kanga_impulse_x16(0, 70, 35, 35) == 1120,
                "equal-mass Kanga impulse mismatch");
        require(gh::original_kanga_impulse_x16(1, 70, 35, 70) == -560,
                "left-facing Kanga impulse mismatch");
        int impulse = 1500;
        require(gh::take_original_horizontal_impulse_step(impulse) == 1024 && impulse == 476,
                "positive impulse clamp mismatch");
        impulse = -1500;
        require(gh::take_original_horizontal_impulse_step(impulse) == -1024 && impulse == -476,
                "negative impulse clamp mismatch");
        require(gh::inside_original_radius(0, 0, 30, 40, 4, true),
                "inclusive radius rejected an interior point");
        require(gh::inside_original_radius(0, 0, 48, 64, 5, true),
                "inclusive radius rejected its boundary");
        require(!gh::inside_original_radius(0, 0, 48, 64, 5, false),
                "strict radius accepted its boundary");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
