#include "attract.hpp"

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
        if (argc != 2) throw std::runtime_error("expected anim.dat path");
        const gh::AttractDatabase database{std::filesystem::path(argv[1])};
        require(database.sequences().size() == 14, "expected all 14 attract sequences");
        const auto& first = database.sequences().front();
        require(first.name == L"star wars", "first attract title changed");
        require(!first.events.empty() && first.events.front().timestamp_ms == 500,
                "first attract timestamp changed");
        std::size_t events = 0;
        for (const auto& sequence : database.sequences()) events += sequence.events.size();
        require(events == 281, "attract event count changed");
        std::cout << "validated 14 native attract sequences and " << events << " events\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
