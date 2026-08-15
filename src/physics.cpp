#include "physics.hpp"

#include <cmath>
#include <array>

namespace gh {
namespace {

int truncate_component(double value) {
    // Segment 16:0882-08be temporarily selects the x87 truncate-toward-zero
    // rounding mode before FISTP stores each response component.
    return static_cast<int>(value);
}

int arithmetic_shift_right(int value, int bits) {
    if (value >= 0) return value >> bits;
    const int divisor = 1 << bits;
    return -((-value + divisor - 1) / divisor);
}

constexpr std::array<int, 64> kOriginalSine{
    0,    100,  200,  297,  392,  483,  569,  650,  724,  792,  851,
    903,  946,  980,  1004, 1019, 1024, 1019, 1004, 980,  946,  903,
    851,  792,  724,  650,  569,  483,  392,  297,  200,  100,  0,
    -100, -200, -297, -392, -483, -569, -650, -724, -792, -851, -903,
    -946, -980, -1004, -1019, -1024, -1019, -1004, -980, -946, -903,
    -851, -792, -724, -650, -569, -483, -392, -297, -200, -100,
};

// The recovered cosine table is intentionally not derived from the sine
// table: the original has several one-unit asymmetries in its negative half.
constexpr std::array<int, 64> kOriginalCosine{
    1024, 1019, 1004, 980,  946,  903,  851,  792,  724,  650,  569,
    483,  392,  297,  200,  100,  0,    -99,  -199, -296, -391, -482,
    -568, -649, -723, -791, -850, -902, -945, -979, -1003, -1018,
    -1024, -1019, -1004, -980, -946, -903, -851, -792, -724, -650,
    -569, -483, -392, -297, -200, -100, 0,    99,   199,  296,
    391,  482,  568,  649,  723,  791,  850,  902,  945,  979,  1003,
    1019,
};

}  // namespace

int scaled_toy_speed16(
    int horizontal_speed,
    int winding,
    int decay_time,
    int slowing_time
) {
    const int remaining = winding - decay_time;
    if (remaining <= 0) return 0;
    int speed = horizontal_speed;
    if (remaining < slowing_time && slowing_time > 0) {
        // Win16 MulDiv rounds this positive ratio to the nearest integer.
        speed = (horizontal_speed * remaining + slowing_time / 2) / slowing_time;
    }
    return speed * 4;
}

MotionVector vector_from_original_heading(int magnitude16, int heading) {
    const int index = heading & 63;
    return {
        arithmetic_shift_right(magnitude16 * kOriginalCosine[static_cast<std::size_t>(index)], 10),
        arithmetic_shift_right(magnitude16 * kOriginalSine[static_cast<std::size_t>(index)], 10),
    };
}

int approach_original_velocity(int current16, int desired16, int friction) {
    return arithmetic_shift_right(
        current16 * 16 + (desired16 - current16) * friction,
        4
    );
}

MotionVector integrate_original_contact_group(
    std::span<const ContactMotionBody> bodies,
    int friction
) {
    long long mass_sum = 0;
    long long current_x_sum = 0;
    long long current_y_sum = 0;
    long long desired_x_sum = 0;
    long long desired_y_sum = 0;
    for (const ContactMotionBody& body : bodies) {
        if (body.motion_blocking) return {};
        mass_sum += body.mass;
        current_x_sum += static_cast<long long>(body.mass) * body.velocity_x16;
        current_y_sum += static_cast<long long>(body.mass) * body.velocity_y16;
        desired_x_sum += static_cast<long long>(body.mass) * body.desired_x16;
        desired_y_sum += static_cast<long long>(body.mass) * body.desired_y16;
    }
    if (mass_sum == 0) return {};

    // Segment 14:05c6-063f uses signed 32-bit division twice, which truncates
    // toward zero: first by 16, then by the component's combined mass.
    const long long adjusted_x =
        current_x_sum + (desired_x_sum - current_x_sum) * friction / 16;
    const long long adjusted_y =
        current_y_sum + (desired_y_sum - current_y_sum) * friction / 16;
    return {
        static_cast<int>(adjusted_x / mass_sum),
        static_cast<int>(adjusted_y / mass_sum),
    };
}

bool resolve_original_collision(CollisionBody& first, CollisionBody& second) {
    if (!first.physical || !second.physical) return false;

    const double delta_x = static_cast<double>(second.position_x16 - first.position_x16);
    const double delta_y = static_cast<double>(second.position_y16 - first.position_y16);
    const double distance = std::hypot(delta_x, delta_y);
    if (distance == 0.0) return false;

    const double normal_x = delta_x / distance;
    const double normal_y = delta_y / distance;
    const double first_x = static_cast<double>(first.velocity_x16);
    const double first_y = static_cast<double>(first.velocity_y16);
    const double second_x = static_cast<double>(second.velocity_x16);
    const double second_y = static_cast<double>(second.velocity_y16);

    const double first_normal = first_x * normal_x + first_y * normal_y;
    const double second_normal = second_x * normal_x + second_y * normal_y;
    if (first_normal < second_normal) return false;

    double first_tangent = first_x * normal_y - first_y * normal_x;
    double second_tangent = second_x * normal_y - second_y * normal_x;
    double first_response = 0.0;
    double second_response = 0.0;

    // Type flags 0x08/0x10 select the original engine's motion-blocking
    // branch.  Playable toy records have neither bit, but obstacles use it.
    if (first.motion_blocking) {
        first_tangent = 0.0;
        second_response = -second_normal;
    } else if (second.motion_blocking) {
        first_response = -first_normal;
        second_tangent = 0.0;
    } else {
        const int mass_sum = first.mass + second.mass;
        if (mass_sum == 0) return false;
        first_response =
            (2.0 * static_cast<double>(second.mass) * second_normal +
             static_cast<double>(first.mass - second.mass) * first_normal) /
            static_cast<double>(mass_sum);
        second_response =
            (2.0 * static_cast<double>(first.mass) * first_normal +
             static_cast<double>(second.mass - first.mass) * second_normal) /
            static_cast<double>(mass_sum);
    }

    first.velocity_x16 =
        truncate_component(first_response * normal_x + first_tangent * normal_y);
    first.velocity_y16 =
        truncate_component(first_response * normal_y - first_tangent * normal_x);
    second.velocity_x16 =
        truncate_component(second_response * normal_x + second_tangent * normal_y);
    second.velocity_y16 =
        truncate_component(second_response * normal_y - second_tangent * normal_x);
    return true;
}

}  // namespace gh
