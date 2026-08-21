#pragma once

#include "audio.hpp"
#include "render.hpp"
#include "screens.hpp"
#include "levels.hpp"
#include "scripts.hpp"
#include "defaults.hpp"
#include "tournament.hpp"
#include "attract.hpp"
#include "champions.hpp"
#include "launch_timing.hpp"
#include "computer_ai.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gh {

enum class Screen {
    Frontend,
    Duel,
};

enum class DuelMode {
    HumanVsComputer,
    HumanVsHuman,
    Tournament,
    Attract,
};

struct InputState {
    std::array<bool, 256> pressed{};
    std::array<bool, 256> held{};
};

struct ToyDefinition {
    std::wstring name;
    std::wstring script_section;
    std::filesystem::path sprite;
    std::filesystem::path launch_sound;
    ToyParameters parameters;
};

struct Toy {
    int definition = 0;
    int owner = 0;
    float x = 0;
    float y = 0;
    int velocity_x16 = 0;
    int velocity_y16 = 0;
    int desired_x16 = 0;
    int desired_y16 = 0;
    int impulse_x16 = 0;
    int impulse_y16 = 0;
    int heading = 0;
    int winding = 0;
    int behavior_state = 0;
    int collision_layer = 0;
    int animation_state = 0;
    int animation_ticks = 0;
    int age_ticks = 0;
    std::uint64_t attached_id = 0;
    bool animation_finished = false;
    bool rocket_docked = false;
    bool exploding = false;
    bool active = true;
    std::uint64_t id = 0;
};

struct ToyBox {
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
};

struct DynamicObstacle {
    int code = 0;
    bool facing_right = false;
    float x = 0;
    float y = 0;
    int velocity_x16 = 0;
    int velocity_y16 = 0;
    int desired_x16 = 0;
    int desired_y16 = 0;
    int heading = 0;
    int animation_ticks = 0;
    int special_state = 0;
    int behavior_state = 0;
    int impulse_x16 = 0;
    int impulse_y16 = 0;
    int winding = 0x4000;
    bool transition_state = false;
};

struct Powerup {
    int owner = 0;
    int kind = 1;
    float x = 0;
    float y = 0;
    int animation_ticks = 0;
    bool active = true;
};

class Game {
public:
    Game(std::filesystem::path asset_root, HWND notification_window);
    ~Game();

    void update(double seconds, const InputState& input);
    void render(Canvas& canvas);
    bool wants_quit() const { return wants_quit_; }
    void handle_mci_notify(WPARAM status, LPARAM device_id);
    void toggle_sound_effects();
    void toggle_music();

private:
    void update_frontend(double seconds, const InputState& input);
    void update_duel(double seconds, const InputState& input);
    void advance_duel_tick();
    void update_desired_motion(Toy& toy);
    bool apply_contact_filter(Toy& source, Toy& target);
    void apply_contact_effect(Toy& source, Toy& target);
    void apply_dynamic_special_contact(Toy& source, DynamicObstacle& target);
    void apply_static_special_contact(Toy& source, const ObstacleDefinition& target);
    void apply_surface_contact(const ObstacleDefinition& obstacle, Toy& target);
    void reverse_toy(Toy& toy);
    void begin_bomby_explosion(Toy& toy);
    int animation_duration(const Toy& toy, int state) const;
    void set_animation_state(Toy& toy, int base_state);
    void advance_animation(Toy& toy);
    Toy* find_toy(std::uint64_t id);
    const Toy* find_toy(std::uint64_t id) const;
    void start_duel(DuelMode mode, const LevelDefinition* level = nullptr);
    void begin_tournament(int encoded_start);
    void start_tournament_match();
    void prepare_tournament_rosters(const LevelDefinition& level);
    void finish_tournament_match();
    void start_attract(const std::wstring& origin_screen);
    void finish_duel();
    bool release_toy(int player);
    bool spawn_toy(
        int definition,
        int owner,
        float x,
        float y,
        bool launch_sound = true,
        int winding = -1
    );
    void update_powerups();
    void apply_powerup_pickup(Powerup& powerup, Toy& target);
    void update_dynamic_obstacle(DynamicObstacle& obstacle);
    void apply_dynamic_contact(DynamicObstacle& obstacle, Toy& target);
    void randomize_toybox(int player);
    void cycle_selected_toy(int player, int direction);
    std::filesystem::path sprite_for(const Toy& toy) const;
    ToyBox collision_box(const Toy& toy);
    ToyBox collision_box(const ObstacleDefinition& obstacle);
    ToyBox collision_box(const DynamicObstacle& obstacle);
    ToyBox collision_box(const Powerup& powerup);
    std::filesystem::path sprite_for(const DynamicObstacle& obstacle) const;
    std::filesystem::path sprite_for(const Powerup& powerup) const;
    void render_frontend(Canvas& canvas);
    void enter_frontend(const std::wstring& name);
    void activate_frontend(const std::wstring& action);
    std::vector<const ScreenCommand*> interactive_commands() const;
    void render_duel(Canvas& canvas);
    void play_music(const std::filesystem::path& relative_path);
    void stop_music();
    void play_sound(const std::filesystem::path& relative_path);
    void play_sound_alias(std::wstring_view alias);
    void play_collision_sound(int relative_speed16);

