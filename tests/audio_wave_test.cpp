#include "audio.hpp"

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
        if (argc != 2) throw std::runtime_error("expected the sounds directory path");
        const std::filesystem::path sounds(argv[1]);
        std::size_t count = 0;
        std::size_t sample_bytes = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sounds)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".wav") continue;
            std::string error;
            const auto info = gh::inspect_wave_file(entry.path(), error);
            if (!info.has_value()) {
                throw std::runtime_error(entry.path().filename().string() + ": " + error);
            }
            require(info->channels == 1, "original effect is not mono");
            require(info->bits_per_sample == 8, "original effect is not 8-bit PCM");
            require(
                info->sample_rate == 22050 || info->sample_rate == 22255 ||
                    info->sample_rate == 44100,
                "unexpected original sample rate"
            );
            require(info->sample_bytes > 0, "effect contains no samples");
            ++count;
            sample_bytes += info->sample_bytes;
        }
        require(count == 47, "expected all 47 recovered effects");
        require(sample_bytes > 800000, "recovered effect payload is unexpectedly small");
        std::cout << "validated " << count << " PCM effects (" << sample_bytes
                  << " sample bytes)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
