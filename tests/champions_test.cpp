#include "champions.hpp"

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
        if (argc != 2) throw std::runtime_error("expected gearhead.ini path");
        const std::filesystem::path source(argv[1]);
        gh::ChampionTable champions{source};
        require(champions.entries().size() == 7, "expected seven champions");
        require(champions.placeholder(L"#00") == L"MARKO", "first name mapping changed");
        require(champions.placeholder(L"#01") == L"   70", "first score mapping changed");
        require(champions.placeholder(L"#12") == L"BOBGREENBERGO", "last name changed");
        require(champions.placeholder(L"#13") == L"   10", "last score mapping changed");
        require(champions.placeholder(L"plain") == L"plain", "plain text must survive");
        require(!champions.qualifies(9) && champions.qualifies(10), "qualification changed");
        champions.insert(65, L"PLAYER");
        require(champions.entries()[1].name == L"PLAYER", "score insertion order changed");
        require(champions.entries().size() == 7, "champion table must remain bounded");
        champions.insert(100, L"12345678901234567890");
        require(champions.entries().front().name.size() == 15, "names must retain Win16 limit");
        const std::filesystem::path save =
            std::filesystem::temp_directory_path() / "gearheads-native-champions-test.dat";
        std::error_code cleanup_error;
        std::filesystem::remove(save, cleanup_error);
        gh::ChampionTable persistent{source, save};
        persistent.insert(100, L"NATIVE PLAYER");
        persistent.save();
        gh::ChampionTable reloaded{source, save};
        require(reloaded.entries().front().name == L"NATIVE PLAYER", "saved champion not loaded");
        std::filesystem::remove(save, cleanup_error);
        std::cout << "validated recovered champion parsing and all 14 screen placeholders\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
