#include "launch_timing.hpp"

#include <algorithm>
#include <cmath>

namespace gh {

void LaunchGauge::begin_match(int gauge_time_ms) {
    charge_ms_ = static_cast<double>(std::max(0, gauge_time_ms));
}

void LaunchGauge::advance(double seconds, int gauge_time_ms) {
    const double maximum = static_cast<double>(std::max(0, gauge_time_ms));
    charge_ms_ = std::clamp(charge_ms_ + std::max(0.0, seconds) * 1000.0, 0.0, maximum);
}

void LaunchGauge::reset_after_launch(int decay_time_ms) {
    // Segment 12:059a divides DecayTime by two and moves the gauge clock back
    // by that amount. Segment 1 calls it after a launch and, when enabled, a
    // toy-selection change.
    charge_ms_ = static_cast<double>(std::max(0, decay_time_ms) / 2);
}

int LaunchGauge::winding(int gauge_time_ms) const {
    return std::clamp(
        static_cast<int>(std::floor(charge_ms_)),
        0,
        std::max(0, gauge_time_ms)
    );
}

bool LaunchGauge::ready(int decay_time_ms, int gauge_time_ms) const {
    // The original input path uses a strict signed greater-than comparison.
    return winding(gauge_time_ms) > decay_time_ms;
}

}  // namespace gh
