#include "xinput_input.hpp"

#include <windows.h>
#include <xinput.h>

#include <array>
#include <cstddef>
#include <cstring>

namespace gh {
namespace {

constexpr std::int16_t kDirectionalThreshold = 16000;

void add_control(std::uint16_t& mask, GamepadControl control) {
    mask = static_cast<std::uint16_t>(mask | gamepad_control_bit(control));
}

}  // namespace

std::uint16_t normalized_xinput_controls(
    const std::uint16_t buttons,
    const std::int16_t thumb_x,
    const std::int16_t thumb_y
) {
    std::uint16_t result = 0;
    if ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0 || thumb_y > kDirectionalThreshold) {
        add_control(result, GamepadControl::Up);
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0 || thumb_y < -kDirectionalThreshold) {
        add_control(result, GamepadControl::Down);
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0 || thumb_x < -kDirectionalThreshold) {
        add_control(result, GamepadControl::Left);
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0 || thumb_x > kDirectionalThreshold) {
        add_control(result, GamepadControl::Right);
    }
    if ((buttons & XINPUT_GAMEPAD_A) != 0) add_control(result, GamepadControl::A);
    if ((buttons & XINPUT_GAMEPAD_B) != 0) add_control(result, GamepadControl::B);
    if ((buttons & XINPUT_GAMEPAD_X) != 0) add_control(result, GamepadControl::X);
    if ((buttons & XINPUT_GAMEPAD_Y) != 0) add_control(result, GamepadControl::Y);
    if ((buttons & XINPUT_GAMEPAD_START) != 0) add_control(result, GamepadControl::Start);
    if ((buttons & XINPUT_GAMEPAD_BACK) != 0) add_control(result, GamepadControl::Back);
    if ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0) {
        add_control(result, GamepadControl::LeftShoulder);
    }
    if ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0) {
        add_control(result, GamepadControl::RightShoulder);
    }
    return result;
}

struct XInputInput::Impl {
    using GetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

    HMODULE module = nullptr;
    GetStateFunction get_state = nullptr;
    std::array<std::uint16_t, XUSER_MAX_COUNT> previous{};

    Impl() {
        constexpr std::array<const wchar_t*, 3> libraries{
            L"xinput1_4.dll",
            L"xinput1_3.dll",
            L"xinput9_1_0.dll",
        };
        for (const wchar_t* library : libraries) {
            module = LoadLibraryExW(library, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (module == nullptr) continue;
            const FARPROC procedure = GetProcAddress(module, "XInputGetState");
            static_assert(sizeof(procedure) == sizeof(get_state));
            std::memcpy(&get_state, &procedure, sizeof(get_state));
            if (get_state != nullptr) break;
            FreeLibrary(module);
            module = nullptr;
        }
    }

    ~Impl() {
        if (module != nullptr) FreeLibrary(module);
    }

    void poll(InputState& input, const bool enabled) {
        input.gamepads = {};
        if (!enabled || get_state == nullptr) {
            previous.fill(0);
            return;
        }

        std::size_t connected_index = 0;
        for (DWORD user_index = 0; user_index < XUSER_MAX_COUNT; ++user_index) {
            XINPUT_STATE state{};
            const bool connected = get_state(user_index, &state) == ERROR_SUCCESS;
            const std::uint16_t current = connected
                                              ? normalized_xinput_controls(
                                                    state.Gamepad.wButtons,
                                                    state.Gamepad.sThumbLX,
                                                    state.Gamepad.sThumbLY
                                                )
                                              : 0;
            const std::uint16_t pressed = newly_pressed_gamepad_controls(
                current, previous[static_cast<std::size_t>(user_index)]
            );
            previous[static_cast<std::size_t>(user_index)] = current;
            if (!connected || connected_index >= input.gamepads.size()) continue;

            GamepadState& gamepad = input.gamepads[connected_index++];
            gamepad.connected = true;
            gamepad.user_index = user_index;
            gamepad.pressed = pressed;
            gamepad.held = current;
        }
    }
};

XInputInput::XInputInput() : impl_(std::make_unique<Impl>()) {}
XInputInput::~XInputInput() = default;

void XInputInput::poll(InputState& input, const bool enabled) {
    impl_->poll(input, enabled);
}

}  // namespace gh
