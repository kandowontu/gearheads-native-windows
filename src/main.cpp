#include "embedded_assets.hpp"
#include "game.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <chrono>
#include <exception>
#include <memory>
#include <string>

namespace {

struct Runtime {
    gh::Canvas canvas;
    gh::Game game;
    gh::InputState input;
    std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();

    Runtime(const std::filesystem::path& assets, HWND window) : game(assets, window) {}
};

std::unique_ptr<Runtime> runtime;

struct WindowMode {
    bool fullscreen = false;
    LONG_PTR windowed_style = 0;
    WINDOWPLACEMENT windowed_placement{};
};

WindowMode window_mode;

void toggle_fullscreen(HWND window) {
    if (!window_mode.fullscreen) {
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(MONITORINFO);
        window_mode.windowed_style = GetWindowLongPtrW(window, GWL_STYLE);
        window_mode.windowed_placement.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(window, &window_mode.windowed_placement) ||
            !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            return;
        }
        SetWindowLongPtrW(
            window, GWL_STYLE, window_mode.windowed_style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)
        );
        SetWindowPos(
            window,
            HWND_TOP,
            monitor.rcMonitor.left,
            monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        window_mode.fullscreen = true;
        return;
    }

    SetWindowLongPtrW(window, GWL_STYLE, window_mode.windowed_style);
    SetWindowPlacement(window, &window_mode.windowed_placement);
    SetWindowPos(
        window,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
    );
    window_mode.fullscreen = false;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const bool alt_enter = wparam == VK_RETURN &&
                                   ((lparam & (1LL << 29)) != 0 || GetKeyState(VK_MENU) < 0);
            if (alt_enter) {
                if ((lparam & (1LL << 30)) == 0) toggle_fullscreen(window);
                return 0;
            }
            if (runtime != nullptr && (lparam & (1LL << 30)) == 0) {
                if (wparam == VK_F9) {
                    runtime->game.toggle_sound_effects();
                    return 0;
                }
                if (wparam == VK_F10) {
                    runtime->game.toggle_music();
                    return 0;
                }
            }
            if (runtime != nullptr && wparam < runtime->input.held.size()) {
                if (!(lparam & (1LL << 30))) runtime->input.pressed[wparam] = true;
                runtime->input.held[wparam] = true;
            }
            return 0;
        }
        case MM_MCINOTIFY:
            if (runtime != nullptr) runtime->game.handle_mci_notify(wparam, lparam);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (runtime != nullptr && wparam < runtime->input.held.size()) {
                runtime->input.held[wparam] = false;
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            if (runtime != nullptr) runtime->canvas.present(dc, client);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_PRINTCLIENT: {
            RECT client{};
            GetClientRect(window, &client);
            if (runtime != nullptr) {
                runtime->canvas.present(reinterpret_cast<HDC>(wparam), client);
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    try {
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com_result)) throw std::runtime_error("COM initialization failed");

        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = 0;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        // WM_ERASEBKGND is suppressed and every paint is supplied from the
        // client-sized back buffer, so a class brush would only create an
        // avoidable black frame while showing or resizing the window.
        window_class.hbrBackground = nullptr;
        window_class.lpszClassName = L"GearheadsNativeWindow";
        if (!RegisterClassExW(&window_class)) throw std::runtime_error("Window registration failed");

        RECT desired{0, 0, 960, 720};
        AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
        HWND window = CreateWindowExW(
            0,
            window_class.lpszClassName,
            L"Gearheads - Native Windows Port 1.1.0",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            desired.right - desired.left,
            desired.bottom - desired.top,
            nullptr,
            nullptr,
            instance,
            nullptr
        );
        if (window == nullptr) throw std::runtime_error("Window creation failed");

        runtime = std::make_unique<Runtime>(gh::materialize_embedded_assets(), window);
        runtime->game.render(runtime->canvas);
        ShowWindow(window, show_command);
        UpdateWindow(window);

        MSG message{};
        bool running = true;
        constexpr double frame_interval = 1.0 / 60.0;
        while (running) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running) break;
            const auto now = std::chrono::steady_clock::now();
            const double seconds = std::chrono::duration<double>(now - runtime->previous).count();
            if (seconds < frame_interval) {
                Sleep(1);
                continue;
            }
            runtime->previous = now;
            runtime->game.update(seconds, runtime->input);
            runtime->game.render(runtime->canvas);
            runtime->input.pressed.fill(false);
            InvalidateRect(window, nullptr, FALSE);
            if (runtime->game.wants_quit()) DestroyWindow(window);
        }
        runtime.reset();
        CoUninitialize();
        return static_cast<int>(message.wParam);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Gearheads native port", MB_OK | MB_ICONERROR);
        return 1;
    }
}
