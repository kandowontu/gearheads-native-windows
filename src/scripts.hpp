#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace gh {

struct AnimationFrame {
    std::filesystem::path image;
    int ticks = 1;
    std::vector<int> parameters;
};

struct ScriptSection {
    std::wstring name;
    std::wstring archive;
    std::unordered_map<std::wstring, std::vector<AnimationFrame>> animations;
    std::unordered_map<int, std::filesystem::path> sounds;
};

class ScriptDatabase {
public:
    explicit ScriptDatabase(const std::filesystem::path& path);

    const ScriptSection* find(const std::wstring& name) const;
    const std::vector<AnimationFrame>& animation(
        const std::wstring& section, const std::wstring& state
    ) const;
    const std::vector<AnimationFrame>& locomotion(const std::wstring& section) const;
    std::filesystem::path sound(const std::wstring& section, int ordinal) const;
    std::size_t size() const { return sections_.size(); }

private:
    std::vector<ScriptSection> sections_;
    std::unordered_map<std::wstring, std::size_t> index_;
};

}  // namespace gh
