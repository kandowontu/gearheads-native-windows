#include "audio.hpp"

#include <mmsystem.h>
#include <dsound.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cwctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gh {
namespace {

constexpr std::size_t kMaximumVoices = 64;

struct DecodedWave {
    WAVEFORMATEX format{};
    std::vector<std::uint8_t> samples;
};

std::uint16_t little_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U)
    );
}

std::uint32_t little_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

bool chunk_name(
    const std::vector<std::uint8_t>& bytes, std::size_t offset, const char (&expected)[5]
) {
    return offset + 4 <= bytes.size() &&
           std::memcmp(bytes.data() + offset, expected, 4) == 0;
}

std::optional<DecodedWave> decode_wave_file(
    const std::filesystem::path& path, std::string& error
) {
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open the file";
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()
    );
    if (bytes.size() < 12 || !chunk_name(bytes, 0, "RIFF") ||
        !chunk_name(bytes, 8, "WAVE")) {
        error = "not a RIFF/WAVE file";
        return std::nullopt;
    }
    const std::uint64_t riff_end = static_cast<std::uint64_t>(little_u32(bytes, 4)) + 8U;
    if (riff_end > bytes.size()) {
        error = "truncated RIFF payload";
        return std::nullopt;
    }

    DecodedWave decoded;
    bool found_format = false;
    bool found_samples = false;
    std::size_t cursor = 12;
    while (cursor + 8 <= riff_end) {
        const std::uint32_t chunk_size = little_u32(bytes, cursor + 4);
        const std::uint64_t data_start = static_cast<std::uint64_t>(cursor) + 8U;
        const std::uint64_t data_end = data_start + chunk_size;
        if (data_end > riff_end || data_end > bytes.size()) {
            error = "truncated WAVE chunk";
            return std::nullopt;
        }
        if (chunk_name(bytes, cursor, "fmt ") && !found_format) {
            if (chunk_size < 16) {
                error = "short WAVE format chunk";
                return std::nullopt;
            }
            const std::size_t start = static_cast<std::size_t>(data_start);
            decoded.format.wFormatTag = little_u16(bytes, start);
            decoded.format.nChannels = little_u16(bytes, start + 2);
            decoded.format.nSamplesPerSec = little_u32(bytes, start + 4);
            decoded.format.nAvgBytesPerSec = little_u32(bytes, start + 8);
            decoded.format.nBlockAlign = little_u16(bytes, start + 12);
            decoded.format.wBitsPerSample = little_u16(bytes, start + 14);
            decoded.format.cbSize = 0;
            found_format = true;
        } else if (chunk_name(bytes, cursor, "data") && !found_samples) {
            decoded.samples.assign(
                bytes.begin() + static_cast<std::ptrdiff_t>(data_start),
                bytes.begin() + static_cast<std::ptrdiff_t>(data_end)
            );
            found_samples = true;
        }
        const std::uint64_t next = data_end + (chunk_size & 1U);
        if (next > std::numeric_limits<std::size_t>::max()) {
            error = "WAVE chunk offset overflow";
            return std::nullopt;
        }
        cursor = static_cast<std::size_t>(next);
    }

    if (!found_format || !found_samples) {
        error = "missing format or sample-data chunk";
        return std::nullopt;
    }
    if (decoded.format.wFormatTag != WAVE_FORMAT_PCM || decoded.format.nChannels == 0 ||
        decoded.format.nSamplesPerSec == 0 || decoded.format.nBlockAlign == 0 ||
        decoded.format.wBitsPerSample == 0 || decoded.samples.empty()) {
        error = "unsupported or empty PCM WAVE file";
        return std::nullopt;
    }
    const std::uint32_t expected_alignment =
        static_cast<std::uint32_t>(decoded.format.nChannels) *
        static_cast<std::uint32_t>(decoded.format.wBitsPerSample) / 8U;
    if (expected_alignment != decoded.format.nBlockAlign ||
        decoded.format.nAvgBytesPerSec !=
            decoded.format.nSamplesPerSec * decoded.format.nBlockAlign ||
        decoded.samples.size() % decoded.format.nBlockAlign != 0) {
        error = "inconsistent PCM WAVE format";
        return std::nullopt;
    }
    return decoded;
}

