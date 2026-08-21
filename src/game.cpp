#include "game.hpp"
#include "dynamic_obstacles.hpp"
#include "match_rules.hpp"
#include "obstacles.hpp"
#include "physics.hpp"
#include "powerups.hpp"
#include "toy_effects.hpp"
#include "toy_behavior.hpp"

#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <format>
#include <functional>
#include <limits>

namespace gh {
namespace {

constexpr COLORREF kCream = RGB(255, 255, 204);
constexpr COLORREF kGreen = RGB(102, 255, 102);
constexpr COLORREF kBlue = RGB(102, 153, 255);
constexpr std::size_t kPlayableToyCount = 12;
bool pressed(const InputState& input, int key) {
    return key >= 0 && key < static_cast<int>(input.pressed.size()) && input.pressed[key];
}

bool any_pressed(const InputState& input) {
    return std::any_of(input.pressed.begin(), input.pressed.end(), [](bool value) {
        return value;
    });
}

int board_grid_width(const Rectangle& stage) {
    return std::max(1, (stage.right - stage.left) / 6);
}

int board_grid_height(const Rectangle& stage) {
    return std::max(1, (stage.bottom - stage.top) / 6);
}

bool valid_rectangle(const Rectangle& rectangle) {
    return rectangle.right > rectangle.left && rectangle.bottom > rectangle.top;
}

int board_lane(float y, const Rectangle& stage) {
    const float row = (y - static_cast<float>(stage.top)) /
                      static_cast<float>(board_grid_height(stage));
    return std::clamp(static_cast<int>(std::lround(row)) - 1, 0, 4);
}

float board_lane_y(int lane, const Rectangle& stage) {
    return static_cast<float>(
        stage.top + (std::clamp(lane, 0, 4) + 1) * board_grid_height(stage)
    );
}

float move_release(float y, int direction, const Rectangle& stage) {
    return board_lane_y(board_lane(y, stage) + direction, stage);
}

bool starts_with(const std::wstring& value, const std::wstring& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::wstring wide_ascii(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::filesystem::path application_data_directory() {
    std::array<wchar_t, MAX_PATH> local_app_data{};
    if (SUCCEEDED(SHGetFolderPathW(
            nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
            SHGFP_TYPE_CURRENT, local_app_data.data()
        ))) {
        return std::filesystem::path(local_app_data.data()) / L"Gearheads Native";
    }
    return {};
}

bool boxes_overlap(const ToyBox& left, const ToyBox& right) {
    return std::min(left.right, right.right) > std::max(left.left, right.left) &&
           std::min(left.bottom, right.bottom) > std::max(left.top, right.top);
}

bool is_interactive(const ScreenCommand& command) {
    if (starts_with(command.key, L"utext") || command.key == L"udefault") return true;
    if (command.key != L"button") return false;
    return command.arguments.size() <= 4 ||
           !starts_with(command.arguments[4], L"~demotoys");
}

bool is_text_command(const ScreenCommand& command) {
    return starts_with(command.key, L"dtext") || starts_with(command.key, L"utext") ||
           command.key == L"udefault" || starts_with(command.key, L"dt") ||
           starts_with(command.key, L"db") || command.key == L"dq";
}

int number(const std::wstring& value, int source_line) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed, 10);
        if (consumed != value.size()) throw std::invalid_argument("trailing data");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(
            "Expected an integer in screens.ini line " + std::to_string(source_line)
        );
    }
}

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::wstring command_action(const ScreenCommand& command) {
    if (command.key == L"button") {
        return command.arguments.size() > 4 ? command.arguments[4] : L"";
    }
    if ((starts_with(command.key, L"utext") || command.key == L"udefault") &&
        command.arguments.size() > 8) {
        return command.arguments.back();
    }
    return L"";
}

bool decode_type_action(
    const std::wstring& action,
    int& player,
    int& definition,
    int maximum_definition
) {
    for (const std::wstring_view prefix : {L"~whattoys", L"~demotoys"}) {
        if (!starts_with(action, std::wstring(prefix)) ||
            action.size() < prefix.size() + 3 || action[prefix.size() + 1] != L'#') {
            continue;
        }
        player = static_cast<int>(action[prefix.size()] - L'1');
        if (player < 0 || player > 1) return false;
        try {
            const int original_type = std::stoi(action.substr(prefix.size() + 2));
            definition = original_type - 15;
        } catch (const std::exception&) {
            return false;
        }
        return definition >= 0 && definition < maximum_definition;
    }
    return false;
}

bool decode_toybox_action(const std::wstring& action, int& player, int& definition) {
    return decode_type_action(action, player, definition, 12);
}

std::vector<std::wstring> words(const std::wstring& value) {
    std::vector<std::wstring> result;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        while (cursor < value.size() && iswspace(value[cursor])) ++cursor;
        const std::size_t start = cursor;
        while (cursor < value.size() && !iswspace(value[cursor])) ++cursor;
        if (cursor > start) result.push_back(value.substr(start, cursor - start));
    }
    return result;
}

std::vector<int> tournament_roster(std::wstring_view encoded, std::mt19937& random) {
    std::vector<int> result;
    std::vector<int> unused(kPlayableToyCount);
    for (std::size_t index = 0; index < unused.size(); ++index) {
        unused[index] = static_cast<int>(index);
    }
    for (const wchar_t character : encoded) {
        if (character >= L'A' && character < L'A' + static_cast<wchar_t>(kPlayableToyCount)) {
            const int definition = character - L'A';
            if (std::find(result.begin(), result.end(), definition) == result.end()) {
                result.push_back(definition);
                std::erase(unused, definition);
            }
        } else if (character == L'?' && !unused.empty()) {
            std::uniform_int_distribution<std::size_t> distribution(0, unused.size() - 1);
            const std::size_t slot = distribution(random);
            result.push_back(unused[slot]);
            unused.erase(unused.begin() + static_cast<std::ptrdiff_t>(slot));
        }
    }
    return result;
}

int description_toy(const ScreenCommand& command) {
    if (command.arguments.size() >= 3) {
        try {
            const int association = std::stoi(command.arguments[2]);
            if (association >= 115 && association <= 132) return association - 115;
        } catch (const std::exception&) {
        }
    }
    const std::wstring& key = command.key;
    if (!starts_with(key, L"db") || key.size() < 4 || !iswdigit(key[2]) || !iswdigit(key[3])) {
        return -1;
    }
    return static_cast<int>((key[2] - L'0') * 10 + (key[3] - L'0')) - 15;
}

int total_animation_ticks(const std::vector<AnimationFrame>& animation) {
    int total = 0;
    for (const AnimationFrame& frame : animation) total += frame.ticks;
    return total;
}

int rounded_muldiv(int value, int multiplier, int divisor) {
    if (divisor == 0) return 0;
    const long long product = static_cast<long long>(value) * multiplier;
    const long long half = std::abs(divisor) / 2;
    if (product >= 0) return static_cast<int>((product + half) / divisor);
    return -static_cast<int>((-product + half) / divisor);
}

std::filesystem::path animation_frame(
    const std::vector<AnimationFrame>& animation,
    int animation_ticks,
    bool loop
) {
    if (animation.empty()) return {};
    const int total = total_animation_ticks(animation);
    if (total <= 0) return animation.front().image;
    int tick = loop ? animation_ticks % total : std::min(animation_ticks, total - 1);
    for (const AnimationFrame& frame : animation) {
        if (tick < frame.ticks) return frame.image;
        tick -= frame.ticks;
    }
    return animation.back().image;
}

}  // namespace

Game::Game(std::filesystem::path asset_root, HWND notification_window)
    : asset_root_(std::move(asset_root)),
      audio_(asset_root_, notification_window, application_data_directory()),
      defaults_(asset_root_ / "data/runtime-defaults.ini"),
      images_(asset_root_),
      screens_(asset_root_ / "data/screens.ini"),
      levels_(asset_root_ / "data/gearhead.ini"),
      scripts_(asset_root_ / "data/script.ini"),
      attracts_(asset_root_ / "data/anim.dat"),
      champions_(
          asset_root_ / "data/gearhead.ini", application_data_directory() / L"champions.dat"
      ) {
    const auto sound = [this](const std::wstring& section, int ordinal) {
        const std::filesystem::path result = scripts_.sound(section, ordinal);
        if (result.empty()) throw std::runtime_error("Missing recovered script sound");
        return result;
    };
    definitions_ = {
        {L"Ziggy", L"roach", "sprites/zg/zg_wh01.png", sound(L"roach", 1), defaults_.toy("roach")},
        {L"Walking Timebomb", L"bomby", "sprites/tb/tb_wh01.png", sound(L"bomby", 1), defaults_.toy("bomby")},
        {L"Clucketta", L"cluck", "sprites/ck/ck_wh01.png", sound(L"cluck", 1), defaults_.toy("cluck")},
        {L"Zap\u2013bot", L"zappa", "sprites/zb/zb_wh01.png", sound(L"zappa", 1), defaults_.toy("zappa")},
        {L"Kangaruffian", L"kanga", "sprites/kg/kg_wh01.png", sound(L"kanga", 1), defaults_.toy("kanga")},
        {L"Big Al", L"bigal", "sprites/ba/ba_wh01.png", sound(L"bigal", 1), defaults_.toy("bigal")},
        {L"Disasteroid", L"destr", "sprites/ds/ds_wh01.png", sound(L"destr", 1), defaults_.toy("destr")},
        {L"Presto", L"stick", "sprites/mm/mm_wh01.png", sound(L"stick", 1), defaults_.toy("stick")},
        {L"Krush Kringle", L"goril", "sprites/kk/kk_wh01.png", sound(L"goril", 1), defaults_.toy("goril")},
        {L"Deadhead", L"skull", "sprites/dh/dh_wh01.png", sound(L"skull", 1), defaults_.toy("skull")},
        {L"Orbit", L"magnt", "sprites/sq/sq_wh01.png", sound(L"magnt", 1), defaults_.toy("magnt")},
        {L"Handy", L"handy", "sprites/ha/ha_wh01.png", sound(L"handy", 1), defaults_.toy("handy")},
        {L"Small Fry", L"small", "sprites/sm/sm_wh01.png", sound(L"small", 2), defaults_.toy("small")},
        {L"Rocket", L"roket", "sprites/digit/roc1w01.png", sound(L"roket", 2), defaults_.toy("roket")},
    };
    max_live_objects_ = defaults_.scalar("Maxtoys");
    AddFontResourceExW((asset_root_ / "fonts/gear.ttf").c_str(), FR_PRIVATE, nullptr);
    // Follow the shipped startup chain: Philips -> R/GA -> scripted toy demo
    // -> Gearheads logo -> main menu.  Each transition is driven by the
    // recovered screen timeout/any-key records.
    enter_frontend(L"phillips");
    play_music("music/open.mid");
}

Game::~Game() {
    stop_music();
    RemoveFontResourceExW((asset_root_ / "fonts/gear.ttf").c_str(), FR_PRIVATE, nullptr);
}

void Game::update(double seconds, const InputState& input) {
    audio_notice_seconds_ = std::max(0.0, audio_notice_seconds_ - std::max(0.0, seconds));
    switch (screen_) {
        case Screen::Frontend: update_frontend(seconds, input); break;
        case Screen::Duel: update_duel(seconds, input); break;
    }
}

std::vector<const ScreenCommand*> Game::interactive_commands() const {
    std::vector<const ScreenCommand*> result;
    const ScreenDefinition* definition = screens_.find(frontend_name_);
    if (definition == nullptr) return result;
    for (const ScreenCommand& command : definition->commands) {
        if (is_interactive(command) && !command_action(command).empty()) {
            result.push_back(&command);
        }
    }
    return result;
}

void Game::enter_frontend(const std::wstring& name) {
    const ScreenDefinition* definition = screens_.find(name);
    if (definition == nullptr) return;
    const bool returning_from_duel = screen_ == Screen::Duel;
    frontend_name_ = lower(name);
    screen_ = Screen::Frontend;
    frontend_elapsed_ = 0.0;
    if (frontend_name_ == L"9p1_toys" || frontend_name_ == L"9dualtoys") {
        // Both original first-pick screens run the >emptyboxes setup routine.
        toyboxes_[0].clear();
        toyboxes_[1].clear();
    }
    menu_item_ = 0;
    const auto interactive = interactive_commands();
    for (std::size_t index = 0; index < interactive.size(); ++index) {
        if (interactive[index]->key == L"udefault") {
            menu_item_ = static_cast<int>(index);
            break;
        }
    }
    if (frontend_name_ == L"reallyquit") {
        wants_quit_ = true;
    } else if (frontend_name_ == L"docompvshume") {
        tournament_active_ = false;
        start_duel(DuelMode::HumanVsComputer);
    } else if (frontend_name_ == L"dohumevshume") {
        tournament_active_ = false;
        start_duel(DuelMode::HumanVsHuman);
    } else if (frontend_name_ == L"n1_101") {
        begin_tournament(101);
    } else if (frontend_name_ == L"n1_413") {
        begin_tournament(413);
    } else if (frontend_name_ == L"n1_1025") {
        begin_tournament(1025);
    } else if (frontend_name_ == L"n1_start") {
        if (tournament_active_ && toyboxes_[1].empty()) {
            toybox_size_ = 4;
            randomize_toybox(1);
        }
    } else if (frontend_name_ == L"do_n1") {
        start_tournament_match();
    } else if (frontend_name_ == L"attract" || frontend_name_ == L"longdemo" ||
               frontend_name_ == L"longdemo2" || frontend_name_ == L"anime") {
        start_attract(frontend_name_);
    } else if (frontend_name_ == L"n1_victory") {
        entered_name_.clear();
    } else if (frontend_name_ == L"main") {
        tournament_active_ = false;
    }
    if (screen_ == Screen::Frontend) {
        for (const ScreenCommand& command : definition->commands) {
            if (command.key == L"routine" && !command.arguments.empty() &&
                starts_with(command.arguments.front(), L">sound$")) {
                activate_frontend(command.arguments.front());
            }
        }
        if (frontend_name_ == L"gearlogo") play_sound_alias(L"logosound");
        if (returning_from_duel) play_music("music/open.mid");
    }
}

