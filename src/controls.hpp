#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace gh {

enum class ControlAction : std::size_t {
    LeftLaneUp,
    LeftLaneDown,
    LeftToyPrevious,
    LeftToyNext,
    LeftRelease,
    RightLaneUp,
    RightLaneDown,
    RightToyPrevious,
    RightToyNext,
    RightRelease,
    Count,
};

struct KeyBinding {
    int primary = 0;
    int secondary = 0;
};

class ControlBindings {
public:
    explicit ControlBindings(std::filesystem::path settings_path = {});

    const KeyBinding& binding(ControlAction action) const;
    bool pressed(ControlAction action, const std::array<bool, 256>& pressed_keys) const;
    bool rebind(ControlAction action, int virtual_key);
    void reset_defaults();
    bool load();
    bool save() const;

    std::wstring binding_text(ControlAction action) const;

private:
    std::filesystem::path settings_path_;
    std::array<KeyBinding, static_cast<std::size_t>(ControlAction::Count)> bindings_{};
};

std::optional<ControlAction> control_action_from_command(std::wstring_view command);
std::wstring_view control_action_label(ControlAction action);
std::wstring virtual_key_name(int virtual_key);
bool bindable_virtual_key(int virtual_key);

}  // namespace gh
