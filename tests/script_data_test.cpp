#include "scripts.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("expected script.ini and asset root paths");
        const gh::ScriptDatabase database{std::filesystem::path(argv[1])};
        const std::filesystem::path assets(argv[2]);
        require(database.size() == 34, "expected all 34 original script sections");
        std::size_t frame_count = 0;
        for (const std::wstring section : {
                 L"roach", L"bomby", L"cluck", L"zappa", L"kanga", L"bigal",
                 L"destr", L"stick", L"goril", L"skull", L"magnt", L"handy"
             }) {
            const gh::ScriptSection* script = database.find(section);
            require(script != nullptr, "playable-toy script section is missing");
            const auto& walking = database.locomotion(section);
            if (walking.empty()) {
                std::wcerr << L"playable toy has no walking animation: " << section << L'\n';
                return 1;
            }
            for (const gh::AnimationFrame& frame : walking) {
                require(std::filesystem::is_regular_file(assets / frame.image), "frame PNG is missing");
                ++frame_count;
            }
            require(!script->sounds.empty(), "playable toy has no sound mapping");
        }
        require(frame_count >= 40, "too few playable walking frames were recovered");
        std::cout << "validated 34 script sections and " << frame_count
                  << " playable walking frames\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