void Game::activate_frontend(const std::wstring& action) {
    if (action.empty()) return;
    if (action.front() == L'@') {
        enter_frontend(action.substr(1));
        return;
    }
    if (starts_with(action, L">sound$") && action.size() > 7) {
        play_sound_alias(action.substr(7));
        return;
    }
    if (starts_with(action, L">setlevel$") && action.size() > 10) {
        wchar_t theme = towupper(action.back());
        if (theme == L'R') {
            static constexpr std::array<wchar_t, 4> themes{L'K', L'G', L'P', L'F'};
            std::uniform_int_distribution<int> distribution(0, static_cast<int>(themes.size()) - 1);
            theme = themes[static_cast<std::size_t>(distribution(random_))];
        }
        selected_level_theme_ = theme;
        return;
    }
    if (starts_with(action, L">numtoys#")) {
        try {
            toybox_size_ = std::clamp(std::stoi(action.substr(9)), 1, 12);
        } catch (const std::exception&) {
            toybox_size_ = 12;
        }
        toyboxes_[0].clear();
        toyboxes_[1].clear();
        return;
    }
    if (starts_with(action, L">powerup#") && action.size() > 9) {
        powerups_enabled_ = action.back() != L'0';
        return;
    }
    if (starts_with(action, L">setai#")) {
        try {
            ai_difficulty_ = std::clamp(std::stoi(action.substr(7)), 1, 9);
        } catch (const std::exception&) {
            ai_difficulty_ = 1;
        }
        return;
    }
    int player = 0;
    int definition = 0;
    if (decode_toybox_action(action, player, definition)) {
        auto& toybox = toyboxes_[static_cast<std::size_t>(player)];
        const auto found = std::find(toybox.begin(), toybox.end(), definition);
        if (found != toybox.end()) {
            toybox.erase(found);
        } else if (static_cast<int>(toybox.size()) < toybox_size_) {
            toybox.push_back(definition);
            selected_toy_[static_cast<std::size_t>(player)] = definition;
        }
        return;
    }
    if (starts_with(action, L"~randombox#") && !action.empty()) {
        const int random_player = static_cast<int>(action.back() - L'1');
        if (random_player >= 0 && random_player <= 1) randomize_toybox(random_player);
    }
}

void Game::update_frontend(double seconds, const InputState& input) {
    frontend_elapsed_ += std::max(0.0, seconds);
    const ScreenDefinition* definition = screens_.find(frontend_name_);
    if (frontend_name_ == L"n1_victory") {
        if (pressed(input, VK_BACK) && !entered_name_.empty()) entered_name_.pop_back();
        for (int key = 'A'; key <= 'Z' && entered_name_.size() < 15; ++key) {
            if (pressed(input, key)) entered_name_.push_back(static_cast<wchar_t>(key));
        }
        for (int key = '0'; key <= '9' && entered_name_.size() < 15; ++key) {
            if (pressed(input, key)) entered_name_.push_back(static_cast<wchar_t>(key));
        }
        if (pressed(input, VK_SPACE) && !entered_name_.empty() && entered_name_.size() < 15 &&
            entered_name_.back() != L' ') {
            entered_name_.push_back(L' ');
        }
        if (pressed(input, VK_RETURN) && !entered_name_.empty()) {
            champions_.insert(tournament_.total_score, entered_name_);
            champions_.save();
            enter_frontend(L"n1_winners");
        }
        return;
    }
    const bool staff_navigation = frontend_name_ == L"staff" &&
                                  (pressed(input, VK_UP) || pressed(input, VK_DOWN) ||
                                   pressed(input, VK_LEFT) || pressed(input, VK_RIGHT));
    if (definition != nullptr && any_pressed(input) && !staff_navigation) {
        for (const ScreenCommand& command : definition->commands) {
            if (command.key == L"anykey" && !command.arguments.empty()) {
                activate_frontend(command.arguments.front());
                return;
            }
        }
    }
    if (definition != nullptr) {
        for (const ScreenCommand& command : definition->commands) {
            if (command.key != L"timeout" || command.arguments.size() < 2) continue;
            const int timeout_seconds = number(command.arguments[0], command.source_line);
            if (frontend_elapsed_ >= static_cast<double>(timeout_seconds)) {
                activate_frontend(command.arguments[1]);
                return;
            }
        }
    }

    const auto interactive = interactive_commands();
    if (!interactive.empty()) {
        const int count = static_cast<int>(interactive.size());
        if (pressed(input, VK_UP) || pressed(input, VK_LEFT)) {
            menu_item_ = (menu_item_ + count - 1) % count;
        }
        if (pressed(input, VK_DOWN) || pressed(input, VK_RIGHT)) {
            menu_item_ = (menu_item_ + 1) % count;
        }
    }

    if (pressed(input, VK_ESCAPE) || pressed(input, VK_BACK)) {
        if (definition != nullptr) {
            for (const ScreenCommand& command : definition->commands) {
                if ((command.key == L"prev" || command.key == L"next") &&
                    !command.arguments.empty()) {
                    activate_frontend(command.arguments.front());
                    return;
                }
            }
        }
        enter_frontend(L"main");
        return;
    }

    if (!pressed(input, VK_RETURN) && !pressed(input, VK_SPACE)) return;
    play_sound_alias(L"oksound");
    if (!interactive.empty()) {
        activate_frontend(command_action(*interactive[static_cast<std::size_t>(menu_item_)]));
        return;
    }
    if (definition == nullptr) return;
    for (const ScreenCommand& command : definition->commands) {
        if (command.key == L"next" && !command.arguments.empty()) {
            activate_frontend(command.arguments.front());
            return;
        }
    }
}

void Game::begin_tournament(int encoded_start) {
    const auto start = decode_original_tournament_start(encoded_start);
    if (!start.has_value()) {
        enter_frontend(L"n1_option");
        return;
    }
    tournament_active_ = true;
    tournament_ = {
        start->level,
        3,
        0,
        start->multiplier,
        std::max(tournament_unlocked_level_, start->level),
    };
    tournament_unlocked_level_ = tournament_.unlocked_level;
    tournament_saved_human_box_.clear();
    tournament_human_box_overridden_ = false;
    last_tournament_points_ = 0;
    toybox_size_ = 4;
    toyboxes_[0].clear();
    toyboxes_[1].clear();
    randomize_toybox(1);
    enter_frontend(L"n1_start");
}

void Game::prepare_tournament_rosters(const LevelDefinition& level) {
    const bool bonus = original_tournament_bonus_level(tournament_.current_level);
    if (bonus && !tournament_human_box_overridden_) {
        tournament_saved_human_box_ = toyboxes_[1];
        tournament_human_box_overridden_ = true;
    } else if (!bonus && tournament_human_box_overridden_) {
        toyboxes_[1] = tournament_saved_human_box_;
        tournament_saved_human_box_.clear();
        tournament_human_box_overridden_ = false;
    }

    for (const std::wstring& token : words(level.level_toys)) {
        if (token.size() < 2 || (token.front() != L'0' && token.front() != L'1')) continue;
        // The tournament data's box 1 is the computer roster. Box 0, when
        // present on bonus and late levels, overrides the human's right box.
        const int player = token.front() == L'1' ? 0 : 1;
        std::vector<int> roster = tournament_roster(
            std::wstring_view(token).substr(1), random_
        );
        if (!roster.empty()) toyboxes_[static_cast<std::size_t>(player)] = std::move(roster);
    }
    const int previous_size = toybox_size_;
    toybox_size_ = 4;
    if (toyboxes_[0].empty()) randomize_toybox(0);
    if (toyboxes_[1].empty()) randomize_toybox(1);
    toybox_size_ = previous_size;
    ai_difficulty_ = std::clamp(level.tournament_intelligence, 1, 9);
}

void Game::start_tournament_match() {
    if (!tournament_active_) {
        enter_frontend(L"n1_option");
        return;
    }
    const LevelDefinition* level = levels_.find_tournament(tournament_.current_level);
    if (level == nullptr) {
        enter_frontend(L"n1_gameover");
        return;
    }
    prepare_tournament_rosters(*level);
    start_duel(DuelMode::Tournament, level);
}

void Game::start_attract(const std::wstring& origin_screen) {
    attract_anykey_target_ = L"main";
    attract_timeout_target_ = L"main";
    attract_match_target_ = L"main";
    attract_timeout_seconds_ = origin_screen == L"anime" ? 30.0 : 60.0;
    if (const ScreenDefinition* definition = screens_.find(origin_screen); definition != nullptr) {
        for (const ScreenCommand& command : definition->commands) {
            if (command.key == L"anykey" && !command.arguments.empty()) {
                attract_anykey_target_ = command.arguments.front();
            } else if (command.key == L"next" && !command.arguments.empty()) {
                attract_match_target_ = command.arguments.front();
            } else if (command.key == L"timeout" && command.arguments.size() >= 2 &&
                       origin_screen != L"anime") {
                attract_timeout_seconds_ = static_cast<double>(
                    number(command.arguments[0], command.source_line)
                );
                attract_timeout_target_ = command.arguments[1];
            }
        }
    }
    static constexpr std::array<wchar_t, 4> themes{L'K', L'G', L'P', L'F'};
    std::uniform_int_distribution<std::size_t> distribution(0, themes.size() - 1);
    selected_level_theme_ = themes[distribution(random_)];
    toybox_size_ = 12;
    toyboxes_[0].clear();
    toyboxes_[1].clear();
    randomize_toybox(0);
    randomize_toybox(1);
    ai_difficulty_ = 7;
    start_duel(DuelMode::Attract);
    if (origin_screen == L"anime" && !attracts_.sequences().empty()) {
        std::uniform_int_distribution<std::size_t> sequence_distribution(
            0, attracts_.sequences().size() - 1
        );
        active_attract_ = &attracts_.sequences()[sequence_distribution(random_)];
        attract_event_ = 0;
        scripted_attract_ = true;
        computer_controlled_ = {false, false};
        attract_timeout_seconds_ =
            static_cast<double>(active_attract_->events.back().timestamp_ms) / 1000.0 + 2.0;
        attract_timeout_target_ = L"@gearlogo";
    }
}

void Game::start_duel(DuelMode mode, const LevelDefinition* level) {
    duel_mode_ = mode;
    active_attract_ = nullptr;
    attract_event_ = 0;
    scripted_attract_ = false;
    computer_controlled_ = mode == DuelMode::HumanVsHuman
                               ? std::array<bool, 2>{false, false}
                               : mode == DuelMode::Attract
                                     ? std::array<bool, 2>{true, true}
                                     : std::array<bool, 2>{true, false};
    toys_.clear();
    docked_rocket_id_.reset();
    previous_toy_contacts_.clear();
    scheduled_sounds_.clear();
    dynamic_obstacles_.clear();
    powerup_.reset();
    powerup_effect_ = {0, 0};
    powerup_effect_ticks_ = {0, 0};
    last_powerup_owner_ = -1;
    score_ = {0, 0};
    for (int player = 0; player < 2; ++player) {
        if (toyboxes_[static_cast<std::size_t>(player)].empty()) randomize_toybox(player);
        selected_toy_[static_cast<std::size_t>(player)] =
            toyboxes_[static_cast<std::size_t>(player)].front();
    }
    release_y_ = {230.0F, 230.0F};
    for (ComputerAiCadence& cadence : ai_cadence_) cadence.reset();
    for (LaunchGauge& gauge : launch_gauges_) {
        gauge.begin_match(defaults_.scalar("GaugeTime"));
    }
    duel_elapsed_ = 0.0;
    duel_countdown_seconds_ = mode == DuelMode::Attract ? 0.0 : 3.0;
    simulation_accumulator_ = 0.0;
    match_winner_ = kNoMatchWinner;
    active_level_ = level != nullptr ? level : levels_.first_for_theme(selected_level_theme_);
    if (active_level_ != nullptr) {
        stage_ = active_level_->stage;
        release_y_ = {board_lane_y(2, stage_), board_lane_y(2, stage_)};
        background_ = active_level_->background;
        friction_ = active_level_->friction;
        ice_ = active_level_->ice;
        for (const ObstacleDefinition& obstacle : active_level_->obstacles) {
            const auto archetype = obstacle_archetype(obstacle.code);
            if (!archetype.has_value() ||
                archetype->interaction != ObstacleInteraction::Dynamic) {
                continue;
            }
            const ToyParameters parameters = defaults_.toy(archetype->parameters);
            const int direction = obstacle.facing_right ? 1 : -1;
            const int velocity_x16 = direction * parameters.horizontal_speed() * 4;
            dynamic_obstacles_.push_back({
                obstacle.code,
                obstacle.facing_right,
                static_cast<float>(obstacle.x),
                static_cast<float>(obstacle.y),
                velocity_x16,
                0,
                velocity_x16,
                0,
                obstacle.facing_right ? 0 : 32,
                0,
            });
        }
    }
    screen_ = Screen::Duel;
    if (active_level_ != nullptr && !active_level_->music.empty()) {
        std::uniform_int_distribution<std::size_t> distribution(0, active_level_->music.size() - 1);
        play_music(active_level_->music[distribution(random_)]);
    } else {
        play_music("music/ktn_med.mid");
    }
    if (duel_countdown_seconds_ > 0.0) {
        play_sound_alias(L"threetwoone");
    }
}

void Game::randomize_toybox(int player) {
    auto& toybox = toyboxes_[static_cast<std::size_t>(player)];
    toybox.resize(kPlayableToyCount);
    for (std::size_t index = 0; index < toybox.size(); ++index) {
        toybox[index] = static_cast<int>(index);
    }
    std::shuffle(toybox.begin(), toybox.end(), random_);
    toybox.resize(static_cast<std::size_t>(toybox_size_));
    selected_toy_[static_cast<std::size_t>(player)] = toybox.front();
}

