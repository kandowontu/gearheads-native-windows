#include "launch_timing.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() {
    try {
        constexpr int gauge_time = 6000;
        constexpr int decay_time = 1200;
        gh::LaunchGauge gauge;

        gauge.begin_match(gauge_time);
        require(gauge.winding(gauge_time) == 6000, "match must begin fully wound");
        require(gauge.ready(decay_time, gauge_time), "initial launch must be ready");

        gauge.reset_after_launch(decay_time);
        require(gauge.winding(gauge_time) == 600, "launch must reset to half DecayTime");
        require(!gauge.ready(decay_time, gauge_time), "immediate repeat launch must be blocked");

        gauge.advance(0.600, gauge_time);
        require(gauge.winding(gauge_time) == 1200, "600 ms must reach the threshold");
        require(!gauge.ready(decay_time, gauge_time), "threshold itself must remain blocked");

        gauge.advance(0.001, gauge_time);
        require(gauge.winding(gauge_time) == 1201, "one more millisecond must be retained");
        require(gauge.ready(decay_time, gauge_time), "charge above DecayTime must launch");

        gauge.advance(60.0, gauge_time);
        require(gauge.winding(gauge_time) == 6000, "charge must cap at GaugeTime");

        std::cout << "validated original launch charge, strict threshold, reset, and cap\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
