#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace gh {

enum class GamepadControl : std::uint16_t {
    Up = 1U << 0,
    Down = 1U << 1,
    Left = 1U << 2,
    Right = 1U << 3,
    A = 1U << 4,
    B = 1U << 5,
    X = 1U << 6,
    Y = 1U << 7,
    Start = 1U << 8,
    Back = 1U << 9,
    LeftShoulder = 1U << 10,
    RightShoulder = 1U << 11,
};

constexpr std::uint16_t gamepad_control_bit(GamepadControl control) {
    return static_cast<std::uint16_t>(control);
}

struct GamepadState {
    bool connected = false;
    std::uint32_t user_index = 0;
    std::uint16_t pressed = 0;
    std::uint16_t held = 0;
};

struct InputState {
    std::array<bool, 256> pressed{};
    std::array<bool, 256> held{};
    std::array<GamepadState, 2> gamepads{};
};

constexpr bool gamepad_control_pressed(
    const GamepadState& gamepad,
    GamepadControl control
) {
    return (gamepad.pressed & gamepad_control_bit(control)) != 0;
}

constexpr bool gamepad_control_held(
    const GamepadState& gamepad,
    GamepadControl control
) {
    return (gamepad.held & gamepad_control_bit(control)) != 0;
}

inline bool any_gamepad_control_pressed(
    const InputState& input,
    GamepadControl control
) {
    for (const GamepadState& gamepad : input.gamepads) {
        if (gamepad.connected && gamepad_control_pressed(gamepad, control)) return true;
    }
    return false;
}

inline bool any_gamepad_pressed(const InputState& input) {
    for (const GamepadState& gamepad : input.gamepads) {
        if (gamepad.connected && gamepad.pressed != 0) return true;
    }
    return false;
}

// Connected controllers are compacted into input.gamepads. Assign them to
// human-controlled board sides from left to right. This makes controller 1
// operate the sole right-side human in one-player modes and the left-side
// human in a two-player duel.
inline std::optional<std::size_t> human_gamepad_index(
    const std::array<bool, 2>& computer_controlled,
    int player
) {
    if (player < 0 || player >= static_cast<int>(computer_controlled.size()) ||
        computer_controlled[static_cast<std::size_t>(player)]) {
        return std::nullopt;
    }
    std::size_t index = 0;
    for (int candidate = 0; candidate < player; ++candidate) {
        if (!computer_controlled[static_cast<std::size_t>(candidate)]) ++index;
    }
    return index;
}

}  // namespace gh