std::wstring normalized_relative(std::filesystem::path value) {
    std::wstring key = value.lexically_normal().generic_wstring();
    std::transform(key.begin(), key.end(), key.begin(), towlower);
    return key;
}

std::wstring lower_wide(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::string hexadecimal(unsigned long value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << value;
    return stream.str();
}

}  // namespace

std::optional<WaveFileInfo> inspect_wave_file(
    const std::filesystem::path& path, std::string& error
) {
    const auto decoded = decode_wave_file(path, error);
    if (!decoded.has_value()) return std::nullopt;
    return WaveFileInfo{
        decoded->format.nChannels,
        decoded->format.nSamplesPerSec,
        decoded->format.wBitsPerSample,
        decoded->samples.size(),
    };
}

class AudioSystem::Impl {
public:
    Impl(
        std::filesystem::path asset_root,
        HWND notification_window,
        std::filesystem::path state_directory
    )
        : asset_root_(std::move(asset_root)),
          notification_window_(notification_window),
          state_directory_(std::move(state_directory)) {
        if (!state_directory_.empty()) {
            std::error_code ignored;
            std::filesystem::create_directories(state_directory_, ignored);
            settings_path_ = state_directory_ / L"audio.ini";
            diagnostics_path_ = state_directory_ / L"audio.log";
        }
        log("audio session started");
        load_settings();
        load_sound_aliases();
        initialize_direct_sound();
    }

    ~Impl() {
        close_music_device();
        stop_effect_voices();
        for (auto& [key, buffer] : effects_) {
            (void)key;
            if (buffer != nullptr) buffer->Release();
        }
        effects_.clear();
        if (direct_sound_ != nullptr) direct_sound_->Release();
        log("audio session ended");
    }

    void play_sound(const std::filesystem::path& relative_path) {
        if (!sound_effects_enabled_ || direct_sound_ == nullptr) return;
        reap_effect_voices();
        const auto found = effects_.find(normalized_relative(relative_path));
        if (found == effects_.end()) {
            log("sound not loaded: " + relative_path.generic_string());
            return;
        }
        if (voices_.size() >= kMaximumVoices) {
            voices_.front()->Stop();
            voices_.front()->Release();
            voices_.erase(voices_.begin());
        }
        IDirectSoundBuffer* voice = nullptr;
        HRESULT result = direct_sound_->DuplicateSoundBuffer(found->second, &voice);
        if (FAILED(result) || voice == nullptr) {
            log("DuplicateSoundBuffer failed: " + hexadecimal(static_cast<unsigned long>(result)));
            return;
        }
        voice->SetCurrentPosition(0);
        result = voice->Play(0, 0, 0);
        if (FAILED(result)) {
            log("sound playback failed: " + hexadecimal(static_cast<unsigned long>(result)));
            voice->Release();
            return;
        }
        voices_.push_back(voice);
    }

    void play_alias(std::wstring_view alias) {
        const auto found = sound_aliases_.find(lower_wide(std::wstring(alias)));
        if (found == sound_aliases_.end()) {
            log("unknown sound alias");
            return;
        }
        play_sound(found->second);
    }

    void play_music(const std::filesystem::path& relative_path) {
        desired_music_ = relative_path;
        close_music_device();
        if (!music_enabled_ || desired_music_.empty()) return;
        const std::filesystem::path absolute = asset_root_ / desired_music_;
        std::wstring element = absolute.wstring();
        const DWORD short_length = GetShortPathNameW(element.c_str(), nullptr, 0);
        if (short_length > 0) {
            std::wstring short_element(static_cast<std::size_t>(short_length), L'\0');
            const DWORD written = GetShortPathNameW(
                element.c_str(), short_element.data(), static_cast<DWORD>(short_element.size())
            );
            if (written > 0 && written < short_element.size()) {
                short_element.resize(written);
                element = std::move(short_element);
            }
        }
        MCI_OPEN_PARMSW parameters{};
        parameters.lpstrDeviceType = L"sequencer";
        parameters.lpstrElementName = element.c_str();
        const MCIERROR open_error = mciSendCommandW(
            0,
            MCI_OPEN,
            MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_WAIT,
            reinterpret_cast<DWORD_PTR>(&parameters)
        );
        if (open_error != 0) {
            log_mci_error("open music", open_error);
            return;
        }
        music_device_id_ = parameters.wDeviceID;
        if (music_device_id_ == 0) {
            log("MCI returned no device id for the opened music track");
            return;
        }
        start_music_from_beginning();
    }

