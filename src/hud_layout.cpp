#include "hud_layout.hpp"

#include <algorithm>
#include <array>

namespace gh {
namespace {

constexpr int kPreviewTop = 384;
constexpr int kPreviewBottom = 476;

bool valid_rectangle(const Rectangle& rectangle) {
    return rectangle.right > rectangle.left && rectangle.bottom > rectangle.top;
}

bool overlaps(const Rectangle& left, const Rectangle& right, int padding = 0) {
    if (!valid_rectangle(left) || !valid_rectangle(right)) return false;
    return left.left < right.right + padding && left.right > right.left - padding &&
           left.top < right.bottom + padding && left.bottom > right.top - padding;
}

}  // namespace

bool needs_fallback_toy_preview(
    const Rectangle& configured_rectangle,
    std::size_t roster_size
) {
    return !valid_rectangle(configured_rectangle) && roster_size > 1;
}

Rectangle fallback_toy_preview_rectangle(const LevelDefinition& level, int player) {
    constexpr std::array<Rectangle, 3> left_candidates{{
        {8, kPreviewTop, 104, kPreviewBottom},
        {112, kPreviewTop, 208, kPreviewBottom},
        {216, kPreviewTop, 312, kPreviewBottom},
    }};
    constexpr std::array<Rectangle, 3> right_candidates{{
        {536, kPreviewTop, 632, kPreviewBottom},
        {432, kPreviewTop, 528, kPreviewBottom},
        {328, kPreviewTop, 424, kPreviewBottom},
    }};
    const auto& candidates = player == 0 ? left_candidates : right_candidates;
    for (const Rectangle& candidate : candidates) {
        if (!overlaps(candidate, level.left_gauge, 4) &&
            !overlaps(candidate, level.right_gauge, 4)) {
            return candidate;
        }
    }
    return candidates.back();
}

}  // namespace gh
