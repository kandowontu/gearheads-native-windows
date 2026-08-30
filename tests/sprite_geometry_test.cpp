#include "sprite_geometry.hpp"

#include <cmath>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.001F;
}
}  // namespace

int main() {
    using namespace gh;

    // Clucketta wh01 is 66x78 with recovered signed origin (34,47).
    const auto forward = original_sprite_collision_box(
        200.0F, 150.0F, 66, 78, 34, 47, 15, 60, 75, 120, false
    );
    require(near(forward.left, 175.75F), "forward left extent lost Win16 MulDiv rounding");
    require(near(forward.top, 149.1875F), "top extent ignored the XR origin");
    require(near(forward.right, 214.75F), "forward right extent is wrong");
    require(near(forward.bottom, 195.375F), "bottom extent is wrong");

    const auto mirrored = original_sprite_collision_box(
        200.0F, 150.0F, 66, 78, 34, 47, 15, 60, 75, 120, true
    );
    require(near(mirrored.left, 182.25F), "mirrored back extent is wrong");
    require(near(mirrored.right, 221.25F), "mirrored front extent is wrong");
    require(original_sprite_draw_x(200.0F, 34) == 166, "draw X ignored XR origin");
    require(original_sprite_draw_y(150.0F, 47) == 103, "draw Y ignored XR origin");
}
