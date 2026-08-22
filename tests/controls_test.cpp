#include "controls.hpp"

#include <windows.h>

#include <cassert>
#include <filesystem>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace gh;
    if (argc != 2) throw std::runtime_error("expected temporary settings directory");

    const std::filesystem::path directory = std::filesystem::path(argv[1]);
    std::filesystem::remove_all(directory);
    const std::filesystem::path settings = directory / "controls.ini";

    ControlBindings controls{settings};
    assert(controls.binding(ControlAction::LeftLaneUp).primary == 'W');
    assert(controls.binding(ControlAction::RightRelease).primary == VK_RETURN);
    assert(controls.binding(ControlAction::RightRelease).secondary == VK_SPACE);
    assert(controls.binding_text(ControlAction::RightRelease) == L"Enter / Space");
    assert(control_action_from_command(L">bind$left_lane_up") ==
           ControlAction::LeftLaneUp);
    assert(!control_action_from_command(L">bind$unknown").has_value());

    assert(controls.rebind(ControlAction::LeftLaneUp, VK_UP));
    assert(controls.binding(ControlAction::LeftLaneUp).primary == VK_UP);
    assert(controls.binding(ControlAction::RightLaneUp).primary == 'W');
    assert(!controls.rebind(ControlAction::LeftLaneUp, VK_ESCAPE));
    assert(!controls.rebind(ControlAction::LeftLaneUp, VK_F9));

    std::array<bool, 256> pressed{};
    pressed[VK_UP] = true;
    assert(controls.pressed(ControlAction::LeftLaneUp, pressed));
    assert(!controls.pressed(ControlAction::RightLaneUp, pressed));

    assert(controls.save());
    ControlBindings loaded{settings};
    assert(loaded.binding(ControlAction::LeftLaneUp).primary == VK_UP);
    assert(loaded.binding(ControlAction::RightLaneUp).primary == 'W');

    loaded.reset_defaults();
    assert(loaded.binding(ControlAction::LeftLaneUp).primary == 'W');
    assert(loaded.binding(ControlAction::RightRelease).secondary == VK_SPACE);
    std::filesystem::remove_all(directory);
    return 0;
}
