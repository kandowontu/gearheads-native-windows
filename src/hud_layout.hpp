#pragma once

#include "levels.hpp"

#include <cstddef>

namespace gh {

bool needs_fallback_toy_preview(
    const Rectangle& configured_rectangle,
    std::size_t roster_size
);

Rectangle fallback_toy_preview_rectangle(const LevelDefinition& level, int player);

}  // namespace gh
