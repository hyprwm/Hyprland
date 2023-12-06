#include "progress/CProgressBar.hpp"
#include "helpers/Colors.hpp"
#include "core/PluginManager.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

const std::string HELP = R"#(┏ hyprpm, a Hyprland Plugin Manager
┃
┣ add [url]              → Install a new plugin repository from git
┣ remove [url/name]      → Remove an installed plugin repository
┣ enable [name]          → Enable a plugin
┣ disable [name]         → Disable a plugin
┣ load                   → Load hyprpm state. Ensure all enabled plugins are loaded.
┗
)#";

int               main(int argc, char** argv, char** envp) {
    std::vector<std::string> ARGS{argc};
    for (int i = 0; i < argc; ++i) {
        ARGS[i] = std::string{argv[i]};
    }

    if (ARGS.size() < 2 || ARGS[1] == "--help" || ARGS[1] == "-h") {
        std::cout << HELP;
        return 1;
    }

    g_pPluginManager = std::make_unique<CPluginManager>();

    if (ARGS[1] == "add") {
        if (ARGS.size() < 3) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for add.\n";
            return 1;
        }

        return g_pPluginManager->addNewPluginRepo(ARGS[2]) ? 0 : 1;
    } else if (ARGS[1] == "remove") {
        if (ARGS.size() < 3) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for remove.\n";
            return 1;
        }

        return g_pPluginManager->removePluginRepo(ARGS[2]) ? 0 : 1;
    } else if (ARGS[1] == "update") {
        bool headersValid = g_pPluginManager->headersValid() == HEADERS_OK;
        bool headers      = g_pPluginManager->updateHeaders();
        if (headers)
            g_pPluginManager->updatePlugins(!headersValid);
    } else if (ARGS[1] == "enable") {
        if (ARGS.size() < 3) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for enable.\n";
            return 1;
        }

        if (!g_pPluginManager->enablePlugin(ARGS[2])) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Couldn't enable plugin (missing?)\n";
            return 1;
        }

        g_pPluginManager->ensurePluginsLoadState();
    } else if (ARGS[1] == "disable") {
        if (ARGS.size() < 3) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for disable.\n";
            return 1;
        }

        if (!g_pPluginManager->disablePlugin(ARGS[2])) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Couldn't disable plugin (missing?)\n";
            return 1;
        }

        g_pPluginManager->ensurePluginsLoadState();
    } else if (ARGS[1] == "load") {
        g_pPluginManager->ensurePluginsLoadState();
    } else {
        std::cout << HELP;
        return 1;
    }

    return 0;
}