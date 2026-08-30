#pragma once

namespace gh {

struct SpriteCollisionBox {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

// Segment 14:0e26-0f7b builds collision rectangles in the original 1/16-pixel
// coordinate space. The object position is the XR sprite's signed-origin
// anchor, rather than the center of its current bitmap.
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
);

int original_sprite_draw_x(float anchor_x, int origin_x);
int original_sprite_draw_y(float anchor_y, int origin_y);

}  // namespace gh
