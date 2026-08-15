#include "../src/embedded_assets.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected a cache directory\n";
        return 2;
    }
    const std::filesystem::path cache_base(argv[1]);
    std::error_code error;
    std::filesystem::remove_all(cache_base, error);
    if (error) {
        std::cerr << "could not reset the test cache\n";
        return 3;
    }

    try {
        const auto assets = gh::materialize_embedded_assets(cache_base);
        const auto second = gh::materialize_embedded_assets(cache_base);
        if (assets != second || !std::filesystem::is_regular_file(assets / "manifest.json") ||
            !std::filesystem::is_regular_file(assets / "data/sprite-origins.ini") ||
            !std::filesystem::is_regular_file(assets / "fonts/gear.ttf") ||
            !std::filesystem::is_regular_file(assets / "music/fty_hi.mid")) {
            std::cerr << "materialized runtime is incomplete\n";
            return 4;
        }

        std::size_t count = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assets)) {
            if (entry.is_regular_file()) ++count;
        }
        if (count != 792) {
            std::cerr << "expected 792 embedded files, found " << count << '\n';
            return 5;
        }
        std::cout << "verified " << count << " files in " << assets.string() << '\n';
        return 0;
    } catch (const std::exception& error_message) {
        std::cerr << error_message.what() << '\n';
        return 6;
    }
}
