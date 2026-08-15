#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace gh {

struct AttractLane {
    int definition = 0;
    int winding = 0;
};

struct AttractEvent {
    int timestamp_ms = 0;
    int player = 0;
    std::array<AttractLane, 5> lanes{};
};

struct AttractSequence {
    std::wstring name;
    std::vector<AttractEvent> events;
};

class AttractDatabase {
public:
    explicit AttractDatabase(const std::filesystem::path& path);

    const std::vector<AttractSequence>& sequences() const { return sequences_; }

private:
    std::vector<AttractSequence> sequences_;
};

}  // namespace gh