void Game::cycle_selected_toy(int player, int direction) {
    const auto& toybox = toyboxes_[static_cast<std::size_t>(player)];
    if (toybox.empty()) return;
    std::array<bool, kPlayableToyCount> available{};
    for (const int definition : toybox) {
        if (definition >= 0 && definition < static_cast<int>(available.size())) {
            available[static_cast<std::size_t>(definition)] = true;
        }
    }
    const int previous = selected_toy_[static_cast<std::size_t>(player)];
    selected_toy_[static_cast<std::size_t>(player)] = original_next_available_toy(
        selected_toy_[static_cast<std::size_t>(player)], direction, available
    );
    if (selected_toy_[static_cast<std::size_t>(player)] != previous) {
        play_sound(scripts_.sound(L"arrow", 1));
    }
    if (defaults_.scalar("Resetimeonpick") != 0) {
        launch_gauges_[static_cast<std::size_t>(player)].reset_after_launch(
            defaults_.scalar("DecayTime")
        );
    }
}

bool Game::release_toy(int player) {
    // Maxtoys is the original global live-object ceiling (segment 10:08e1 and
    // 24b0), independent of the 1-12 toybox-size roster option.
    if (player < 0 || player > 1) return false;
    if (docked_rocket_id_.has_value()) {
        Toy* rocket = find_toy(*docked_rocket_id_);
        if (rocket == nullptr) {
            docked_rocket_id_.reset();
        } else if (rocket->owner == player && rocket->rocket_docked) {
            const int direction = player == 0 ? 1 : -1;
            rocket->rocket_docked = false;
            rocket->collision_layer = 0;
            rocket->velocity_x16 = direction *
                definitions_[rocket->definition].parameters.horizontal_speed() * 4;
            rocket->desired_x16 = rocket->velocity_x16;
            set_animation_state(*rocket, kAnimationWalk);
            play_sound(scripts_.sound(L"roket", 2));
            docked_rocket_id_.reset();
            return true;
        }
    }
    if (static_cast<int>(toys_.size()) >= max_live_objects_) return false;
    const PowerupKind effect = static_cast<PowerupKind>(
        powerup_effect_[static_cast<std::size_t>(player)]
    );
    if (original_powerup_blocks_release(effect)) {
        play_sound(scripts_.sound(L"puup", 2));
        return false;
    }

    const int gauge_time = defaults_.scalar("GaugeTime");
    const int decay_time = defaults_.scalar("DecayTime");
    LaunchGauge& gauge = launch_gauges_[static_cast<std::size_t>(player)];
    if (!gauge.ready(decay_time, gauge_time)) {
        play_sound(scripts_.sound(L"arrow", 2));
        return false;
    }
    const int winding = gauge.winding(gauge_time);

    const int selected_lane = board_lane(
        release_y_[static_cast<std::size_t>(player)], stage_
    );
    std::vector<float> rows{release_y_[static_cast<std::size_t>(player)]};
    for (const int lane : original_powerup_extra_lanes(effect, selected_lane, 5)) {
        rows.push_back(board_lane_y(lane, stage_));
    }
    const float start_x = original_powerup_starts_at_center(effect)
                              ? static_cast<float>(stage_.left + stage_.right) / 2.0F
                              : (player == 0
                                     ? static_cast<float>(stage_.left + board_grid_width(stage_) / 2)
                                     : static_cast<float>(stage_.right - board_grid_width(stage_) / 2));
    bool first = true;
    bool launched = false;
    for (const float row : rows) {
        if (!spawn_toy(selected_toy_[player], player, start_x, row, first, winding)) break;
        launched = true;
        first = false;
    }
    if (launched) gauge.reset_after_launch(decay_time);
    return launched;
}

bool Game::spawn_toy(
    int definition_index,
    int owner,
    float x,
    float y,
    bool launch_sound,
    int winding
) {
    if (definition_index < 0 || definition_index >= static_cast<int>(definitions_.size()) ||
        owner < 0 || owner > 1 || static_cast<int>(toys_.size()) >= max_live_objects_) {
        return false;
    }
    const auto& definition = definitions_[static_cast<std::size_t>(definition_index)];
    const int direction = owner == 0 ? 1 : -1;
    Toy toy;
    toy.definition = definition_index;
    toy.owner = owner;
    toy.x = x;
    toy.y = y;
    // GEAR_EN segment 10:18fb-1911 constructs the horizontal velocity as
    // direction * type.horizontal_speed * 4 in 1/16-pixel units.  The native
    // model stores pixels per original simulation tick, hence speed / 4.
    toy.velocity_x16 = direction * definition.parameters.horizontal_speed() * 4;
    toy.heading = owner == 0 ? 0 : 32;
    if (definition.parameters.movement_mode() == 1) {
        const float stage_middle = static_cast<float>(stage_.top + stage_.bottom) / 2.0F;
        toy.velocity_y16 = std::abs(toy.velocity_x16);
        if (toy.y > stage_middle) toy.velocity_y16 = -toy.velocity_y16;
        if (toy.velocity_x16 >= 0) {
            toy.heading = toy.velocity_y16 >= 0 ? 8 : 56;
        } else {
            toy.heading = toy.velocity_y16 >= 0 ? 24 : 40;
        }
    }
    toy.winding = winding >= 0 ? winding : defaults_.scalar("GaugeTime");
    toy.id = next_toy_id_++;
    toys_.push_back(toy);
    if (launch_sound) play_sound(definition.launch_sound);
    return true;
}

void Game::update_duel(double seconds, const InputState& input) {
    duel_elapsed_ += std::max(0.0, seconds);
    if (duel_mode_ == DuelMode::Attract && any_pressed(input)) {
        stop_music();
        activate_frontend(attract_anykey_target_);
        return;
    }
    if (duel_mode_ == DuelMode::Attract && duel_elapsed_ >= attract_timeout_seconds_) {
        stop_music();
        activate_frontend(attract_timeout_target_);
        return;
    }
    if (duel_mode_ != DuelMode::Attract && pressed(input, VK_ESCAPE)) {
        enter_frontend(L"main");
        return;
    }
    if (duel_countdown_seconds_ > 0.0) {
        const int before = static_cast<int>(std::ceil(duel_countdown_seconds_));
        duel_countdown_seconds_ = std::max(0.0, duel_countdown_seconds_ - std::max(0.0, seconds));
        const int after = static_cast<int>(std::ceil(duel_countdown_seconds_));
        if (after > 0 && after < before) play_sound_alias(L"threetwoone");
        if (before > 0 && after == 0) play_sound_alias(L"gosound");
        return;
    }
    for (LaunchGauge& gauge : launch_gauges_) {
        gauge.advance(seconds, defaults_.scalar("GaugeTime"));
    }

    if (scripted_attract_ && active_attract_ != nullptr) {
        const int elapsed_ms = static_cast<int>(duel_elapsed_ * 1000.0);
        while (attract_event_ < active_attract_->events.size() &&
               active_attract_->events[attract_event_].timestamp_ms <= elapsed_ms) {
            const AttractEvent& event = active_attract_->events[attract_event_++];
            const int player = event.player;
            const float start_x = player == 0
                                      ? static_cast<float>(
                                            stage_.left + board_grid_width(stage_) / 2
                                        )
                                      : static_cast<float>(
                                            stage_.right - board_grid_width(stage_) / 2
                                        );
            for (std::size_t lane = 0; lane < event.lanes.size(); ++lane) {
                const AttractLane& command = event.lanes[lane];
                if (command.winding <= 0) continue;
                if (spawn_toy(
                        command.definition,
                        player,
                        start_x,
                        board_lane_y(static_cast<int>(lane), stage_),
                        false
                    )) {
                    toys_.back().winding = command.winding;
                }
            }
        }
    }

    if (!computer_controlled_[0]) {
        if (pressed(input, 'W')) {
            const float previous = release_y_[0];
            release_y_[0] = move_release(release_y_[0], -1, stage_);
            if (release_y_[0] != previous) play_sound(scripts_.sound(L"arrow", 1));
        }
        if (pressed(input, 'S')) {
            const float previous = release_y_[0];
            release_y_[0] = move_release(release_y_[0], 1, stage_);
            if (release_y_[0] != previous) play_sound(scripts_.sound(L"arrow", 1));
        }
        if (pressed(input, 'A')) cycle_selected_toy(0, -1);
        if (pressed(input, 'D')) cycle_selected_toy(0, 1);
        if (pressed(input, 'F')) release_toy(0);
    }
    if (!computer_controlled_[1]) {
        if (pressed(input, VK_UP)) {
            const float previous = release_y_[1];
            release_y_[1] = move_release(release_y_[1], -1, stage_);
            if (release_y_[1] != previous) play_sound(scripts_.sound(L"arrow", 1));
        }
        if (pressed(input, VK_DOWN)) {
            const float previous = release_y_[1];
            release_y_[1] = move_release(release_y_[1], 1, stage_);
            if (release_y_[1] != previous) play_sound(scripts_.sound(L"arrow", 1));
        }
        if (pressed(input, VK_LEFT)) cycle_selected_toy(1, -1);
        if (pressed(input, VK_RIGHT)) cycle_selected_toy(1, 1);
        if (pressed(input, VK_RETURN) || pressed(input, VK_SPACE)) release_toy(1);
    }

    for (int player = 0; player < 2; ++player) {
        if (!computer_controlled_[static_cast<std::size_t>(player)]) continue;
        const auto& box = toyboxes_[static_cast<std::size_t>(player)];
        if (box.empty()) continue;

        std::uniform_int_distribution<int> tenth_distribution(0, 9);
        std::uniform_int_distribution<int> step_distribution(-1, 1);
        const ComputerCadenceDecision decision =
            ai_cadence_[static_cast<std::size_t>(player)].update(
                ai_difficulty_,
                tenth_distribution(random_),
                launch_gauges_[static_cast<std::size_t>(player)].winding(
                    defaults_.scalar("GaugeTime")
                ),
                defaults_.scalar("DecayTime"),
                static_cast<std::uint32_t>(std::max(0.0, duel_elapsed_) * 1000.0)
            );

        if (decision.cursor_step_due) {
            const int cursor_step = step_distribution(random_);
            release_y_[static_cast<std::size_t>(player)] = move_release(
                release_y_[static_cast<std::size_t>(player)],
                cursor_step,
                stage_
            );
        }
        if (!decision.attempt_launch) continue;

        if (decision.tier != ComputerTier::Low) {
            // The recovered medium/high routines choose a lane from their
            // per-type board analysis before entering the common release path.
            // Prefer the lane containing the nearest opposing threat; an empty
            // opening board retains the visible cursor lane.
            const int current_lane = board_lane(
                release_y_[static_cast<std::size_t>(player)], stage_
            );
            int target_lane = current_lane;
            float nearest_goal_distance = std::numeric_limits<float>::max();
            for (const Toy& toy : toys_) {
                if (!toy.active || toy.owner == player) continue;
                const float distance = player == 0
                                           ? toy.x - static_cast<float>(stage_.left)
                                           : static_cast<float>(stage_.right) - toy.x;
                if (distance < nearest_goal_distance) {
                    nearest_goal_distance = distance;
                    target_lane = board_lane(toy.y, stage_);
                }
            }
            release_y_[static_cast<std::size_t>(player)] = board_lane_y(target_lane, stage_);
        }

        if (!release_toy(player)) continue;
        if (decision.tier == ComputerTier::Low) {
            cycle_selected_toy(player, step_distribution(random_));
        } else {
            std::uniform_int_distribution<std::size_t> toy_distribution(0, box.size() - 1);
            const int next = box[toy_distribution(random_)];
            if (next != selected_toy_[static_cast<std::size_t>(player)]) {
                selected_toy_[static_cast<std::size_t>(player)] = next;
                launch_gauges_[static_cast<std::size_t>(player)].reset_after_launch(
                    defaults_.scalar("DecayTime")
                );
            }
        }
    }

    simulation_accumulator_ += std::min(seconds, 0.25);
    const double tick_seconds = static_cast<double>(defaults_.scalar("FrameStep")) / 1000.0;
    while (simulation_accumulator_ >= tick_seconds) {
        advance_duel_tick();
        simulation_accumulator_ -= tick_seconds;
        if (match_winner_ != kNoMatchWinner) {
            finish_duel();
            return;
        }
    }
}

void Game::finish_tournament_match() {
    const bool human_won = match_winner_ == 1;
    const bool was_bonus = original_tournament_bonus_level(tournament_.current_level);
    const bool tournament_completed = human_won && tournament_.current_level >= 50;
    const TournamentResult result = apply_original_tournament_result(
        tournament_, human_won, score_[1], score_[0]
    );
    last_tournament_points_ = result.points_awarded;
    if (human_won && score_[0] == 0) {
        play_sound_alias(L"perfect");
    } else if (human_won && result.points_awarded > 0) {
        play_sound_alias(L"upscore");
    } else if (!human_won && !result.game_over) {
        play_sound_alias(L"dnscore");
    }
    tournament_unlocked_level_ = std::max(
        tournament_unlocked_level_, tournament_.unlocked_level
    );
    if (tournament_completed) {
        tournament_active_ = false;
        enter_frontend(L"n1_victory");
    } else if (human_won) {
        if (was_bonus && tournament_human_box_overridden_) {
            toyboxes_[1] = tournament_saved_human_box_;
            tournament_saved_human_box_.clear();
            tournament_human_box_overridden_ = false;
        }
        toyboxes_[0].clear();
        enter_frontend(L"n1_start");
    } else if (result.game_over) {
        enter_frontend(L"n1_gameover");
    } else {
        start_tournament_match();
    }
}

void Game::finish_duel() {
    stop_music();
    if (duel_mode_ == DuelMode::Tournament) {
        // Tournament score/life and game-over cues are selected after applying
        // the recovered progression result below.
    } else if (duel_mode_ == DuelMode::HumanVsHuman || duel_mode_ == DuelMode::Attract) {
        play_sound_alias(L"endsound");
    } else if (match_winner_ == 1) {
        play_sound_alias(L"winsound");
    } else {
        play_sound_alias(L"losesound");
    }
    if (duel_mode_ == DuelMode::Tournament) {
        finish_tournament_match();
    } else if (duel_mode_ == DuelMode::Attract) {
        activate_frontend(attract_match_target_);
    } else {
        enter_frontend(
            duel_mode_ == DuelMode::HumanVsHuman ? L"qgoon2" : L"qgoon1"
        );
    }
}

