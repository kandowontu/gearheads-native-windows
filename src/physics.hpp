#pragma once

#include <span>

namespace gh {

// The original engine stores positions and velocities in signed 1/16-pixel
// units.  Keeping this boundary explicit also preserves its post-collision
// truncation behavior.
struct CollisionBody {
    int position_x16 = 0;
    int position_y16 = 0;
    int velocity_x16 = 0;
    int velocity_y16 = 0;
    int mass = 0;
    bool physical = true;
    bool motion_blocking = false;
};

struct MotionVector {
    int x16 = 0;
    int y16 = 0;
};

struct ContactMotionBody {
    int velocity_x16 = 0;
    int velocity_y16 = 0;
    int desired_x16 = 0;
    int desired_y16 = 0;
    int mass = 0;
    bool motion_blocking = false;
};

// Port of GEAR_EN segment 14:0062-03fb.  Returns false when the bodies are
// coincident, non-physical, or already separating.
bool resolve_original_collision(CollisionBody& first, CollisionBody& second);

// Helpers used by segment 10's movement target function and segment 14's
// isolated-body integrator.
int scaled_toy_speed16(
    int horizontal_speed,
    int winding,
    int decay_time,
    int slowing_time
);
MotionVector vector_from_original_heading(int magnitude16, int heading);
int approach_original_velocity(int current16, int desired16, int friction);
MotionVector integrate_original_contact_group(
    std::span<const ContactMotionBody> bodies,
    int friction
);

}  // namespace gh
