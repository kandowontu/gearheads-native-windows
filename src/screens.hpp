#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace gh {

struct ScreenCommand {
    std::wstring key;
    std::vector<std::wstring> arguments;
    int source_line = 0;
};

struct ScreenDefinition {
    std::wstring name;
    std::vector<ScreenCommand> commands;
};

class ScreenDatabase {
public:
    explicit ScreenDatabase(const std::filesystem::path& path);

    const ScreenDefinition* find(const std::wstring& name) const;
    std::size_t size() const { return screens_.size(); }

private:
    std::vector<ScreenDefinition> screens_;
    std::unordered_map<std::wstring, std::size_t> index_;
};

}  // namespace gh
