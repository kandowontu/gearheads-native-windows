#include "input.hpp"
#include "xinput_input.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool includes(std::uint16_t mask, gh::GamepadControl control) {
    return (mask & gh::gamepad_control_bit(control)) != 0;
}

}  // namespace

int main() {
    try {
        using gh::GamepadControl;

        constexpr std::uint16_t dpad_up = 0x0001;
        constexpr std::uint16_t button_a = 0x1000;
        constexpr std::uint16_t button_b = 0x2000;
        constexpr std::uint16_t left_shoulder = 0x0100;
        constexpr std::uint16_t right_shoulder = 0x0200;

        const std::uint16_t buttons = gh::normalized_xinput_controls(
            static_cast<std::uint16_t>(
                dpad_up | button_a | button_b | left_shoulder | right_shoulder
            ),
            0,
            0
        );
        require(includes(buttons, GamepadControl::Up), "D-pad up was not mapped");
        require(includes(buttons, GamepadControl::A), "A was not mapped");
        require(includes(buttons, GamepadControl::B), "B was not mapped");
        require(
            includes(buttons, GamepadControl::LeftShoulder),
            "left shoulder was not mapped"
        );
        require(
            includes(buttons, GamepadControl::RightShoulder),
            "right shoulder was not mapped"
        );

        const std::uint16_t neutral = gh::normalized_xinput_controls(0, 16000, -16000);
        require(neutral == 0, "stick threshold should be strict");
        const std::uint16_t diagonal = gh::normalized_xinput_controls(0, -16001, 16001);
        require(includes(diagonal, GamepadControl::Left), "left stick was not mapped");
        require(includes(diagonal, GamepadControl::Up), "up stick was not mapped");

        const std::uint16_t previous = gh::gamepad_control_bit(GamepadControl::A);
        const std::uint16_t current = static_cast<std::uint16_t>(
            previous | gh::gamepad_control_bit(GamepadControl::Right)
        );
        require(
            gh::newly_pressed_gamepad_controls(current, previous) ==
                gh::gamepad_control_bit(GamepadControl::Right),
            "held controls repeated as new presses"
        );

        const std::array<bool, 2> one_player{true, false};
        const std::array<bool, 2> two_player{false, false};
        require(
            !gh::human_gamepad_index(one_player, 0).has_value(),
            "computer side received a controller"
        );
        require(
            gh::human_gamepad_index(one_player, 1) == 0,
            "one-player human did not receive controller 1"
        );
        require(
            gh::human_gamepad_index(two_player, 0) == 0 &&
                gh::human_gamepad_index(two_player, 1) == 1,
            "two-player controllers were not assigned left to right"
        );

        gh::InputState input;
        input.gamepads[0].connected = true;
        input.gamepads[0].pressed = gh::gamepad_control_bit(GamepadControl::A);
        require(gh::any_gamepad_pressed(input), "controller press was not visible");
        require(
            gh::any_gamepad_control_pressed(input, GamepadControl::A),
            "controller A press was not visible"
        );

        std::cout << "validated XInput mapping, edge detection, and player assignment\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
