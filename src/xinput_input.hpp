#pragma once

#include "input.hpp"

#include <cstdint>
#include <memory>

namespace gh {

// Convert raw XInput button and left-stick values to the engine's fixed
// controller controls. Exposed separately so the mapping and dead zone can be
// regression tested without requiring physical hardware.
std::uint16_t normalized_xinput_controls(
    std::uint16_t buttons,
    std::int16_t thumb_x,
    std::int16_t thumb_y
);

constexpr std::uint16_t newly_pressed_gamepad_controls(
    std::uint16_t current,
    std::uint16_t previous
) {
    return static_cast<std::uint16_t>(current & ~previous);
}

class XInputInput {
public:
    XInputInput();
    ~XInputInput();

    XInputInput(const XInputInput&) = delete;
    XInputInput& operator=(const XInputInput&) = delete;

    void poll(InputState& input, bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gh
