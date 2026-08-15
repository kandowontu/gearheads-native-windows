#include "defaults.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected runtime-defaults.ini path");
        const gh::DefaultsDatabase defaults{std::filesystem::path(argv[1])};
        require(defaults.numeric_count() == 48, "expected all 48 numeric descriptors");
        require(defaults.string_count() == 3, "expected all three string descriptors");
        require(defaults.scalar("GaugeTime") == 6000, "GaugeTime recovery changed");
        require(defaults.scalar("FrameStep") == 55, "FrameStep recovery changed");
        require(defaults.scalar("Winningscore") == 21, "winning score recovery changed");
        require(defaults.text("Anim") == "anim.dat", "Anim string recovery changed");

        const auto ziggy = defaults.toy("roach");
        require(
            ziggy.values == std::array<int, 12>{8, 120, 2, 3, 0, 0, 8, 40, 75, 100, 4, 5},
            "Ziggy parameter vector changed"
        );
        require(ziggy.mass() == 8 && ziggy.horizontal_speed() == 120, "named toy fields shifted");
        require(
            ziggy.collision_front_percent() == 8 &&
                ziggy.collision_bottom_percent() == 100,
            "collision parameter fields shifted"
        );
        const auto wall4 = defaults.toy("wall4");
        require(wall4.values[9] == 66, "wall4 vector was not preserved");
        std::cout << "validated native access to all 51 recovered defaults\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