std::filesystem::path Game::sprite_for(const Toy& toy) const {
    const ToyDefinition& definition = definitions_[toy.definition];
    const bool rocket = toy.definition == 13;
    std::wstring key = original_animation_key(toy.animation_state, rocket);
    const auto* animation = &scripts_.animation(definition.script_section, key);
    if (animation->empty() && rocket) {
        key = original_animation_key(toy.animation_state, false);
        animation = &scripts_.animation(definition.script_section, key);
    }
    if (animation->empty()) {
        key = original_animation_key(original_animation_base(toy.animation_state), rocket);
        animation = &scripts_.animation(definition.script_section, key);
    }
    if (animation->empty()) animation = &scripts_.locomotion(definition.script_section);
    if (animation->empty()) return definition.sprite;
    const int base = original_animation_base(toy.animation_state);
    const bool loop = base != kAnimationDeath && !original_animation_is_transient(base);
    const std::filesystem::path frame = animation_frame(*animation, toy.animation_ticks, loop);
    return frame.empty() ? definition.sprite : frame;
}

std::filesystem::path Game::sprite_for(const DynamicObstacle& obstacle) const {
    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value()) return {};
    const std::wstring section = wide_ascii(archetype->parameters);
    std::wstring state = L"w";
    if (obstacle.code == 74 && obstacle.special_state != 0) {
        state = L"x";
    } else if (obstacle.code == 73) {
        if (obstacle.special_state != 0) {
            state = std::abs(obstacle.velocity_x16) >= std::abs(obstacle.velocity_y16)
                        ? L"xh"
                        : L"yh";
        } else if (obstacle.velocity_y16 < -16) {
            state = L"wu";
        } else if (obstacle.velocity_y16 > 16) {
            state = L"wd";
        }
    }
    const std::filesystem::path frame = animation_frame(
        scripts_.animation(section, state), obstacle.animation_ticks, true
    );
    return frame.empty() ? archetype->sprite : frame;
}

std::filesystem::path Game::sprite_for(const Powerup& powerup) const {
    const std::filesystem::path frame = animation_frame(
        scripts_.animation(L"puup", L"w"), powerup.animation_ticks, true
    );
    return frame.empty() ? std::filesystem::path("sprites/digit/key1w01.png") : frame;
}

int Game::animation_duration(const Toy& toy, int state) const {
    const std::wstring& section = definitions_[toy.definition].script_section;
    const bool rocket = toy.definition == 13;
    const auto* animation = &scripts_.animation(
        section, original_animation_key(state, rocket)
    );
    if (animation->empty() && rocket) {
        animation = &scripts_.animation(section, original_animation_key(state));
    }
    if (animation->empty()) {
        animation = &scripts_.animation(
            section, original_animation_key(original_animation_base(state), rocket)
        );
    }
    return std::max(1, total_animation_ticks(*animation));
}

void Game::set_animation_state(Toy& toy, int base_state) {
    const std::wstring& section = definitions_[toy.definition].script_section;
    const int base = original_animation_base(base_state);
    const bool has_down = !scripts_.animation(
        section, original_animation_key(base + 1, toy.definition == 13)
    ).empty();
    const bool has_up = !scripts_.animation(
        section, original_animation_key(base + 2, toy.definition == 13)
    ).empty();
    const int resolved = original_directional_animation_state(
        base, toy.heading, has_down, has_up
    );
    if (resolved == toy.animation_state) return;
    toy.animation_state = resolved;
    toy.animation_ticks = 0;
    toy.animation_finished = false;
}

void Game::advance_animation(Toy& toy) {
    ++toy.age_ticks;
    ++toy.animation_ticks;
    const int duration = animation_duration(toy, toy.animation_state);
    if (toy.animation_ticks < duration) return;
    const int base = original_animation_base(toy.animation_state);
    if (base == kAnimationDeath) {
        toy.animation_ticks = duration;
        toy.animation_finished = true;
    } else if (original_animation_is_transient(base)) {
        set_animation_state(toy, kAnimationWalk);
    }
}

Toy* Game::find_toy(std::uint64_t id) {
    const auto found = std::find_if(toys_.begin(), toys_.end(), [id](const Toy& toy) {
        return toy.id == id && toy.active;
    });
    return found == toys_.end() ? nullptr : &*found;
}

const Toy* Game::find_toy(std::uint64_t id) const {
    const auto found = std::find_if(toys_.begin(), toys_.end(), [id](const Toy& toy) {
        return toy.id == id && toy.active;
    });
    return found == toys_.end() ? nullptr : &*found;
}

void Game::begin_bomby_explosion(Toy& toy) {
    toy.winding = 0;
    toy.velocity_x16 = 0;
    toy.velocity_y16 = 0;
    toy.desired_x16 = 0;
    toy.desired_y16 = 0;
    set_animation_state(toy, kAnimationDeath);
    toy.exploding = true;
    play_sound(scripts_.sound(L"bomby", 3));
}

ToyBox Game::collision_box(const Toy& toy) {
    const ToyDefinition& definition = definitions_[toy.definition];
    const ToyParameters& parameters = definition.parameters;
    const Image& sprite = images_.load(sprite_for(toy));
    float center_x = toy.x;
    float center_y = toy.y;
    if (toy.definition == 11 && toy.attached_id != 0) {
        if (const Toy* attached = find_toy(toy.attached_id); attached != nullptr) {
            const ToyParameters& attached_parameters =
                definitions_[attached->definition].parameters;
            const float direction = toy.owner == 0 ? 1.0F : -1.0F;
            center_x = attached->x + direction *
                static_cast<float>(attached_parameters.handy_attach_x());
            center_y = attached->y +
                static_cast<float>(attached_parameters.handy_attach_y());
        }
    }
    const float draw_left = center_x - static_cast<float>(sprite.width) / 2.0F;
    const float draw_top = center_y - static_cast<float>(sprite.height) / 2.0F;
    float front = static_cast<float>(parameters.collision_front_percent());
    float back = static_cast<float>(parameters.collision_back_percent());
    if (toy.owner != 0) {
        front = 100.0F - front;
        back = 100.0F - back;
    }
    const float width = static_cast<float>(std::max(0, sprite.width - 1));
    const float height = static_cast<float>(std::max(0, sprite.height - 1));
    // Segment 14:0e26-0f7b uses MulDiv over (width-1,height-1), then
    // constructs both the forward and mirrored boxes from Front/Top/Back/Bottom.
    return {
        draw_left + width * std::min(front, back) / 100.0F,
        draw_top + height *
                       static_cast<float>(parameters.collision_top_percent()) / 100.0F,
        draw_left + width * std::max(front, back) / 100.0F,
        draw_top + height *
                       static_cast<float>(parameters.collision_bottom_percent()) / 100.0F,
    };
}

ToyBox Game::collision_box(const ObstacleDefinition& obstacle) {
    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value() || archetype->interaction == ObstacleInteraction::Dynamic) {
        return {};
    }
    const auto& values = defaults_.integers(archetype->parameters);
    if (values.size() != 6 && values.size() != 12) return {};
    const Image& sprite = images_.load(archetype->sprite);
    const float draw_left = static_cast<float>(obstacle.x - sprite.width / 2);
    const float draw_top = static_cast<float>(obstacle.y - sprite.height / 2);
    const std::size_t collision_offset = values.size() == 12 ? 6 : 2;
    float front = static_cast<float>(values[collision_offset]);
    float back = static_cast<float>(values[collision_offset + 2]);
    if (!obstacle.facing_right) {
        front = 100.0F - front;
        back = 100.0F - back;
    }
    const float width = static_cast<float>(std::max(0, sprite.width - 1));
    const float height = static_cast<float>(std::max(0, sprite.height - 1));
    return {
        draw_left + width * std::min(front, back) / 100.0F,
        draw_top + height * static_cast<float>(values[collision_offset + 1]) / 100.0F,
        draw_left + width * std::max(front, back) / 100.0F,
        draw_top + height * static_cast<float>(values[collision_offset + 3]) / 100.0F,
    };
}

ToyBox Game::collision_box(const DynamicObstacle& obstacle) {
    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value() || archetype->interaction != ObstacleInteraction::Dynamic) {
        return {};
    }
    const ToyParameters parameters = defaults_.toy(archetype->parameters);
    const Image& sprite = images_.load(sprite_for(obstacle));
    const float draw_left = obstacle.x - static_cast<float>(sprite.width) / 2.0F;
    const float draw_top = obstacle.y - static_cast<float>(sprite.height) / 2.0F;
    float front = static_cast<float>(parameters.collision_front_percent());
    float back = static_cast<float>(parameters.collision_back_percent());
    if (obstacle.velocity_x16 < 0 ||
        (obstacle.velocity_x16 == 0 && !obstacle.facing_right)) {
        front = 100.0F - front;
        back = 100.0F - back;
    }
    const float width = static_cast<float>(std::max(0, sprite.width - 1));
    const float height = static_cast<float>(std::max(0, sprite.height - 1));
    return {
        draw_left + width * std::min(front, back) / 100.0F,
        draw_top + height * static_cast<float>(parameters.collision_top_percent()) / 100.0F,
        draw_left + width * std::max(front, back) / 100.0F,
        draw_top + height * static_cast<float>(parameters.collision_bottom_percent()) / 100.0F,
    };
}

ToyBox Game::collision_box(const Powerup& powerup) {
    const auto& values = defaults_.integers("puup");
    if (values.size() != 6) return {};
    const Image& sprite = images_.load(sprite_for(powerup));
    const float draw_left = powerup.x - static_cast<float>(sprite.width) / 2.0F;
    const float draw_top = powerup.y - static_cast<float>(sprite.height) / 2.0F;
    const float width = static_cast<float>(std::max(0, sprite.width - 1));
    const float height = static_cast<float>(std::max(0, sprite.height - 1));
    return {
        draw_left + width * static_cast<float>(values[2]) / 100.0F,
        draw_top + height * static_cast<float>(values[3]) / 100.0F,
        draw_left + width * static_cast<float>(values[4]) / 100.0F,
        draw_top + height * static_cast<float>(values[5]) / 100.0F,
    };
}

void Game::update_desired_motion(Toy& toy) {
    if (toy.definition == 0 && toy.behavior_state != 0) {
        toy.desired_x16 = 0;
        toy.desired_y16 = 0;
        set_animation_state(toy, kAnimationAction);
        return;
    }
    const ToyParameters& parameters = definitions_[toy.definition].parameters;
    const int speed16 = scaled_toy_speed16(
        parameters.horizontal_speed(),
        toy.winding,
        defaults_.scalar("DecayTime"),
        defaults_.scalar("SlowingTime")
    );
    const int mode = parameters.movement_mode();
    if (speed16 == 0) {
        toy.desired_x16 = 0;
        toy.desired_y16 = 0;
        return;
    }

    if (mode == 0) {
        toy.desired_x16 = toy.heading > 16 && toy.heading < 48 ? -speed16 : speed16;
        toy.desired_y16 = 0;
        set_animation_state(toy, original_animation_base(toy.animation_state));
        return;
    }
    if (mode == 1) {
        switch ((toy.heading & 63) / 16) {
            case 0:
                toy.desired_x16 = speed16;
                toy.desired_y16 = speed16;
                toy.heading = 8;
                break;
            case 1:
                toy.desired_x16 = -speed16;
                toy.desired_y16 = speed16;
                toy.heading = 24;
                break;
            case 2:
                toy.desired_x16 = -speed16;
                toy.desired_y16 = -speed16;
                toy.heading = 40;
                break;
            default:
                toy.desired_x16 = speed16;
                toy.desired_y16 = -speed16;
                toy.heading = 56;
                break;
        }
        set_animation_state(toy, original_animation_base(toy.animation_state));
        return;
    }
    if (mode == 2) {
        const int erratic_period = std::max(1, defaults_.scalar("Erratic"));
        if (toy.age_ticks % erratic_period == 0) {
            std::uniform_int_distribution<int> heading_distribution(-16, 15);
            toy.heading = (heading_distribution(random_) + (toy.owner == 0 ? 0 : 32)) & 63;
            const int magnitude16 = static_cast<int>(std::hypot(
                static_cast<double>(toy.velocity_x16),
                static_cast<double>(toy.velocity_y16)
            ));
            const MotionVector redirected = vector_from_original_heading(magnitude16, toy.heading);
            toy.velocity_x16 = redirected.x16;
            toy.velocity_y16 = redirected.y16;
        }
        const MotionVector desired = vector_from_original_heading(speed16, toy.heading);
        toy.desired_x16 = desired.x16;
        toy.desired_y16 = desired.y16;
        if (toy.desired_x16 != 0) toy.owner = toy.desired_x16 > 0 ? 0 : 1;
        set_animation_state(toy, original_animation_base(toy.animation_state));
        return;
    }

    toy.desired_x16 = 0;
    toy.desired_y16 = 0;
}

