#include "controls.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <format>
#include <set>
#include <string>

namespace gh {
namespace {

constexpr std::size_t kActionCount = static_cast<std::size_t>(ControlAction::Count);

constexpr std::array<std::string_view, kActionCount> kSettingNames{
    "left_lane_up",
    "left_lane_down",
    "left_toy_previous",
    "left_toy_next",
    "left_release",
    "right_lane_up",
    "right_lane_down",
    "right_toy_previous",
    "right_toy_next",
    "right_release",
};

constexpr std::array<std::wstring_view, kActionCount> kCommandNames{
    L"left_lane_up",
    L"left_lane_down",
    L"left_toy_previous",
    L"left_toy_next",
    L"left_release",
    L"right_lane_up",
    L"right_lane_down",
    L"right_toy_previous",
    L"right_toy_next",
    L"right_release",
};

constexpr std::array<std::wstring_view, kActionCount> kLabels{
    L"Left player lane up",
    L"Left player lane down",
    L"Left player previous toy",
    L"Left player next toy",
    L"Left player release",
    L"Right player lane up",
    L"Right player lane down",
    L"Right player previous toy",
    L"Right player next toy",
    L"Right player release",
};

std::array<KeyBinding, kActionCount> default_bindings() {
    return {{
        {'W', 0},
        {'S', 0},
        {'A', 0},
        {'D', 0},
        {'F', 0},
        {VK_UP, 0},
        {VK_DOWN, 0},
        {VK_LEFT, 0},
        {VK_RIGHT, 0},
        {VK_RETURN, VK_SPACE},
    }};
}

std::optional<int> parse_key(std::string_view text) {
    if (text.empty()) return 0;
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(std::string(text), &consumed, 10);
        if (consumed != text.size()) return std::nullopt;
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool valid_bindings(const std::array<KeyBinding, kActionCount>& bindings) {
    std::set<int> assigned;
    for (const KeyBinding& binding : bindings) {
        if (!bindable_virtual_key(binding.primary) ||
            (binding.secondary != 0 && !bindable_virtual_key(binding.secondary))) {
            return false;
        }
        if (!assigned.insert(binding.primary).second) return false;
        if (binding.secondary != 0 && !assigned.insert(binding.secondary).second) return false;
    }
    return true;
}

}  // namespace

ControlBindings::ControlBindings(std::filesystem::path settings_path)
    : settings_path_(std::move(settings_path)) {
    reset_defaults();
    load();
}

const KeyBinding& ControlBindings::binding(ControlAction action) const {
    return bindings_[static_cast<std::size_t>(action)];
}

bool ControlBindings::pressed(
    ControlAction action,
    const std::array<bool, 256>& pressed_keys
) const {
    const KeyBinding& keys = binding(action);
    const auto is_pressed = [&pressed_keys](int key) {
        return key > 0 && key < static_cast<int>(pressed_keys.size()) &&
               pressed_keys[static_cast<std::size_t>(key)];
    };
    return is_pressed(keys.primary) || is_pressed(keys.secondary);
}

bool ControlBindings::rebind(ControlAction action, int virtual_key) {
    if (!bindable_virtual_key(virtual_key)) return false;
    const std::size_t target_index = static_cast<std::size_t>(action);
    if (target_index >= bindings_.size()) return false;

    const int previous_key = bindings_[target_index].primary;
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (index == target_index) continue;
        KeyBinding& other = bindings_[index];
        if (other.primary == virtual_key) other.primary = previous_key;
        if (other.secondary == virtual_key) other.secondary = 0;
    }
    bindings_[target_index] = {virtual_key, 0};
    return true;
}

void ControlBindings::reset_defaults() {
    bindings_ = default_bindings();
}

bool ControlBindings::load() {
    if (settings_path_.empty()) return false;
    std::ifstream input(settings_path_);
    if (!input) return false;

    auto candidate = default_bindings();
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string_view name(line.data(), equals);
        const std::string_view value(line.data() + equals + 1, line.size() - equals - 1);
        const auto found = std::find(kSettingNames.begin(), kSettingNames.end(), name);
        if (found == kSettingNames.end()) continue;

        const std::size_t index = static_cast<std::size_t>(found - kSettingNames.begin());
        const std::size_t comma = value.find(',');
        const auto primary = parse_key(value.substr(0, comma));
        const auto secondary = comma == std::string_view::npos
                                   ? std::optional<int>{0}
                                   : parse_key(value.substr(comma + 1));
        if (!primary.has_value() || !secondary.has_value()) return false;
        candidate[index] = {*primary, *secondary};
    }
    if (!valid_bindings(candidate)) return false;
    bindings_ = candidate;
    return true;
}