    void stop_music() {
        desired_music_.clear();
        close_music_device();
    }

    void handle_mci_notify(WPARAM status, LPARAM device_id) {
        if (static_cast<MCIDEVICEID>(device_id) != music_device_id_ || music_device_id_ == 0) {
            return;
        }
        if (status == MCI_NOTIFY_SUCCESSFUL) {
            ++completed_music_loops_;
            if (music_enabled_ && !desired_music_.empty()) start_music_from_beginning();
        } else if (status == MCI_NOTIFY_FAILURE) {
            log("MCI reported a music playback failure");
            close_music_device();
        }
    }

    bool sound_effects_enabled() const { return sound_effects_enabled_; }
    bool music_enabled() const { return music_enabled_; }
    bool music_device_open() const { return music_device_id_ != 0; }
    std::uint64_t completed_music_loops() const { return completed_music_loops_; }

    void set_sound_effects_enabled(bool enabled) {
        if (sound_effects_enabled_ == enabled) return;
        sound_effects_enabled_ = enabled;
        if (!enabled) stop_effect_voices();
        save_settings();
        log(std::string("sound effects ") + (enabled ? "enabled" : "disabled"));
    }

    void set_music_enabled(bool enabled) {
        if (music_enabled_ == enabled) return;
        music_enabled_ = enabled;
        if (enabled) {
            if (!desired_music_.empty()) play_music(desired_music_);
        } else {
            close_music_device();
        }
        save_settings();
        log(std::string("music ") + (enabled ? "enabled" : "disabled"));
    }