    std::filesystem::path asset_root_;
    AudioSystem audio_;
    DefaultsDatabase defaults_;
    ImageCache images_;
    ScreenDatabase screens_;
    LevelDatabase levels_;
    ScriptDatabase scripts_;
    AttractDatabase attracts_;
    ChampionTable champions_;
    Screen screen_ = Screen::Frontend;
    std::wstring frontend_name_ = L"main";
    int menu_item_ = 0;
    bool wants_quit_ = false;
    DuelMode duel_mode_ = DuelMode::HumanVsComputer;
    std::array<bool, 2> computer_controlled_{true, false};
    std::vector<ToyDefinition> definitions_;
    std::vector<Toy> toys_;
    std::uint64_t next_toy_id_ = 1;
    std::optional<std::uint64_t> docked_rocket_id_;
    std::set<std::pair<std::uint64_t, std::uint64_t>> previous_toy_contacts_;
    std::vector<std::pair<int, std::filesystem::path>> scheduled_sounds_;
    std::vector<DynamicObstacle> dynamic_obstacles_;
    std::optional<Powerup> powerup_;
    std::array<std::vector<int>, 2> toyboxes_;
    std::array<int, 2> selected_toy_{0, 0};
    std::array<float, 2> release_y_{230.0F, 230.0F};
    std::array<int, 2> score_{0, 0};
    TournamentState tournament_{};
    int tournament_unlocked_level_ = 1;
    bool tournament_active_ = false;
    std::vector<int> tournament_saved_human_box_;
    bool tournament_human_box_overridden_ = false;
    int last_tournament_points_ = 0;
    wchar_t selected_level_theme_ = L'K';
    const LevelDefinition* active_level_ = nullptr;
    Rectangle stage_{};
    std::filesystem::path background_ = "backgrounds/bgk1w01a_bmp.png";
    int toybox_size_ = 12;
    int max_live_objects_ = 59;
    int friction_ = 5;
    bool ice_ = false;
    int match_winner_ = -1;
    bool powerups_enabled_ = true;
    int last_powerup_owner_ = -1;
    std::array<int, 2> powerup_effect_{0, 0};
    std::array<int, 2> powerup_effect_ticks_{0, 0};
    std::array<ComputerAiCadence, 2> ai_cadence_{};
    std::array<LaunchGauge, 2> launch_gauges_{};
    int ai_difficulty_ = 5;
    double frontend_elapsed_ = 0.0;
    double duel_elapsed_ = 0.0;
    double duel_countdown_seconds_ = 0.0;
    double attract_timeout_seconds_ = 60.0;
    std::wstring attract_anykey_target_ = L"main";
    std::wstring attract_timeout_target_ = L"main";
    std::wstring attract_match_target_ = L"main";
    const AttractSequence* active_attract_ = nullptr;
    std::size_t attract_event_ = 0;
    bool scripted_attract_ = false;
    std::wstring entered_name_;
    std::wstring audio_notice_;
    double audio_notice_seconds_ = 0.0;
    double simulation_accumulator_ = 0.0;
    std::mt19937 random_{std::random_device{}()};
};

}  // namespace gh
