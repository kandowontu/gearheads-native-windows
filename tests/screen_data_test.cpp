#include "screens.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool interactive(const gh::ScreenCommand& command) {
    return command.key.rfind(L"utext", 0) == 0 || command.key == L"udefault" ||
           command.key == L"button";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected screens.ini path");
        const gh::ScreenDatabase database{std::filesystem::path(argv[1])};
        require(database.size() == 37, "expected 35 recovered and 2 native screens");

        const gh::ScreenDefinition* main = database.find(L"MAIN");
        require(main != nullptr, "main screen is missing or lookup is case-sensitive");
        const auto main_items = std::count_if(
            main->commands.begin(), main->commands.end(), interactive
        );
        require(main_items == 6, "main screen should expose six selectable items");
        require(
            std::any_of(main->commands.begin(), main->commands.end(), [](const auto& command) {
                return std::find(
                           command.arguments.begin(),
                           command.arguments.end(),
                           L"Duel \u2013 Human vs Computer"
                       ) != command.arguments.end();
            }),
            "UTF-8 en dash was not preserved"
        );

        const gh::ScreenDefinition* scores = database.find(L"helpscores");
        require(scores != nullptr, "helpscores screen is missing");
        require(
            std::count_if(scores->commands.begin(), scores->commands.end(), [](const auto& command) {
                return command.key == L"dtext1";
            }) == 2,
            "duplicate ordered dtext1 commands were not preserved"
        );

        const gh::ScreenDefinition* controls = database.find(L"controls");
        require(controls != nullptr, "native controls screen is missing");
        require(
            std::count_if(
                controls->commands.begin(), controls->commands.end(), interactive
            ) == 12,
            "controls screen should expose ten bindings, reset, and back"
        );
        require(std::any_of(
                    controls->commands.begin(), controls->commands.end(), [](const auto& command) {
                        return std::find(command.arguments.begin(), command.arguments.end(),
                                         L">bind$right_release") != command.arguments.end();
                    }),
                "right-player release binding is missing");

        const gh::ScreenDefinition* cheats = database.find(L"cheats");
        require(cheats != nullptr, "hidden cheat screen is missing");
        require(
            std::count_if(
                cheats->commands.begin(), cheats->commands.end(), interactive
            ) == 7,
            "cheat screen should expose five toggles, reset, and back"
        );

        const gh::ScreenDefinition* staff = database.find(L"staff");
        require(staff != nullptr, "staff screen is missing");
        require(
            std::count_if(staff->commands.begin(), staff->commands.end(), [](const auto& command) {
                return command.key == L"button";
            }) == 18,
            "staff screen should contain all 18 portrait hotspots"
        );

        const gh::ScreenDefinition* toybox = database.find(L"9p1_toys");
        require(toybox != nullptr, "one-player toybox screen is missing");
        require(
            std::count_if(toybox->commands.begin(), toybox->commands.end(), [](const auto& command) {
                return command.key == L"button";
            }) == 12,
            "one-player toybox should contain all 12 original buttons"
        );

        std::set<std::wstring> transition_targets;
        for (const std::wstring section : {
                 L"main", L"help", L"controls", L"cheats", L"p1_start", L"9p1_toys", L"dual", L"9dualtoys",
                 L"9play2toys", L"n1_option", L"n1_start", L"staff"
             }) {
            const gh::ScreenDefinition* screen = database.find(section);
            require(screen != nullptr, "required screen is missing");
            for (const gh::ScreenCommand& command : screen->commands) {
                for (const std::wstring& argument : command.arguments) {
                    if (!argument.empty() && argument.front() == L'@') {
                        transition_targets.insert(argument.substr(1));
                    }
                }
            }
        }
        for (const std::wstring& target : transition_targets) {
            require(database.find(target) != nullptr, "a screen transition has no target section");
        }
        std::cout << "validated 37 screens, duplicate commands, Unicode, and transition targets\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