bool Game::apply_contact_filter(Toy& source, Toy& target) {
    if (!source.active || source.exploding || source.winding <= 0 ||
        !target.active || target.exploding || target.winding <= 0) {
        return false;
    }
    const int decay_time = defaults_.scalar("DecayTime");
    const int source_x16 = static_cast<int>(source.x * 16.0F);
    const int target_x16 = static_cast<int>(target.x * 16.0F);

    if (source.definition == 3 && source.winding > decay_time &&
        original_target_is_in_front(source.owner, source_x16, target_x16)) {
        const int previous = target.winding;
        target.winding = original_zappa_drain(
            target.winding, definitions_[source.definition].parameters.primary_extra()
        );
        source.desired_x16 = 0;
        source.desired_y16 = 0;
        set_animation_state(source, kAnimationAction);
        if (previous != target.winding && target.winding > decay_time &&
            original_animation_base(target.animation_state) != kAnimationZap) {
            set_animation_state(target, kAnimationZap);
            play_sound(scripts_.sound(L"zappa", 2));
        }
    }

    if (source.definition == 6 && source.winding > decay_time &&
        source.behavior_state == 0 &&
        original_animation_base(source.animation_state) == kAnimationWalk &&
        original_target_is_in_front(source.owner, source_x16, target_x16)) {
        if (target.definition == 6 && target.winding >= source.winding) return true;
        source.behavior_state =
            definitions_[source.definition].parameters.primary_extra();
        source.velocity_x16 = 0;
        source.velocity_y16 = 0;
        source.desired_x16 = 0;
        source.desired_y16 = 0;
        set_animation_state(source, kAnimationAction);
        if (target.definition != 5 && target.definition != 13) target.winding = -100;
        play_sound(scripts_.sound(L"destr", 2));
    }

    if (source.definition == 9 && target.definition != 3 && target.definition != 13 &&
        source.winding > decay_time && target.winding > decay_time &&
        original_animation_base(target.animation_state) != kAnimationFlip) {
        if (target.definition == 4 && target.owner == source.owner) return true;
        if (target.winding > source.winding) {
            std::uniform_int_distribution<int> coin(0, 1);
            if (coin(random_) != 0) return true;
        }
        reverse_toy(target);
        set_animation_state(target, kAnimationFlip);
        set_animation_state(source, kAnimationAction);
        play_sound(scripts_.sound(L"skull", 2));
    }

    if (source.definition == 10) {
        source.impulse_y16 = original_orbit_contact_impulse(
            source.impulse_y16, source.behavior_state, source.winding, decay_time
        );
    }

    if (source.definition == 11 && target.definition != 9 && target.definition != 11 &&
        target.definition != 13 && source.winding > decay_time && target.winding > 0 &&
        original_target_is_in_front(source.owner, source_x16, target_x16)) {
        if (target.attached_id != 0) {
            const Toy* attached = find_toy(target.attached_id);
            if (attached != nullptr && attached->definition == 11) return true;
        }
        const int previous = target.winding;
        target.winding = std::min(
            defaults_.scalar("GaugeTime"),
            target.winding + definitions_[source.definition].parameters.primary_extra()
        );
        if (previous <= decay_time && target.winding > decay_time) {
            if (target.definition == 6) target.behavior_state = 1;
            set_animation_state(target, kAnimationWalk);
        }
        if (original_animation_base(source.animation_state) != kAnimationAction) {
            play_sound(scripts_.sound(L"handy", 2));
        }
        source.attached_id = target.id;
        set_animation_state(source, kAnimationAction);
    }

    if (source.definition == 12 && target.definition == 2 && source.impulse_y16 == 0 &&
        source.collision_layer == 0 && source.winding > decay_time) {
        source.impulse_y16 = -160;
    }
    return true;
}

void Game::apply_contact_effect(Toy& source, Toy& target) {
    if (!source.active || source.exploding || source.winding <= defaults_.scalar("DecayTime") ||
        !target.active || target.exploding || target.winding <= 0) {
        return;
    }
    const int source_x16 = static_cast<int>(source.x * 16.0F);
    const int target_x16 = static_cast<int>(target.x * 16.0F);

    if (source.definition == 0 && target.definition != 9) {
        source.behavior_state = source.behavior_state == 0 ? 1 : 0;
        if (source.behavior_state != 0) {
            source.desired_x16 = 0;
            source.desired_y16 = 0;
            set_animation_state(source, kAnimationAction);
        }
    }

    if (source.definition == 4 && source.velocity_x16 != 0 &&
        original_target_is_in_front(source.owner, source_x16, target_x16)) {
        if (!original_kanga_can_punch(
                target.definition == 4, source.winding, target.winding
            )) return;
        const bool entering_action =
            original_animation_base(source.animation_state) != kAnimationAction;
        target.impulse_x16 = original_kanga_impulse_x16(
            source.owner,
            definitions_[source.definition].parameters.primary_extra(),
            definitions_[source.definition].parameters.mass(),
            definitions_[target.definition].parameters.mass()
        );
        set_animation_state(source, kAnimationAction);
        if (entering_action) play_sound(scripts_.sound(L"kanga", 2));
    }

    if (source.definition == 10 && source.impulse_y16 == 0) {
        const int margin = board_grid_height(stage_);
        std::uniform_int_distribution<int> coin(0, 1);
        source.behavior_state = original_orbit_choose_behavior(
            static_cast<int>(source.y), stage_.top, stage_.bottom, margin, coin(random_)
        );
        set_animation_state(source, kAnimationAction);
        play_sound(scripts_.sound(L"magnt", 2));
    }
}

void Game::apply_dynamic_special_contact(Toy& source, DynamicObstacle& target) {
    if (!source.active || source.exploding || source.winding <= defaults_.scalar("DecayTime") ||
        target.winding <= 0) {
        return;
    }
    if (source.definition == 10) {
        source.impulse_y16 = original_orbit_contact_impulse(
            source.impulse_y16,
            source.behavior_state,
            source.winding,
            defaults_.scalar("DecayTime")
        );
    }
    if (source.definition == 0) {
        source.behavior_state = source.behavior_state == 0 ? 1 : 0;
        if (source.behavior_state != 0) set_animation_state(source, kAnimationAction);
    }
    if (source.definition == 4 && source.velocity_x16 != 0 &&
        original_target_is_in_front(
            source.owner,
            static_cast<int>(source.x * 16.0F),
            static_cast<int>(target.x * 16.0F)
        )) {
        const auto archetype = obstacle_archetype(target.code);
        if (archetype.has_value()) {
            const bool entering_action =
                original_animation_base(source.animation_state) != kAnimationAction;
            target.impulse_x16 = original_kanga_impulse_x16(
                source.owner,
                definitions_[source.definition].parameters.primary_extra(),
                definitions_[source.definition].parameters.mass(),
                defaults_.toy(archetype->parameters).mass()
            );
            set_animation_state(source, kAnimationAction);
            if (entering_action) play_sound(scripts_.sound(L"kanga", 2));
        }
    }
    if (source.definition == 10 && source.impulse_y16 == 0) {
        const int margin = board_grid_height(stage_);
        std::uniform_int_distribution<int> coin(0, 1);
        source.behavior_state = original_orbit_choose_behavior(
            static_cast<int>(source.y), stage_.top, stage_.bottom, margin, coin(random_)
        );
        set_animation_state(source, kAnimationAction);
        play_sound(scripts_.sound(L"magnt", 2));
    }
}

void Game::apply_static_special_contact(Toy& source, const ObstacleDefinition& target) {
    if (!source.active || source.exploding || source.winding <= defaults_.scalar("DecayTime")) {
        return;
    }
    const auto archetype = obstacle_archetype(target.code);
    if (!archetype.has_value()) return;
    if (source.definition == 10) {
        source.impulse_y16 = original_orbit_contact_impulse(
            source.impulse_y16,
            source.behavior_state,
            source.winding,
            defaults_.scalar("DecayTime")
        );
    }
    if (archetype->interaction != ObstacleInteraction::MotionBlocking) return;
    if (source.definition == 0) {
        source.behavior_state = source.behavior_state == 0 ? 1 : 0;
        if (source.behavior_state != 0) set_animation_state(source, kAnimationAction);
    }
    if (source.definition == 10 && source.impulse_y16 == 0) {
        const int margin = board_grid_height(stage_);
        std::uniform_int_distribution<int> coin(0, 1);
        source.behavior_state = original_orbit_choose_behavior(
            static_cast<int>(source.y), stage_.top, stage_.bottom, margin, coin(random_)
        );
        set_animation_state(source, kAnimationAction);
        play_sound(scripts_.sound(L"magnt", 2));
    }
}

void Game::reverse_toy(Toy& toy) {
    toy.velocity_x16 = -toy.velocity_x16;
    toy.velocity_y16 = -toy.velocity_y16;
    toy.desired_x16 = -toy.desired_x16;
    toy.desired_y16 = -toy.desired_y16;
    toy.heading = (toy.heading + 32) & 63;
    toy.owner = 1 - toy.owner;
    set_animation_state(toy, original_animation_base(toy.animation_state));
}

void Game::apply_surface_contact(const ObstacleDefinition& obstacle, Toy& target) {
    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value() || archetype->interaction != ObstacleInteraction::Surface ||
        !target.active || target.exploding || target.winding <= 0) {
        return;
    }
    const auto& surface = defaults_.integers(archetype->parameters);
    if (surface.size() != 6) return;
    const int strength = surface[1];

    switch (obstacle.code) {
        case 51: {
            if (target.definition == 13) return;
            const int amount = rounded_muldiv(
                strength,
                definitions_[target.definition].parameters.horizontal_speed(),
                100
            );
            target.desired_x16 += obstacle.facing_right ? amount : -amount;
            break;
        }
        case 52:
        case 53: {
            if (target.definition == 13) return;
            const int amount = rounded_muldiv(
                strength,
                definitions_[target.definition].parameters.horizontal_speed(),
                100
            );
            target.desired_y16 += obstacle.code == 52 ? -amount : amount;
            break;
        }
        case 54: {
            if (target.definition == 13) return;
            target.impulse_x16 = static_cast<int>(
                (static_cast<float>(obstacle.x) - target.x) * 16.0F
            );
            target.impulse_y16 = static_cast<int>(
                (static_cast<float>(obstacle.y) - target.y) * 16.0F
            );
            target.velocity_x16 = 0;
            target.velocity_y16 = 0;
            target.desired_x16 = 0;
            target.desired_y16 = 0;
            target.winding = -100;
            play_sound(scripts_.sound(L"crak", 1));
            const auto& crack_animation = scripts_.animation(L"crak", L"x");
            int delay = 0;
            for (int phase = 2; phase <= 3; ++phase) {
                const std::size_t frame = static_cast<std::size_t>(phase - 2);
                delay += frame < crack_animation.size() ? crack_animation[frame].ticks : 1;
                scheduled_sounds_.emplace_back(delay, scripts_.sound(L"crak", phase));
            }
            break;
        }
        case 55: {
            if (obstacle.facing_right != (target.owner != 0)) return;
            play_sound(scripts_.sound(L"tele", 1));
            if (target.winding <= defaults_.scalar("DecayTime")) {
                target.winding = std::min(target.winding, 1);
                return;
            }
            std::vector<const ObstacleDefinition*> exits;
            if (active_level_ != nullptr) {
                for (const ObstacleDefinition& candidate : active_level_->obstacles) {
                    if (candidate.code == 55 &&
                        candidate.facing_right != obstacle.facing_right) {
                        exits.push_back(&candidate);
                    }
                }
            }
            if (!exits.empty()) {
                std::uniform_int_distribution<std::size_t> exit_roll(0, exits.size() - 1);
                const ObstacleDefinition& exit = *exits[exit_roll(random_)];
                target.x = static_cast<float>(exit.x);
                target.y = static_cast<float>(exit.y);
                set_animation_state(target, kAnimationAction);
                play_sound(scripts_.sound(L"arrow", 3));
            }
            break;
        }
        case 56:
        case 58: {
            if (target.definition == 13) return;
            const int percent = 100 - strength;
            target.desired_x16 = rounded_muldiv(target.desired_x16, percent, 100);
            target.desired_y16 = rounded_muldiv(target.desired_y16, percent, 100);
            break;
        }
        case 57: {
            const int percent = strength * 8 + 100;
            target.desired_x16 = rounded_muldiv(target.desired_x16, percent, 100);
            target.desired_y16 = rounded_muldiv(target.desired_y16, percent, 100);
            break;
        }
        case 59:
            if (target.impulse_y16 != 0 || target.definition == 10) return;
            if (target.desired_y16 == 0) {
                target.impulse_y16 = target.y < static_cast<float>(obstacle.y) ? -16 : 16;
            } else {
                target.impulse_y16 = target.desired_y16 > 0 ? 16 : -16;
            }
            break;
        default: break;
    }
}

void Game::apply_powerup_pickup(Powerup& powerup, Toy& target) {
    if (!powerup.active || !target.active || target.exploding || target.winding <= 0 ||
        target.owner == powerup.owner) {
        return;
    }
    const PowerupKind kind = static_cast<PowerupKind>(powerup.kind);
    const int target_owner = target.owner;
    // The +60 callback first rejects a collector who already has an effect.
    if (powerup_effect_[static_cast<std::size_t>(target_owner)] != 0) return;

    const int recipient = original_powerup_effect_recipient(kind, powerup.owner);
    powerup_effect_[static_cast<std::size_t>(recipient)] = powerup.kind;
    powerup_effect_ticks_[static_cast<std::size_t>(recipient)] = std::max(
        1,
        original_powerup_random_bound(15, defaults_.scalar("FrameStep"))
    );
    powerup.active = false;
    play_sound(scripts_.sound(L"puup", 1));

    if (kind == PowerupKind::Rocket) {
        if (docked_rocket_id_.has_value() && find_toy(*docked_rocket_id_) != nullptr) return;
        const float x = recipient == 0
                            ? static_cast<float>(stage_.left + board_grid_width(stage_) / 2)
                            : static_cast<float>(stage_.right - board_grid_width(stage_) / 2);
        if (spawn_toy(
            13,
            recipient,
            x,
            release_y_[static_cast<std::size_t>(recipient)],
            false
        )) {
            Toy& rocket = toys_.back();
            rocket.rocket_docked = true;
            rocket.collision_layer = 1;
            rocket.velocity_x16 = 0;
            rocket.velocity_y16 = 0;
            rocket.desired_x16 = 0;
            rocket.desired_y16 = 0;
            docked_rocket_id_ = rocket.id;
        }
    }
}

void Game::update_powerups() {
    for (std::size_t player = 0; player < powerup_effect_.size(); ++player) {
        if (powerup_effect_ticks_[player] <= 0) continue;
        --powerup_effect_ticks_[player];
        if (powerup_effect_ticks_[player] == 0) powerup_effect_[player] = 0;
    }

    if (powerup_.has_value() && powerup_->active) {
        ++powerup_->animation_ticks;
        const ToyBox key_box = collision_box(*powerup_);
        for (std::size_t index = 0; index < toys_.size(); ++index) {
            Toy& target = toys_[index];
            if (!target.active || target.exploding || target.winding <= 0 ||
                target.owner == powerup_->owner) {
                continue;
            }
            if (!boxes_overlap(key_box, collision_box(target))) continue;
            apply_powerup_pickup(*powerup_, target);
            if (!powerup_->active) break;
        }
        if (!powerup_->active) powerup_.reset();
    }

    if (powerup_.has_value() || !powerups_enabled_ || active_level_ == nullptr ||
        active_level_->powerup_probability <= 0 || powerup_effect_[0] != 0 ||
        powerup_effect_[1] != 0) {
        return;
    }
    const bool rocket_active = std::any_of(toys_.begin(), toys_.end(), [](const Toy& toy) {
        return toy.active && toy.definition == 13;
    });
    if (rocket_active) return;

    const int bound = original_powerup_random_bound(
        active_level_->powerup_probability, defaults_.scalar("FrameStep")
    );
    if (bound <= 0) return;
    std::uniform_int_distribution<int> spawn_roll(0, bound - 1);
    if (spawn_roll(random_) != 0) return;

    if (last_powerup_owner_ < 0) {
        std::uniform_int_distribution<int> owner_roll(0, 1);
        last_powerup_owner_ = owner_roll(random_);
    } else {
        last_powerup_owner_ = 1 - last_powerup_owner_;
    }
    std::uniform_int_distribution<int> kind_roll(1, 5);
    Powerup powerup;
    powerup.owner = last_powerup_owner_;
    powerup.kind = kind_roll(random_);
    powerup.x = powerup.owner == 0 ? static_cast<float>(stage_.left)
                                   : static_cast<float>(stage_.right);
    powerup.y = release_y_[static_cast<std::size_t>(powerup.owner)];
    powerup_ = powerup;
}