    const std::filesystem::path& diagnostics_path() const { return diagnostics_path_; }

private:
    void initialize_direct_sound() {
        HRESULT result = DirectSoundCreate8(nullptr, &direct_sound_, nullptr);
        if (FAILED(result) || direct_sound_ == nullptr) {
            log("DirectSoundCreate8 failed; effects are unavailable: " +
                hexadecimal(static_cast<unsigned long>(result)));
            return;
        }
        result = direct_sound_->SetCooperativeLevel(notification_window_, DSSCL_NORMAL);
        if (FAILED(result)) {
            log("DirectSound cooperative level failed; effects are unavailable: " +
                hexadecimal(static_cast<unsigned long>(result)));
            direct_sound_->Release();
            direct_sound_ = nullptr;
            return;
        }

        const std::filesystem::path directory = asset_root_ / L"sounds";
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
            if (error) break;
            if (!entry.is_regular_file() || entry.path().extension() != L".wav") continue;
            std::string decode_error;
            const auto wave = decode_wave_file(entry.path(), decode_error);
            if (!wave.has_value()) {
                log("could not load " + entry.path().filename().string() + ": " + decode_error);
                continue;
            }
            if (wave->samples.size() > std::numeric_limits<DWORD>::max()) {
                log("sound is too large for DirectSound: " + entry.path().filename().string());
                continue;
            }
            WAVEFORMATEX format = wave->format;
            DSBUFFERDESC description{};
            description.dwSize = sizeof(description);
            description.dwFlags = DSBCAPS_STATIC | DSBCAPS_GETCURRENTPOSITION2;
            description.dwBufferBytes = static_cast<DWORD>(wave->samples.size());
            description.lpwfxFormat = &format;
            IDirectSoundBuffer* buffer = nullptr;
            result = direct_sound_->CreateSoundBuffer(&description, &buffer, nullptr);
            if (FAILED(result) || buffer == nullptr) {
                log("CreateSoundBuffer failed for " + entry.path().filename().string() + ": " +
                    hexadecimal(static_cast<unsigned long>(result)));
                continue;
            }
            void* first = nullptr;
            void* second = nullptr;
            DWORD first_size = 0;
            DWORD second_size = 0;
            result = buffer->Lock(
                0,
                description.dwBufferBytes,
                &first,
                &first_size,
                &second,
                &second_size,
                0
            );
            if (FAILED(result)) {
                log("DirectSound buffer lock failed for " + entry.path().filename().string());
                buffer->Release();
                continue;
            }
            std::memcpy(first, wave->samples.data(), first_size);
            if (second != nullptr && second_size > 0) {
                std::memcpy(second, wave->samples.data() + first_size, second_size);
            }
            buffer->Unlock(first, first_size, second, second_size);
            const std::filesystem::path relative =
                std::filesystem::relative(entry.path(), asset_root_, error);
            if (error) {
                buffer->Release();
                error.clear();
                continue;
            }
            effects_.emplace(normalized_relative(relative), buffer);
        }
        if (error) log("could not enumerate the embedded sound cache: " + error.message());
        log("DirectSound initialized with " + std::to_string(effects_.size()) + " effects");
    }

    void reap_effect_voices() {
        auto next = voices_.begin();
        while (next != voices_.end()) {
            DWORD status = 0;
            const HRESULT result = (*next)->GetStatus(&status);
            if (FAILED(result) || (status & DSBSTATUS_PLAYING) == 0) {
                (*next)->Release();
                next = voices_.erase(next);
            } else {
                ++next;
            }
        }
    }

    void stop_effect_voices() {
        for (IDirectSoundBuffer* voice : voices_) {
            voice->Stop();
            voice->Release();
        }
        voices_.clear();
    }

    void start_music_from_beginning() {
        if (music_device_id_ == 0) return;
        MCI_PLAY_PARMS parameters{};
        parameters.dwCallback = reinterpret_cast<DWORD_PTR>(notification_window_);
        parameters.dwFrom = 0;
        const MCIERROR error = mciSendCommandW(
            music_device_id_,
            MCI_PLAY,
            MCI_FROM | MCI_NOTIFY,
            reinterpret_cast<DWORD_PTR>(&parameters)
        );
        if (error != 0) {
            log_mci_error("play music", error);
            close_music_device();
        }
    }

    void close_music_device() {
        if (music_device_id_ == 0) return;
        const MCIDEVICEID closing = music_device_id_;
        music_device_id_ = 0;
        const MCIERROR stop_error = mciSendCommandW(closing, MCI_STOP, MCI_WAIT, 0);
        if (stop_error != 0) log_mci_error("stop music", stop_error);
        const MCIERROR close_error = mciSendCommandW(closing, MCI_CLOSE, MCI_WAIT, 0);
        if (close_error != 0) log_mci_error("close music", close_error);
    }

    void load_settings() {
        std::ifstream input(settings_path_);
        std::string line;
        while (std::getline(input, line)) {
            if (line == "sound=0") sound_effects_enabled_ = false;
            if (line == "sound=1") sound_effects_enabled_ = true;
            if (line == "music=0") music_enabled_ = false;
            if (line == "music=1") music_enabled_ = true;
        }
    }

    void load_sound_aliases() {
        std::ifstream input(asset_root_ / L"data" / L"gearhead.ini");
        std::string line;
        bool in_sound_section = false;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty() && line.front() == '[' && line.back() == ']') {
                std::string section = line.substr(1, line.size() - 2);
                std::transform(section.begin(), section.end(), section.begin(),
                               [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                in_sound_section = section == "sound";
                continue;
            }
            if (!in_sound_section || line.empty() || line.front() == ';') continue;
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos || equals + 2 > line.size() ||
                line[equals + 1] != '@') {
                continue;
            }
            std::string key = line.substr(0, equals);
            std::string resource = line.substr(equals + 2);
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            std::transform(resource.begin(), resource.end(), resource.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            sound_aliases_.emplace(
                std::wstring(key.begin(), key.end()),
                std::filesystem::path(L"sounds") /
                    (std::wstring(resource.begin(), resource.end()) + L".wav")
            );
        }
        log("loaded " + std::to_string(sound_aliases_.size()) + " sound aliases");
    }

    void save_settings() {
        if (settings_path_.empty()) return;
        std::ofstream output(settings_path_, std::ios::trunc);
        if (!output) {
            log("could not save audio settings");
            return;
        }
        output << "sound=" << (sound_effects_enabled_ ? 1 : 0) << '\n'
               << "music=" << (music_enabled_ ? 1 : 0) << '\n';
    }

    void log_mci_error(const std::string& operation, MCIERROR error) {
        std::array<wchar_t, 256> message{};
        std::string detail;
        if (mciGetErrorStringW(error, message.data(), static_cast<UINT>(message.size()))) {
            const int bytes = WideCharToMultiByte(
                CP_UTF8, 0, message.data(), -1, nullptr, 0, nullptr, nullptr
            );
            if (bytes > 1) {
                detail.resize(static_cast<std::size_t>(bytes));
                WideCharToMultiByte(
                    CP_UTF8, 0, message.data(), -1, detail.data(), bytes, nullptr, nullptr
                );
                detail.pop_back();
            }
        }
        log(operation + " failed (" + std::to_string(error) + ")" +
            (detail.empty() ? std::string{} : ": " + detail));
    }

    void log(const std::string& message) const {
        if (diagnostics_path_.empty()) return;
        std::ofstream output(diagnostics_path_, std::ios::app);
        if (!output) return;
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
        localtime_s(&local, &now);
        output << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << "  " << message << '\n';
    }

    std::filesystem::path asset_root_;
    HWND notification_window_ = nullptr;
    std::filesystem::path state_directory_;
    std::filesystem::path settings_path_;
    std::filesystem::path diagnostics_path_;
    IDirectSound8* direct_sound_ = nullptr;
    std::unordered_map<std::wstring, IDirectSoundBuffer*> effects_;
    std::unordered_map<std::wstring, std::filesystem::path> sound_aliases_;
    std::vector<IDirectSoundBuffer*> voices_;
    bool sound_effects_enabled_ = true;
    bool music_enabled_ = true;
    std::filesystem::path desired_music_;
    MCIDEVICEID music_device_id_ = 0;
    std::uint64_t completed_music_loops_ = 0;
};

