#pragma once

namespace gh {

// The original gauge is a millisecond clock, not an ammunition counter. A
// launch transfers its current value to the new toy as winding energy.
class LaunchGauge {
public:
    void begin_match(int gauge_time_ms);
    void advance(double seconds, int gauge_time_ms);
    void reset_after_launch(int decay_time_ms);

    int winding(int gauge_time_ms) const;
    bool ready(int decay_time_ms, int gauge_time_ms) const;

private:
    double charge_ms_ = 0.0;
};

}  // namespace gh