void Game::apply_dynamic_contact(DynamicObstacle& obstacle, Toy& target) {
    if (!target.active || target.exploding || target.winding <= 0) return;
    if (obstacle.code == 74) {
        // Block's +60 callback only latches its one-tick impact frame.
        obstacle.special_state = 1;
        return;
    }
    if (obstacle.code != 73 || obstacle.special_state != 0) return;

    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value()) return;
    const int bug_mass = defaults_.toy(archetype->parameters).mass();
    const int target_mass = definitions_[target.definition].parameters.mass();
    target.velocity_x16 = 0;
    target.velocity_y16 = 0;
    target.desired_x16 = 0;
    target.desired_y16 = 0;
    target.impulse_x16 = original_bug_transferred_impulse(
        obstacle.velocity_x16, bug_mass, target_mass
    );
    target.impulse_y16 = original_bug_transferred_impulse(
        obstacle.velocity_y16, bug_mass, target_mass
    );
    obstacle.velocity_x16 = -obstacle.velocity_x16;
    obstacle.velocity_y16 = -obstacle.velocity_y16;
    obstacle.desired_x16 = -obstacle.desired_x16;
    obstacle.desired_y16 = -obstacle.desired_y16;
    obstacle.heading = (obstacle.heading + 32) & 63;
    obstacle.facing_right = !obstacle.facing_right;
    obstacle.special_state = 3;
}

void Game::update_dynamic_obstacle(DynamicObstacle& obstacle) {
    if (obstacle.special_state > 0) --obstacle.special_state;
    if (obstacle.code != 73) {
        obstacle.desired_x16 = 0;
        obstacle.desired_y16 = 0;
        return;
    }

    const auto archetype = obstacle_archetype(obstacle.code);
    if (!archetype.has_value()) return;
    const ToyParameters parameters = defaults_.toy(archetype->parameters);
    const int speed16 = parameters.horizontal_speed() * 4;

    // 10:4066 has two rare state changes, each gated by Random(32)==0.
    std::uniform_int_distribution<int> state_roll(0, 31);
    if (!obstacle.transition_state && obstacle.behavior_state == 0 && state_roll(random_) == 0) {
        obstacle.behavior_state = 9;
        obstacle.transition_state = true;
    } else if (obstacle.transition_state && obstacle.behavior_state == 9 &&
               state_roll(random_) == 0) {
        obstacle.behavior_state = 15;
        obstacle.transition_state = false;
    } else if (obstacle.behavior_state >= 15 && state_roll(random_) == 0) {
        obstacle.behavior_state = 0;
    }

    if (obstacle.behavior_state >= 15) {
        obstacle.desired_x16 = 0;
        obstacle.desired_y16 = 0;
        obstacle.velocity_x16 = original_bug_slow_velocity(obstacle.velocity_x16);
        obstacle.velocity_y16 = original_bug_slow_velocity(obstacle.velocity_y16);
    } else {
        const int erratic = std::max(1, defaults_.scalar("Erratic"));
        if (obstacle.animation_ticks % erratic == 0) {
            std::uniform_int_distribution<int> turn(-8, 8);
            obstacle.heading = (obstacle.heading + turn(random_)) & 63;
        }
        MotionVector desired = vector_from_original_heading(speed16, obstacle.heading);
        if (obstacle.behavior_state == 9) {
            desired.x16 /= 2;
            desired.y16 /= 2;
        }
        obstacle.desired_x16 = desired.x16;
        obstacle.desired_y16 = desired.y16;
        if (obstacle.velocity_x16 == 0 && obstacle.velocity_y16 == 0) {
            obstacle.velocity_x16 = obstacle.desired_x16;
            obstacle.velocity_y16 = obstacle.desired_y16;
        }
    }
    obstacle.facing_right = obstacle.desired_x16 > 0;
}

