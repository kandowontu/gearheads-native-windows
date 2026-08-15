#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace gh {

struct WaveFileInfo {
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t bits_per_sample = 0;
    std::size_t sample_bytes = 0;
};

std::optional<WaveFileInfo> inspect_wave_file(
    const std::filesystem::path& path, std::string& error
);

class AudioSystem {
public:
    AudioSystem(
        std::filesystem::path asset_root,
        HWND notification_window,
        std::filesystem::path state_directory
    );
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    void play_sound(const std::filesystem::path& relative_path);
    void play_alias(std::wstring_view alias);
    void play_music(const std::filesystem::path& relative_path);
    void stop_music();
    void handle_mci_notify(WPARAM status, LPARAM device_id);

    bool sound_effects_enabled() const;
    bool music_enabled() const;
    bool music_device_open() const;
    std::uint64_t completed_music_loops() const;
    void set_sound_effects_enabled(bool enabled);
    void set_music_enabled(bool enabled);

    const std::filesystem::path& diagnostics_path() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gh
