#include "obstacles.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main() {
    try {
        for (int code : {51, 52, 53, 54, 55, 56, 57, 58, 59, 73, 74, 75, 76, 77, 78}) {
            const auto obstacle = gh::obstacle_archetype(code);
            require(obstacle.has_value(), "shipped obstacle code is unmapped");
            require(!obstacle->parameters.empty(), "obstacle parameter record is missing");
            require(!obstacle->sprite.empty(), "obstacle sprite is missing");
        }
        require(!gh::obstacle_archetype(60).has_value(), "unknown obstacle was accepted");
        require(
            gh::obstacle_archetype(51)->parameters == "htrd" &&
                gh::obstacle_archetype(52)->parameters == "utrd" &&
                gh::obstacle_archetype(53)->parameters == "dtrd",
            "conveyor direction mapping changed"
        );
        require(
            gh::obstacle_archetype(56)->parameters == "hole",
            "mudd must use the recovered hole configuration descriptor"
        );
        require(
            gh::obstacle_archetype(73)->interaction == gh::ObstacleInteraction::Dynamic,
            "garden bug stopped being dynamic"
        );
        require(
            gh::obstacle_archetype(74)->interaction == gh::ObstacleInteraction::Dynamic,
            "heavy block stopped being movable"
        );
        for (int code : {75, 76, 77, 78}) {
            require(
                gh::obstacle_archetype(code)->interaction ==
                    gh::ObstacleInteraction::MotionBlocking,
                "solid block/wall mapping changed"
            );
        }
        std::cout << "validated all shipped obstacle-code mappings\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