void Game::advance_duel_tick() {
    auto pending = scheduled_sounds_.begin();
    while (pending != scheduled_sounds_.end()) {
        --pending->first;
        if (pending->first <= 0) {
            play_sound(pending->second);
            pending = scheduled_sounds_.erase(pending);
        } else {
            ++pending;
        }
    }
    for (Toy& toy : toys_) {
        toy.attached_id = 0;
        if (toy.active) advance_animation(toy);
        if (toy.rocket_docked) {
            toy.y = release_y_[static_cast<std::size_t>(toy.owner)];
        }
    }
    update_powerups();
    for (DynamicObstacle& obstacle : dynamic_obstacles_) update_dynamic_obstacle(obstacle);
    const auto physical_toy = [](const Toy& toy) {
        return toy.active && !toy.exploding && toy.winding > 0 && toy.collision_layer == 0;
    };
    const std::size_t body_count = toys_.size() + dynamic_obstacles_.size();
    std::set<std::pair<std::uint64_t, std::uint64_t>> current_toy_contacts;
    std::vector<std::size_t> parent(body_count);
    std::vector<bool> motion_blocked(body_count, false);
    for (std::size_t index = 0; index < parent.size(); ++index) parent[index] = index;
    const auto find_root = [&parent](std::size_t index) {
        while (parent[index] != index) {
            parent[index] = parent[parent[index]];
            index = parent[index];
        }
        return index;
    };
    if (active_level_ != nullptr) {
        for (std::size_t index = 0; index < toys_.size(); ++index) {
            Toy& toy = toys_[index];
            if (!physical_toy(toy)) continue;
            for (const ObstacleDefinition& obstacle : active_level_->obstacles) {
                const auto archetype = obstacle_archetype(obstacle.code);
                if (!archetype.has_value()) {
                    continue;
                }
                const ToyBox toy_box = collision_box(toy);
                const ToyBox obstacle_box = collision_box(obstacle);
                const float overlap_x = std::min(toy_box.right, obstacle_box.right) -
                                        std::max(toy_box.left, obstacle_box.left);
                const float overlap_y = std::min(toy_box.bottom, obstacle_box.bottom) -
                                        std::max(toy_box.top, obstacle_box.top);
                if (overlap_x <= 0.0F || overlap_y <= 0.0F) continue;

                apply_static_special_contact(toy, obstacle);

                if (archetype->interaction == ObstacleInteraction::Surface) {
                    apply_surface_contact(obstacle, toy);
                    continue;
                }
                if (archetype->interaction != ObstacleInteraction::MotionBlocking) continue;

                if (ice_) {
                    CollisionBody toy_body{
                        static_cast<int>(toy.x * 16.0F),
                        static_cast<int>(toy.y * 16.0F),
                        toy.velocity_x16,
                        toy.velocity_y16,
                        definitions_[toy.definition].parameters.mass(),
                    };
                    CollisionBody obstacle_body{
                        obstacle.x * 16,
                        obstacle.y * 16,
                        0,
                        0,
                        defaults_.toy(archetype->parameters).mass(),
                        true,
                        true,
                    };
                    if (resolve_original_collision(toy_body, obstacle_body)) {
                        toy.velocity_x16 = toy_body.velocity_x16;
                        toy.velocity_y16 = toy_body.velocity_y16;
                    }
                } else {
                    motion_blocked[index] = true;
                }
            }
        }
    }
    for (std::size_t obstacle_index = 0; obstacle_index < dynamic_obstacles_.size();
         ++obstacle_index) {
        DynamicObstacle& obstacle = dynamic_obstacles_[obstacle_index];
        const auto archetype = obstacle_archetype(obstacle.code);
        if (!archetype.has_value()) continue;
        for (std::size_t toy_index = 0; toy_index < toys_.size(); ++toy_index) {
            Toy& toy = toys_[toy_index];
            if (!physical_toy(toy)) continue;
            const ToyBox toy_box = collision_box(toy);
            const ToyBox obstacle_box = collision_box(obstacle);
            const float overlap_x = std::min(toy_box.right, obstacle_box.right) -
                                    std::max(toy_box.left, obstacle_box.left);
            const float overlap_y = std::min(toy_box.bottom, obstacle_box.bottom) -
                                    std::max(toy_box.top, obstacle_box.top);
            if (overlap_x <= 0.0F || overlap_y <= 0.0F) continue;

            apply_dynamic_special_contact(toy, obstacle);
            apply_dynamic_contact(obstacle, toy);

            if (ice_) {
                CollisionBody toy_body{
                    static_cast<int>(toy.x * 16.0F),
                    static_cast<int>(toy.y * 16.0F),
                    toy.velocity_x16,
                    toy.velocity_y16,
                    definitions_[toy.definition].parameters.mass(),
                };
                CollisionBody obstacle_body{
                    static_cast<int>(obstacle.x * 16.0F),
                    static_cast<int>(obstacle.y * 16.0F),
                    obstacle.velocity_x16,
                    obstacle.velocity_y16,
                    defaults_.toy(archetype->parameters).mass(),
                };
                if (resolve_original_collision(toy_body, obstacle_body)) {
                    toy.velocity_x16 = toy_body.velocity_x16;
                    toy.velocity_y16 = toy_body.velocity_y16;
                    obstacle.velocity_x16 = obstacle_body.velocity_x16;
                    obstacle.velocity_y16 = obstacle_body.velocity_y16;
                }
            } else {
                const std::size_t toy_root = find_root(toy_index);
                const std::size_t obstacle_root =
                    find_root(toys_.size() + obstacle_index);
                if (toy_root != obstacle_root) parent[obstacle_root] = toy_root;
            }
        }
    }
    for (std::size_t first = 0; first < dynamic_obstacles_.size(); ++first) {
        for (std::size_t second = first + 1; second < dynamic_obstacles_.size(); ++second) {
            DynamicObstacle& left = dynamic_obstacles_[first];
            DynamicObstacle& right = dynamic_obstacles_[second];
            if (!boxes_overlap(collision_box(left), collision_box(right))) continue;

            if (left.code == 74) left.special_state = 1;
            if (right.code == 74) right.special_state = 1;
            std::uniform_int_distribution<int> jitter(0, 3);
            const auto separate_bug = [this, &jitter](
                                          DynamicObstacle& bug,
                                          const DynamicObstacle& target
                                      ) {
                if (bug.code != 73) return;
                const BugSteerImpulse impulse = original_bug_separation_impulse(
                    static_cast<int>(bug.x * 16.0F),
                    static_cast<int>(bug.y * 16.0F),
                    static_cast<int>(target.x * 16.0F),
                    static_cast<int>(target.y * 16.0F),
                    stage_.left * 16,
                    stage_.top * 16,
                    stage_.right * 16,
                    stage_.bottom * 16,
                    jitter(random_),
                    jitter(random_)
                );
                bug.impulse_x16 = impulse.x16;
                bug.impulse_y16 = impulse.y16;
            };
            separate_bug(left, right);
            separate_bug(right, left);

            const std::size_t left_index = toys_.size() + first;
            const std::size_t right_index = toys_.size() + second;
            if (ice_) {
                const auto left_type = obstacle_archetype(left.code);
                const auto right_type = obstacle_archetype(right.code);
                if (left_type.has_value() && right_type.has_value()) {
                    CollisionBody left_body{
                        static_cast<int>(left.x * 16.0F),
                        static_cast<int>(left.y * 16.0F),
                        left.velocity_x16,
                        left.velocity_y16,
                        defaults_.toy(left_type->parameters).mass(),
                    };
                    CollisionBody right_body{
                        static_cast<int>(right.x * 16.0F),
                        static_cast<int>(right.y * 16.0F),
                        right.velocity_x16,
                        right.velocity_y16,
                        defaults_.toy(right_type->parameters).mass(),
                    };
                    if (resolve_original_collision(left_body, right_body)) {
                        left.velocity_x16 = left_body.velocity_x16;
                        left.velocity_y16 = left_body.velocity_y16;
                        right.velocity_x16 = right_body.velocity_x16;
                        right.velocity_y16 = right_body.velocity_y16;
                    }
                }
            } else {
                const std::size_t left_root = find_root(left_index);
                const std::size_t right_root = find_root(right_index);
                if (left_root != right_root) parent[right_root] = left_root;
            }
        }
    }
    for (std::size_t first = 0; first < toys_.size(); ++first) {
        if (!physical_toy(toys_[first])) continue;
        for (std::size_t second = first + 1; second < toys_.size(); ++second) {
            if (!physical_toy(toys_[second])) continue;
            Toy& left = toys_[first];
            Toy& right = toys_[second];
            const ToyBox left_box = collision_box(left);
            const ToyBox right_box = collision_box(right);
            const float overlap_x = std::min(left_box.right, right_box.right) -
                                    std::max(left_box.left, right_box.left);
            const float overlap_y = std::min(left_box.bottom, right_box.bottom) -
                                    std::max(left_box.top, right_box.top);
            if (overlap_x <= 0.0F || overlap_y <= 0.0F) continue;
            const auto contact = std::minmax(left.id, right.id);
            current_toy_contacts.emplace(contact.first, contact.second);
            if (!previous_toy_contacts_.contains({contact.first, contact.second})) {
                const int relative_x = left.velocity_x16 - right.velocity_x16;
                const int relative_y = left.velocity_y16 - right.velocity_y16;
                play_collision_sound(static_cast<int>(std::hypot(relative_x, relative_y)));
            }
            apply_contact_filter(left, right);
            apply_contact_filter(right, left);
            apply_contact_effect(left, right);
            apply_contact_effect(right, left);
            if (ice_) {
                CollisionBody left_body{
                    static_cast<int>(left.x * 16.0F),
                    static_cast<int>(left.y * 16.0F),
                    left.velocity_x16,
                    left.velocity_y16,
                    definitions_[left.definition].parameters.mass(),
                };
                CollisionBody right_body{
                    static_cast<int>(right.x * 16.0F),
                    static_cast<int>(right.y * 16.0F),
                    right.velocity_x16,
                    right.velocity_y16,
                    definitions_[right.definition].parameters.mass(),
                };
                if (resolve_original_collision(left_body, right_body)) {
                    left.velocity_x16 = left_body.velocity_x16;
                    left.velocity_y16 = left_body.velocity_y16;
                    right.velocity_x16 = right_body.velocity_x16;
                    right.velocity_y16 = right_body.velocity_y16;
                }
            } else {
                const std::size_t first_root = find_root(first);
                const std::size_t second_root = find_root(second);
                if (first_root != second_root) parent[second_root] = first_root;
            }
        }
    }

    if (!ice_) {
        std::vector<std::vector<std::size_t>> components(body_count);
        for (std::size_t index = 0; index < toys_.size(); ++index) {
            if (physical_toy(toys_[index])) components[find_root(index)].push_back(index);
        }
        for (std::size_t index = 0; index < dynamic_obstacles_.size(); ++index) {
            const std::size_t body_index = toys_.size() + index;
            components[find_root(body_index)].push_back(body_index);
        }
        for (const auto& component : components) {
            if (component.empty()) continue;
            if (component.size() == 1) {
                const std::size_t body_index = component.front();
                if (body_index >= toys_.size()) {
                    DynamicObstacle& obstacle =
                        dynamic_obstacles_[body_index - toys_.size()];
                    obstacle.velocity_x16 = approach_original_velocity(
                        obstacle.velocity_x16, obstacle.desired_x16, friction_
                    );
                    obstacle.velocity_y16 = approach_original_velocity(
                        obstacle.velocity_y16, obstacle.desired_y16, friction_
                    );
                    continue;
                }
                Toy& toy = toys_[body_index];
                if (motion_blocked[body_index]) {
                    toy.velocity_x16 = 0;
                    toy.velocity_y16 = 0;
                    toy.desired_x16 = 0;
                    toy.desired_y16 = 0;
                    continue;
                }
                if (toy.winding > defaults_.scalar("DecayTime")) {
                    toy.velocity_x16 =
                        approach_original_velocity(toy.velocity_x16, toy.desired_x16, friction_);
                    toy.velocity_y16 =
                        approach_original_velocity(toy.velocity_y16, toy.desired_y16, friction_);
                } else {
                    toy.velocity_x16 = 0;
                    toy.velocity_y16 = 0;
                }
                continue;
            }

            std::vector<ContactMotionBody> bodies;
            bodies.reserve(component.size());
            for (const std::size_t index : component) {
                if (index < toys_.size()) {
                    const Toy& toy = toys_[index];
                    bodies.push_back({
                        toy.velocity_x16,
                        toy.velocity_y16,
                        toy.desired_x16,
                        toy.desired_y16,
                        definitions_[toy.definition].parameters.mass(),
                        motion_blocked[index],
                    });
                } else {
                    const DynamicObstacle& obstacle =
                        dynamic_obstacles_[index - toys_.size()];
                    const auto archetype = obstacle_archetype(obstacle.code);
                    if (!archetype.has_value()) continue;
                    const ToyParameters parameters = defaults_.toy(archetype->parameters);
                    bodies.push_back({
                        obstacle.velocity_x16,
                        obstacle.velocity_y16,
                        obstacle.desired_x16,
                        obstacle.desired_y16,
                        parameters.mass(),
                        false,
                    });
                }
            }
            const MotionVector group_velocity =
                integrate_original_contact_group(bodies, friction_);
            for (const std::size_t index : component) {
                if (index < toys_.size()) {
                    Toy& toy = toys_[index];
                    toy.velocity_x16 = group_velocity.x16;
                    toy.velocity_y16 = group_velocity.y16;
                    toy.desired_x16 = 0;
                    toy.desired_y16 = 0;
                } else {
                    DynamicObstacle& obstacle =
                        dynamic_obstacles_[index - toys_.size()];
                    obstacle.velocity_x16 = group_velocity.x16;
                    obstacle.velocity_y16 = group_velocity.y16;
                }
            }
        }
    }

    struct SmallFrySpawn {
        int owner = 0;
        float x = 0.0F;
        float y = 0.0F;
    };
    std::vector<SmallFrySpawn> small_fry_spawns;
    const int decay_time = defaults_.scalar("DecayTime");
    const auto ordinary_tick = [this, decay_time](Toy& toy) {
        const int previous_winding = toy.winding;
        toy.winding -= definitions_[toy.definition].parameters.vim_decay();
        if (toy.winding <= 0) {
            toy.winding = 0;
            set_animation_state(toy, kAnimationDeath);
            return;
        }
        if (previous_winding > decay_time && toy.winding <= decay_time) {
            set_animation_state(toy, kAnimationWindDown);
        }
        update_desired_motion(toy);
    };
    const auto bomby_blast = [this](std::size_t source_index) {
        Toy& source = toys_[source_index];
        const int source_x16 = static_cast<int>(source.x * 16.0F);
        const int source_y16 = static_cast<int>(source.y * 16.0F);
        const int radius = definitions_[source.definition].parameters.primary_extra();
        for (Toy& target : toys_) {
            const BlastTarget blast_target{
                target.definition + 15,
                static_cast<int>(target.x * 16.0F),
                static_cast<int>(target.y * 16.0F),
                target.winding,
                target.active && !target.exploding,
            };
            target.winding = original_bomby_blast_winding(
                source_x16, source_y16, radius, blast_target
            );
        }
        source.active = false;
    };
    const auto krush_roar = [this](std::size_t source_index) {
        const Toy& source = toys_[source_index];
        const int radius = definitions_[source.definition].parameters.secondary_extra();
        const int source_x16 = static_cast<int>(source.x * 16.0F);
        const int source_y16 = static_cast<int>(source.y * 16.0F);
        for (std::size_t target_index = 0; target_index < toys_.size(); ++target_index) {
            if (target_index == source_index) continue;
            Toy& target = toys_[target_index];
            if (!target.active || target.exploding || target.winding <= 0 ||
                target.definition == 13) {
                continue;
            }
            if (inside_original_radius(
                    source_x16,
                    source_y16,
                    static_cast<int>(target.x * 16.0F),
                    static_cast<int>(target.y * 16.0F),
                    radius,
                    true
                )) {
                reverse_toy(target);
            }
        }
    };

    // The original dispatches every type's +68 callback before the common
    // position pass. This makes Bomby chains and Krush reversals affect all
    // objects on the same simulation tick.
    for (std::size_t toy_index = 0; toy_index < toys_.size(); ++toy_index) {
        Toy& toy = toys_[toy_index];
        if (!toy.active) continue;
        if (toy.definition == 1 && toy.winding < 0) {
            bomby_blast(toy_index);
            continue;
        }
        if (toy.exploding) {
            if (toy.animation_finished) bomby_blast(toy_index);
            continue;
        }
        if (toy.winding <= 0) {
            if (toy.definition == 1) {
                begin_bomby_explosion(toy);
            } else {
                toy.active = false;
            }
            continue;
        }
        if (toy.definition == 2 && toy.winding > decay_time &&
            toy.behavior_state != 0) {
            toy.desired_x16 = 0;
            toy.desired_y16 = 0;
            if (toy.behavior_state == 1) {
                if (original_animation_base(toy.animation_state) != kAnimationAction) {
                    toy.behavior_state = 0;
                }
            } else {
                --toy.behavior_state;
                if (toy.behavior_state == 1) {
                    std::uniform_int_distribution<int> egg_roll(0, 99);
                    const ToyBox box = collision_box(toy);
                    if (egg_roll(random_) <
                            definitions_[toy.definition].parameters.secondary_extra() &&
                        box.left > static_cast<float>(stage_.left) &&
                        box.right < static_cast<float>(stage_.right)) {
                        const float direction = toy.owner == 0 ? -1.0F : 1.0F;
                        small_fry_spawns.push_back({
                            toy.owner,
                            toy.x + direction * (box.right - box.left) / 2.0F,
                            toy.y,
                        });
                        set_animation_state(toy, kAnimationAction);
                    }
                    toy.velocity_x16 = toy.owner == 0 ? 64 : -64;
                }
            }
            continue;
        }
        if (toy.definition == 12 && toy.collision_layer != 0) {
            if (original_small_fry_should_hatch(
                    toy.collision_layer, toy.animation_state
                )) {
                toy.behavior_state = 1;
                toy.collision_layer = 0;
                play_sound(scripts_.sound(L"small", 1));
            }
            continue;
        }
        if (toy.definition == 13 && toy.rocket_docked) {
            toy.velocity_x16 = 0;
            toy.velocity_y16 = 0;
            toy.impulse_x16 = 0;
            continue;
        }
        if (toy.definition == 13) {
            const int warned_winding = original_rocket_warning_winding(
                toy.winding, decay_time, defaults_.scalar("FrameStep")
            );
            if (warned_winding != toy.winding) {
                play_sound(scripts_.sound(L"roket", 2));
                toy.winding = warned_winding;
            }
        }
        if (toy.definition == 1 && toy.winding < decay_time) {
            toy.winding = 1;
        }

        if (toy.definition == 6 && toy.winding > decay_time &&
            toy.behavior_state > 0 &&
            original_animation_base(toy.animation_state) != kAnimationAction) {
            toy.behavior_state = std::max(0, toy.behavior_state - 2);
            if (toy.behavior_state == 0) set_animation_state(toy, kAnimationAlternate);
        }
        if (toy.definition == 7 && toy.winding > decay_time) {
            const int primary = definitions_[toy.definition].parameters.primary_extra();
            std::uniform_int_distribution<int> wait_roll(0, std::max(0, primary - 1));
            const PrestoStep step = original_presto_step(
                toy.behavior_state,
                animation_duration(toy, kAnimationAction),
                primary,
                wait_roll(random_)
            );
            toy.behavior_state = step.behavior_state;
            if (step.begin_action) {
                set_animation_state(toy, kAnimationAction);
                play_sound(scripts_.sound(L"stick", 2));
            }
            if (step.jump) {
                std::uniform_int_distribution<int> lane_roll(0, 4);
                std::uniform_int_distribution<int> x_roll(
                    0, std::max(0, board_grid_width(stage_) * 16 - 1)
                );
                toy.impulse_y16 = static_cast<int>(
                    (board_lane_y(lane_roll(random_), stage_) - toy.y) * 16.0F
                );
                toy.impulse_x16 = x_roll(random_);
                if (toy.owner != 0) toy.impulse_x16 = -toy.impulse_x16;
            }
        }
        if (toy.definition == 8 && toy.winding > decay_time) {
            const KrushStep step = original_krush_step(
                toy.behavior_state,
                animation_duration(toy, kAnimationAction),
                definitions_[toy.definition].parameters.primary_extra()
            );
            toy.behavior_state = step.behavior_state;
            if (step.begin_action) {
                toy.velocity_x16 = 0;
                set_animation_state(toy, kAnimationAction);
            }
            if (step.play_roar) play_sound(scripts_.sound(L"goril", 3));
            if (step.reverse_targets) krush_roar(toy_index);
        }
        ordinary_tick(toy);
        if (toy.definition == 2 && toy.winding > decay_time) {
            std::uniform_int_distribution<int> rest_roll(
                0, std::max(1, defaults_.scalar("Curved")) - 1
            );
            if (rest_roll(random_) == 0) {
                toy.collision_layer = 0;
                toy.behavior_state =
                    definitions_[toy.definition].parameters.primary_extra();
                toy.velocity_x16 = 0;
                toy.velocity_y16 = 0;
                toy.desired_x16 = 0;
                toy.desired_y16 = 0;
                set_animation_state(toy, kAnimationWalk);
            } else {
                toy.collision_layer = 1;
                set_animation_state(toy, kAnimationAlternate);
            }
        }
    }
    for (const SmallFrySpawn& spawn : small_fry_spawns) {
        const int active_count = static_cast<int>(std::count_if(
            toys_.begin(), toys_.end(), [](const Toy& toy) { return toy.active; }
        ));
        if (active_count >= max_live_objects_) break;
        Toy small;
        small.definition = 12;
        small.owner = spawn.owner;
        small.x = spawn.x;
        small.y = spawn.y;
        small.heading = spawn.owner == 0 ? 0 : 32;
        small.winding = defaults_.scalar("GaugeTime");
        small.collision_layer = 1;
        small.animation_state = kAnimationAction;
        small.id = next_toy_id_++;
        toys_.push_back(small);
    }

    for (DynamicObstacle& obstacle : dynamic_obstacles_) {
        const auto archetype = obstacle_archetype(obstacle.code);
        if (!archetype.has_value()) continue;
        const int impulse_step = take_original_horizontal_impulse_step(obstacle.impulse_x16);
        obstacle.x += static_cast<float>(obstacle.velocity_x16 + impulse_step) / 16.0F;
        obstacle.y += static_cast<float>(obstacle.velocity_y16 + obstacle.impulse_y16) / 16.0F;
        obstacle.impulse_y16 = 0;
        ++obstacle.animation_ticks;
        ToyBox box = collision_box(obstacle);
        if (box.left < static_cast<float>(stage_.left) ||
            box.right > static_cast<float>(stage_.right)) {
            obstacle.velocity_x16 = -obstacle.velocity_x16;
            obstacle.desired_x16 = -obstacle.desired_x16;
            obstacle.facing_right = obstacle.velocity_x16 >= 0;
            obstacle.heading = (32 - obstacle.heading) & 63;
            if (box.left < static_cast<float>(stage_.left)) {
                obstacle.x += static_cast<float>(stage_.left) - box.left;
            } else {
                obstacle.x -= box.right - static_cast<float>(stage_.right);
            }
        }
        box = collision_box(obstacle);
        if (box.top < static_cast<float>(stage_.top) ||
            box.bottom > static_cast<float>(stage_.bottom)) {
            obstacle.velocity_y16 = -obstacle.velocity_y16;
            obstacle.desired_y16 = -obstacle.desired_y16;
            obstacle.heading = (64 - obstacle.heading) & 63;
            if (box.top < static_cast<float>(stage_.top)) {
                obstacle.y += static_cast<float>(stage_.top) - box.top;
            } else {
                obstacle.y -= box.bottom - static_cast<float>(stage_.bottom);
            }
        }
    }
    for (Toy& toy : toys_) {
        if (!toy.active || toy.exploding || toy.rocket_docked) continue;
        const int impulse_step = take_original_horizontal_impulse_step(toy.impulse_x16);
        toy.x += static_cast<float>(impulse_step + toy.velocity_x16) / 16.0F;
        toy.y += static_cast<float>(toy.velocity_y16 + toy.impulse_y16) / 16.0F;
        toy.impulse_y16 = 0;
        ToyBox box = collision_box(toy);
        if (box.top < static_cast<float>(stage_.top) ||
            box.bottom > static_cast<float>(stage_.bottom)) {
            toy.velocity_y16 = -toy.velocity_y16;
            toy.desired_y16 = -toy.desired_y16;
            toy.heading = (64 - toy.heading) & 63;
            set_animation_state(toy, original_animation_base(toy.animation_state));
            if (box.top < static_cast<float>(stage_.top)) {
                toy.y += static_cast<float>(stage_.top) - box.top;
            } else {
                toy.y -= box.bottom - static_cast<float>(stage_.bottom);
            }
        }
        box = collision_box(toy);
        if (box.left > static_cast<float>(stage_.right)) {
            ++score_[0];
            play_sound(scripts_.sound(L"digit", 1));
            toy.active = false;
        } else if (box.right < static_cast<float>(stage_.left)) {
            ++score_[1];
            play_sound(scripts_.sound(L"digit", 2));
            toy.active = false;
        }
    }
    toys_.erase(
        std::remove_if(toys_.begin(), toys_.end(), [](const Toy& toy) { return !toy.active; }),
        toys_.end()
    );
    match_winner_ = original_match_winner(
        score_[0], score_[1], defaults_.scalar("Winningscore")
    );
    previous_toy_contacts_ = std::move(current_toy_contacts);
}