bool ControlBindings::save() const {
    if (settings_path_.empty()) return false;
    std::error_code error;
    if (!settings_path_.parent_path().empty()) {
        std::filesystem::create_directories(settings_path_.parent_path(), error);
        if (error) return false;
    }
    std::ofstream output(settings_path_, std::ios::trunc);
    if (!output) return false;
    output << "version=1\n";
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        output << kSettingNames[index] << '=' << bindings_[index].primary;
        if (bindings_[index].secondary != 0) {
            output << ',' << bindings_[index].secondary;
        }
        output << '\n';
    }
    return static_cast<bool>(output);
}

std::wstring ControlBindings::binding_text(ControlAction action) const {
    const KeyBinding& keys = binding(action);
    std::wstring result = virtual_key_name(keys.primary);
    if (keys.secondary != 0) result += L" / " + virtual_key_name(keys.secondary);
    return result;
}

std::optional<ControlAction> control_action_from_command(std::wstring_view command) {
    constexpr std::wstring_view prefix = L">bind$";
    if (!command.starts_with(prefix)) return std::nullopt;
    const std::wstring_view name = command.substr(prefix.size());
    const auto found = std::find(kCommandNames.begin(), kCommandNames.end(), name);
    if (found == kCommandNames.end()) return std::nullopt;
    return static_cast<ControlAction>(found - kCommandNames.begin());
}

std::wstring_view control_action_label(ControlAction action) {
    const std::size_t index = static_cast<std::size_t>(action);
    return index < kLabels.size() ? kLabels[index] : L"Control";
}

bool bindable_virtual_key(int virtual_key) {
    if (virtual_key <= 0 || virtual_key >= 256) return false;
    if (virtual_key >= VK_LBUTTON && virtual_key <= VK_XBUTTON2) return false;
    return virtual_key != VK_ESCAPE && virtual_key != VK_F9 && virtual_key != VK_F10;
}

std::wstring virtual_key_name(int virtual_key) {
    if (virtual_key >= 'A' && virtual_key <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(virtual_key));
    }
    if (virtual_key >= '0' && virtual_key <= '9') {
        return std::wstring(1, static_cast<wchar_t>(virtual_key));
    }
    if (virtual_key >= VK_F1 && virtual_key <= VK_F24) {
        return std::format(L"F{}", virtual_key - VK_F1 + 1);
    }
    if (virtual_key >= VK_NUMPAD0 && virtual_key <= VK_NUMPAD9) {
        return std::format(L"Num {}", virtual_key - VK_NUMPAD0);
    }
    switch (virtual_key) {
        case VK_BACK: return L"Backspace";
        case VK_TAB: return L"Tab";
        case VK_RETURN: return L"Enter";
        case VK_SHIFT: return L"Shift";
        case VK_CONTROL: return L"Ctrl";
        case VK_MENU: return L"Alt";
        case VK_PAUSE: return L"Pause";
        case VK_CAPITAL: return L"Caps Lock";
        case VK_SPACE: return L"Space";
        case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";
        case VK_END: return L"End";
        case VK_HOME: return L"Home";
        case VK_LEFT: return L"Left";
        case VK_UP: return L"Up";
        case VK_RIGHT: return L"Right";
        case VK_DOWN: return L"Down";
        case VK_SNAPSHOT: return L"Print Screen";
        case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete";
        case VK_LWIN: return L"Left Windows";
        case VK_RWIN: return L"Right Windows";
        case VK_APPS: return L"Menu";
        case VK_MULTIPLY: return L"Num *";
        case VK_ADD: return L"Num +";
        case VK_SUBTRACT: return L"Num -";
        case VK_DECIMAL: return L"Num .";
        case VK_DIVIDE: return L"Num /";
        case VK_NUMLOCK: return L"Num Lock";
        case VK_SCROLL: return L"Scroll Lock";
        case VK_LSHIFT: return L"Left Shift";
        case VK_RSHIFT: return L"Right Shift";
        case VK_LCONTROL: return L"Left Ctrl";
        case VK_RCONTROL: return L"Right Ctrl";
        case VK_LMENU: return L"Left Alt";
        case VK_RMENU: return L"Right Alt";
        case VK_OEM_1: return L";";
        case VK_OEM_PLUS: return L"=";
        case VK_OEM_COMMA: return L",";
        case VK_OEM_MINUS: return L"-";
        case VK_OEM_PERIOD: return L".";
        case VK_OEM_2: return L"/";
        case VK_OEM_3: return L"`";
        case VK_OEM_4: return L"[";
        case VK_OEM_5: return L"\\";
        case VK_OEM_6: return L"]";
        case VK_OEM_7: return L"'";
        default: return std::format(L"Key {}", virtual_key);
    }
}

}  // namespace gh
