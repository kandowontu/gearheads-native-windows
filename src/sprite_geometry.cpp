#include "sprite_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gh {
namespace {

int muldiv_nearest(int value, int multiplier, int divisor) {
    if (divisor == 0) return 0;
    const long long product = static_cast<long long>(value) * multiplier;
    const long long half = std::abs(divisor) / 2;
    if (product >= 0) return static_cast<int>((product + half) / divisor);
    return -static_cast<int>((-product + half) / divisor);
}

float pixels(int value16) {
    return static_cast<float>(value16) / 16.0F;
}

}  // namespace

SpriteCollisionBox original_sprite_collision_box(
    float anchor_x,
    float anchor_y,
    int width,
    int height,
    int origin_x,
    int origin_y,
    int front_percent,
    int top_percent,
    int back_percent,
    int bottom_percent,
    bool mirrored
) {
    const int anchor_x16 = static_cast<int>(std::lround(anchor_x * 16.0F));
    const int anchor_y16 = static_cast<int>(std::lround(anchor_y * 16.0F));
    const int width16 = std::max(0, width - 1) * 16;
    const int height16 = std::max(0, height - 1) * 16;
    const int base_x16 = anchor_x16 - origin_x * 16;
    const int base_y16 = anchor_y16 - origin_y * 16;

    const auto horizontal = [=](int percent) {
        const int extent = muldiv_nearest(width16, percent, 100);
        return mirrored ? base_x16 + width16 - extent : base_x16 + extent;
    };
    const int front16 = horizontal(front_percent);
    const int back16 = horizontal(back_percent);
    const int top16 = base_y16 + muldiv_nearest(height16, top_percent, 100);
    const int bottom16 = base_y16 + muldiv_nearest(height16, bottom_percent, 100);
    return {
        pixels(std::min(front16, back16)),
        pixels(std::min(top16, bottom16)),
        pixels(std::max(front16, back16)),
        pixels(std::max(top16, bottom16)),
    };
}

int original_sprite_draw_x(float anchor_x, int origin_x) {
    return static_cast<int>(anchor_x) - origin_x;
}

int original_sprite_draw_y(float anchor_y, int origin_y) {
    return static_cast<int>(anchor_y) - origin_y;
}

}  // namespace gh