void Game::render(Canvas& canvas) {
    switch (screen_) {
        case Screen::Frontend: render_frontend(canvas); break;
        case Screen::Duel: render_duel(canvas); break;
    }
    if (audio_notice_seconds_ > 0.0 && !audio_notice_.empty()) {
        canvas.fill_rect(176, 436, 288, 32, RGB(0, 0, 0));
        canvas.frame_rect(176, 436, 288, 32, kCream, 1);
        canvas.text(audio_notice_, 320, 443, 18, kCream, 1);
    }
}

void Game::render_frontend(Canvas& canvas) {
    canvas.clear(RGB(0, 0, 0));
    const ScreenDefinition* definition = screens_.find(frontend_name_);
    if (definition == nullptr) return;
    const auto interactive = interactive_commands();
    const ScreenCommand* selected = interactive.empty()
                                        ? nullptr
                                        : interactive[static_cast<std::size_t>(menu_item_)];

    for (const ScreenCommand& command : definition->commands) {
        int action_player = 0;
        int action_definition = 0;
        const bool toybox_button = decode_toybox_action(
            command_action(command), action_player, action_definition
        );
        const bool map_command = command.key == L"map" || command.key == L"map_title" ||
                                 command.key == L"button" || command.key == L"mrgai";
        if (map_command && command.arguments.size() >= 4) {
            const int x = number(command.arguments[0], command.source_line);
            const int y = number(command.arguments[1], command.source_line);
            std::wstring resource = lower(command.arguments[2]);
            const std::filesystem::path relative =
                std::filesystem::path(L"ui") / (resource + L".png");
            if (!std::filesystem::is_regular_file(asset_root_ / relative)) continue;
            const Image& image = images_.load(relative);
            const std::size_t crop = command.key == L"button" ? 5 : 4;
            int draw_width = image.width;
            int draw_height = image.height;
            if (command.arguments.size() >= crop + 4) {
                const int source_x = number(command.arguments[crop], command.source_line);
                const int source_y = number(command.arguments[crop + 1], command.source_line);
                draw_width = number(command.arguments[crop + 2], command.source_line);
                draw_height = number(command.arguments[crop + 3], command.source_line);
                canvas.image_region(image, x, y, source_x, source_y, draw_width, draw_height);
            } else {
                canvas.image(image, x, y);
            }
            if (toybox_button) {
                const auto& toybox = toyboxes_[static_cast<std::size_t>(action_player)];
                if (std::find(toybox.begin(), toybox.end(), action_definition) != toybox.end()) {
                    canvas.frame_rect(
                        x, y, draw_width, draw_height,
                        action_player == 0 ? kGreen : kBlue, 4
                    );
                }
            }
            if (&command == selected) {
                canvas.frame_rect(x, y, draw_width, draw_height, kGreen, 2);
            }
            continue;
        }
        if (!is_text_command(command) || command.arguments.size() < 8) continue;
        const int description_definition = description_toy(command);
        if (description_definition >= 0) {
            int selected_definition = -1;
            if (selected != nullptr) {
                int ignored_player = 0;
                decode_type_action(
                    command_action(*selected), ignored_player, selected_definition, 18
                );
            }
            if (description_definition != selected_definition) continue;
        }
        const int alignment = number(command.arguments[2], command.source_line);
        if (alignment < 0 || alignment > 2) continue;
        std::wstring display_text = champions_.placeholder(command.arguments[3]);
        if (frontend_name_ == L"n1_victory" && command.arguments[3] == L"#00") {
            display_text = entered_name_.empty() ? L"_" : entered_name_ + L"_";
        }
        canvas.text(
            display_text,
            number(command.arguments[0], command.source_line),
            number(command.arguments[1], command.source_line),
            number(command.arguments[4], command.source_line),
            RGB(
                number(command.arguments[5], command.source_line),
                number(command.arguments[6], command.source_line),
                number(command.arguments[7], command.source_line)
            ),
            alignment,
            &command == selected
        );
    }
    if (frontend_name_ == L"n1_start" && tournament_active_) {
        canvas.text(
            std::format(
                L"Level {}   Lives {}   Score {}",
                tournament_.current_level,
                tournament_.lives,
                tournament_.total_score
            ),
            320,
            35,
            22,
            kCream,
            1
        );
        if (last_tournament_points_ > 0) {
            canvas.text(
                std::format(L"Previous win: +{}", last_tournament_points_),
                320,
                465,
                16,
                kGreen,
                1
            );
        }
    } else if (frontend_name_ == L"n1_gameover" && tournament_active_) {
        canvas.text(
            std::format(L"Tournament score: {}", tournament_.total_score),
            320,
            390,
            24,
            kCream,
            1
        );
    }
}

void Game::render_duel(Canvas& canvas) {
    canvas.clear(RGB(0, 0, 0));
    canvas.image(images_.load(background_), 0, 0);
    if (active_level_ != nullptr && !active_level_->gauge_archive.empty()) {
        const int gauge_time = defaults_.scalar("GaugeTime");
        const std::array<Rectangle, 2> rectangles{
            active_level_->left_gauge,
            active_level_->right_gauge,
        };
        for (int player = 0; player < 2; ++player) {
            const Rectangle& rectangle = rectangles[static_cast<std::size_t>(player)];
            if (!valid_rectangle(rectangle)) continue;
            const int charge = launch_gauges_[static_cast<std::size_t>(player)].winding(
                gauge_time
            );
            const int frame = std::clamp(charge * 36 / std::max(1, gauge_time), 0, 35) + 1;
            const std::wstring filename = std::format(
                L"{}1w{:02}.png", active_level_->gauge_archive, frame
            );
            const Image& image = images_.load(
                std::filesystem::path(L"sprites") / active_level_->gauge_archive / filename
            );
            canvas.image(
                image,
                (rectangle.left + rectangle.right) / 2 - image.origin_x,
                (rectangle.top + rectangle.bottom) / 2 - image.height + image.origin_y
            );
        }
    }
    if (active_level_ != nullptr) {
        const std::array<Rectangle, 2> rectangles{
            active_level_->left_toybox,
            active_level_->right_toybox,
        };
        for (int player = 0; player < 2; ++player) {
            const Rectangle& rectangle = rectangles[static_cast<std::size_t>(player)];
            const int definition_index = selected_toy_[static_cast<std::size_t>(player)];
            if (!valid_rectangle(rectangle) || definition_index < 0 ||
                definition_index >= static_cast<int>(definitions_.size())) {
                continue;
            }
            const Image& image = images_.load(
                definitions_[static_cast<std::size_t>(definition_index)].sprite
            );
            canvas.image(
                image,
                (rectangle.left + rectangle.right - image.width) / 2,
                (rectangle.top + rectangle.bottom - image.height) / 2,
                player == 1
            );
        }
    }
    if (active_level_ != nullptr) {
        for (const ObstacleDefinition& obstacle : active_level_->obstacles) {
            const auto archetype = obstacle_archetype(obstacle.code);
            if (!archetype.has_value()) continue;
            if (archetype->interaction == ObstacleInteraction::Dynamic) continue;
            const Image& image = images_.load(archetype->sprite);
            canvas.image(
                image,
                obstacle.x - image.width / 2,
                obstacle.y - image.height / 2,
                !obstacle.facing_right
            );
        }
    }
    for (const DynamicObstacle& obstacle : dynamic_obstacles_) {
        const auto archetype = obstacle_archetype(obstacle.code);
        if (!archetype.has_value()) continue;
        const Image& image = images_.load(sprite_for(obstacle));
        canvas.image(
            image,
            static_cast<int>(obstacle.x) - image.width / 2,
            static_cast<int>(obstacle.y) - image.height / 2,
            obstacle.velocity_x16 < 0 ||
                (obstacle.velocity_x16 == 0 && !obstacle.facing_right)
        );
    }
    if (powerup_.has_value() && powerup_->active) {
        const Image& image = images_.load(sprite_for(*powerup_));
        canvas.image(
            image,
            static_cast<int>(powerup_->x) - image.width / 2,
            static_cast<int>(powerup_->y) - image.height / 2,
            powerup_->owner != 0
        );
    }
    for (const Toy& toy : toys_) {
        const std::filesystem::path sprite_path = sprite_for(toy);
        const Image& sprite = images_.load(sprite_path);
        float draw_x = toy.x;
        float draw_y = toy.y;
        if (toy.definition == 11 && toy.attached_id != 0) {
            if (const Toy* attached = find_toy(toy.attached_id); attached != nullptr) {
                const ToyParameters& parameters =
                    definitions_[attached->definition].parameters;
                const float direction = toy.owner == 0 ? 1.0F : -1.0F;
                draw_x = attached->x + direction *
                    static_cast<float>(parameters.handy_attach_x());
                draw_y = attached->y + static_cast<float>(parameters.handy_attach_y());
            }
        }
        canvas.image(
            sprite,
            static_cast<int>(draw_x) - sprite.width / 2,
            static_cast<int>(draw_y) - sprite.height / 2,
            toy.owner != 0
        );
    }
    const Image& arrow = images_.load("sprites/digit/arr_w01.png");
    for (int player = 0; player < 2; ++player) {
        const bool mirrored = player == 1;
        const int origin_x = mirrored ? arrow.width - 1 - arrow.origin_x : arrow.origin_x;
        const int anchor_x = mirrored ? stage_.right : stage_.left;
        canvas.image(
            arrow,
            anchor_x - origin_x,
            static_cast<int>(release_y_[static_cast<std::size_t>(player)]) -
                arrow.height + arrow.origin_y - 12,
            mirrored
        );
    }

    const auto draw_number = [this, &canvas](int value, int center_x, int top_y) {
        const std::wstring digits = std::to_wstring(std::max(0, value));
        std::vector<std::reference_wrapper<const Image>> images;
        int width = std::max(0, static_cast<int>(digits.size()) - 1);
        for (const wchar_t digit : digits) {
            const Image& image = images_.load(std::filesystem::path(L"sprites/digit") /
                std::format(L"digwh0{}.png", digit));
            width += image.width;
            images.emplace_back(image);
        }
        int x = center_x - width / 2;
        for (const auto image_reference : images) {
            const Image& image = image_reference.get();
            canvas.image(image, x, top_y);
            x += image.width + 1;
        }
    };
    draw_number(score_[0], 64, 8);
    draw_number(score_[1], 608, 8);
    if (duel_mode_ == DuelMode::Tournament) {
        draw_number(tournament_.lives, 320, 383);
    }
    if (duel_countdown_seconds_ > 0.0) {
        canvas.text(
            std::to_wstring(static_cast<int>(std::ceil(duel_countdown_seconds_))),
            320,
            190,
            96,
            kCream,
            1
        );
    }
}

void Game::play_music(const std::filesystem::path& relative_path) {
    audio_.play_music(relative_path);
}

void Game::stop_music() {
    audio_.stop_music();
}

void Game::play_sound(const std::filesystem::path& relative_path) {
    audio_.play_sound(relative_path);
}

void Game::play_sound_alias(std::wstring_view alias) {
    audio_.play_alias(alias);
}

void Game::play_collision_sound(int relative_speed16) {
    if (relative_speed16 <= 0) return;
    const int ordinal = relative_speed16 < 160 ? 1 : relative_speed16 < 480 ? 2 : 3;
    play_sound(scripts_.sound(L"boxer", ordinal));
}

void Game::handle_mci_notify(WPARAM status, LPARAM device_id) {
    audio_.handle_mci_notify(status, device_id);
}

void Game::toggle_sound_effects() {
    audio_.set_sound_effects_enabled(!audio_.sound_effects_enabled());
    audio_notice_ = audio_.sound_effects_enabled() ? L"Sound effects: On" : L"Sound effects: Off";
    audio_notice_seconds_ = 2.5;
}

void Game::toggle_music() {
    audio_.set_music_enabled(!audio_.music_enabled());
    audio_notice_ = audio_.music_enabled() ? L"Music: On" : L"Music: Off";
    audio_notice_seconds_ = 2.5;
}

}  // namespace gh
