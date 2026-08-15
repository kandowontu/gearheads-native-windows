#include "audio.hpp"

#include <mmsystem.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
gh::AudioSystem* audio = nullptr;

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == MM_MCINOTIFY && audio != nullptr) {
        audio->handle_mci_notify(wparam, lparam);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected a temporary parent directory");
        const std::filesystem::path temporary =
            std::filesystem::path(argv[1]) / std::to_string(GetCurrentProcessId());
        const std::filesystem::path assets = temporary / "assets";
        const std::filesystem::path state = temporary / "state";
        std::filesystem::create_directories(assets / "music");

        // Format-0 MIDI: 96 ticks/quarter, 500 ms of silence, then end-of-track.
        constexpr std::array<unsigned char, 33> midi{
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
            'M', 'T', 'r', 'k', 0, 0, 0, 11,
            0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20,
            96, 0xFF, 0x2F, 0,
        };
        {
            std::ofstream output(assets / "music" / "loop.mid", std::ios::binary);
            output.write(reinterpret_cast<const char*>(midi.data()), midi.size());
        }

        const wchar_t* class_name = L"GearheadsAudioLoopTest";
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = class_name;
        require(RegisterClassW(&window_class) != 0, "could not register test window");
        HWND window = CreateWindowW(
            class_name, class_name, WS_OVERLAPPED, 0, 0, 1, 1,
            nullptr, nullptr, window_class.hInstance, nullptr
        );
        require(window != nullptr, "could not create test window");

        {
            gh::AudioSystem system(assets, window, state);
            audio = &system;
            system.play_music("music/loop.mid");
            require(system.music_device_open(), "MCI could not open the synthetic MIDI");

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            MSG message{};
            while (std::chrono::steady_clock::now() < deadline &&
                   system.completed_music_loops() < 2) {
                while (system.completed_music_loops() < 2 &&
                       PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            require(
                system.completed_music_loops() >= 2,
                "MIDI completion notification did not restart playback"
            );
            system.stop_music();
            audio = nullptr;
        }
        DestroyWindow(window);
        UnregisterClassW(class_name, window_class.hInstance);
        std::filesystem::remove_all(temporary);
        std::cout << "validated notified MIDI completion and restart\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
