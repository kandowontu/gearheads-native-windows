#include "obstacles.hpp"

namespace gh {

std::optional<ObstacleArchetype> obstacle_archetype(int code) {
    // GEARHEAD.INI's level-hacking notes give the public obstacle codes.  The
    // mapped names are script records 6-14 and 29-34 at DS:2354.
    switch (code) {
        case 51: return ObstacleArchetype{51, "htrd", "sprites/digit/c1fwh01.png"};
        case 52: return ObstacleArchetype{52, "utrd", "sprites/digit/c2fwh01.png"};
        case 53: return ObstacleArchetype{53, "dtrd", "sprites/digit/c2fwh04.png"};
        case 54: return ObstacleArchetype{54, "crak", "sprites/digit/crpwh04.png"};
        case 55: return ObstacleArchetype{55, "tele", "sprites/digit/tpfwh01.png"};
        // The script record is named "mudd", but its six-word configuration
        // descriptor is named "hole" and writes into mudd+0x12.
        case 56: return ObstacleArchetype{56, "hole", "sprites/digit/m1gwh01.png"};
        case 57: return ObstacleArchetype{57, "oily", "sprites/digit/m2gwh01.png"};
        case 58: return ObstacleArchetype{58, "glue", "sprites/digit/m3gwh01.png"};
        case 59: return ObstacleArchetype{59, "rock", "sprites/digit/r2gwh01.png"};
        case 73:
            return ObstacleArchetype{
                73, "buggy", "sprites/gb/gb_wh01.png", ObstacleInteraction::Dynamic
            };
        case 74:
            return ObstacleArchetype{
                74, "block", "sprites/digit/r1gwh01.png", ObstacleInteraction::Dynamic
            };
        case 75:
            return ObstacleArchetype{
                75, "wall1", "sprites/digit/gtfwh01.png", ObstacleInteraction::MotionBlocking
            };
        case 76:
            return ObstacleArchetype{
                76, "wall2", "sprites/digit/gtf2w01.png", ObstacleInteraction::MotionBlocking
            };
        case 77:
            return ObstacleArchetype{
                77, "wall3", "sprites/digit/gtf3w01.png", ObstacleInteraction::MotionBlocking
            };
        case 78:
            return ObstacleArchetype{
                78, "wall4", "sprites/digit/gtfwh01.png", ObstacleInteraction::MotionBlocking
            };
        default: return std::nullopt;
    }
}

}  // namespace gh
