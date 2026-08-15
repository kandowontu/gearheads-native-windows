#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gh {

struct ToyParameters {
    std::array<int, 12> values{};

    int mass() const { return values[0]; }
    int horizontal_speed() const { return values[1]; }
    int movement_mode() const { return values[2]; }
    int vim_decay() const { return values[3]; }
    int primary_extra() const { return values[4]; }
    int secondary_extra() const { return values[5]; }
    int collision_front_percent() const { return values[6]; }
    int collision_top_percent() const { return values[7]; }
    int collision_back_percent() const { return values[8]; }
    int collision_bottom_percent() const { return values[9]; }
    int handy_attach_x() const { return values[10]; }
    int handy_attach_y() const { return values[11]; }
};

class DefaultsDatabase {
public:
    explicit DefaultsDatabase(const std::filesystem::path& path);

    const std::vector<int>& integers(std::string_view name) const;
    const std::string& text(std::string_view name) const;
    ToyParameters toy(std::string_view name) const;
    int scalar(std::string_view name) const;

    std::size_t numeric_count() const { return integers_.size(); }
    std::size_t string_count() const { return strings_.size(); }

private:
    std::unordered_map<std::string, std::vector<int>> integers_;
    std::unordered_map<std::string, std::string> strings_;
};

}  // namespace gh