AudioSystem::AudioSystem(
    std::filesystem::path asset_root,
    HWND notification_window,
    std::filesystem::path state_directory
)
    : impl_(std::make_unique<Impl>(
          std::move(asset_root), notification_window, std::move(state_directory)
      )) {}

AudioSystem::~AudioSystem() = default;

void AudioSystem::play_sound(const std::filesystem::path& relative_path) {
    impl_->play_sound(relative_path);
}

void AudioSystem::play_alias(std::wstring_view alias) { impl_->play_alias(alias); }

void AudioSystem::play_music(const std::filesystem::path& relative_path) {
    impl_->play_music(relative_path);
}

void AudioSystem::stop_music() { impl_->stop_music(); }

void AudioSystem::handle_mci_notify(WPARAM status, LPARAM device_id) {
    impl_->handle_mci_notify(status, device_id);
}

bool AudioSystem::sound_effects_enabled() const { return impl_->sound_effects_enabled(); }
bool AudioSystem::music_enabled() const { return impl_->music_enabled(); }
bool AudioSystem::music_device_open() const { return impl_->music_device_open(); }
std::uint64_t AudioSystem::completed_music_loops() const {
    return impl_->completed_music_loops();
}

void AudioSystem::set_sound_effects_enabled(bool enabled) {
    impl_->set_sound_effects_enabled(enabled);
}

void AudioSystem::set_music_enabled(bool enabled) { impl_->set_music_enabled(enabled); }

const std::filesystem::path& AudioSystem::diagnostics_path() const {
    return impl_->diagnostics_path();
}

}  // namespace gh
