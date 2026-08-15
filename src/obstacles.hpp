#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace gh {

enum class ObstacleInteraction {
    Surface,
    Dynamic,
    MotionBlocking,
};

struct ObstacleArchetype {
    int code = 0;
    std::string_view parameters;
    std::filesystem::path sprite;
    ObstacleInteraction interaction = ObstacleInteraction::Surface;
};

std::optional<ObstacleArchetype> obstacle_archetype(int code);

}  // namespace gh
