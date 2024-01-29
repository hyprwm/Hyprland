#include "progress/CProgressBar.hpp"
#include "helpers/Colors.hpp"
#include "core/PluginManager.hpp"
#include "core/DataState.hpp"

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
┣ update                 → Check and update all plugins if needed
┣ reload                 → Reload hyprpm state. Ensure all enabled plugins are loaded.
┣ list                   → List all installed plugins
┃
┣ Flags:
┃
┣ --notify       | -n    → Send a hyprland notification for important events (e.g. load fail)
┣ --help         | -h    → Show this menu
┣ --verbose      | -v    → Enable too much logging
┣ --force        | -f    → Force an operation ignoring checks (e.g. update -f)
┗
)#";

int               main(int argc, char** argv, char** envp) {
    std::vector<std::string> ARGS{argc};
    for (int i = 0; i < argc; ++i) {
        ARGS[i] = std::string{argv[i]};
    }

    if (ARGS.size() < 2) {
        std::cout << HELP;
        return 1;
    }

    std::vector<std::string> command;
    bool                     notify = false, verbose = false, force = false;

    for (int i = 1; i < argc; ++i) {
        if (ARGS[i].starts_with("-")) {
            if (ARGS[i] == "--help" || ARGS[i] == "-h") {
                std::cout << HELP;
                return 0;
            } else if (ARGS[i] == "--notify" || ARGS[i] == "-n") {
                notify = true;
            } else if (ARGS[i] == "--verbose" || ARGS[i] == "-v") {
                verbose = true;
            } else if (ARGS[i] == "--force" || ARGS[i] == "-f") {
                force = true;
                std::cout << Colors::RED << "!" << Colors::RESET << " Using --force, I hope you know what you are doing.\n";
            } else {
                std::cerr << "Unrecognized option " << ARGS[i];
                return 1;
            }
        } else {
            command.push_back(ARGS[i]);
        }
    }

    if (command.empty()) {
        std::cout << HELP;
        return 0;
    }

    g_pPluginManager             = std::make_unique<CPluginManager>();
    g_pPluginManager->m_bVerbose = verbose;

    if (command[0] == "add") {
        if (command.size() < 2) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for add.\n";
            return 1;
        }

        return g_pPluginManager->addNewPluginRepo(command[1]) ? 0 : 1;
    } else if (command[0] == "remove") {
        if (ARGS.size() < 2) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for remove.\n";
            return 1;
        }

        return g_pPluginManager->removePluginRepo(command[1]) ? 0 : 1;
    } else if (command[0] == "update") {
        bool headersValid = g_pPluginManager->headersValid() == HEADERS_OK;
        bool headers      = g_pPluginManager->updateHeaders(force);
        if (headers) {
            const auto HLVER            = g_pPluginManager->getHyprlandVersion();
            auto       GLOBALSTATE      = DataState::getGlobalState();
            const auto COMPILEDOUTDATED = HLVER.hash != GLOBALSTATE.headersHashCompiled;

            bool       ret1 = g_pPluginManager->updatePlugins(!headersValid || force || COMPILEDOUTDATED);

            if (!ret1)
                return 1;

            auto ret2 = g_pPluginManager->ensurePluginsLoadState();

            if (ret2 != LOADSTATE_OK)
                return 1;
        } else if (notify)
            g_pPluginManager->notify(ICON_ERROR, 0, 10000, "[hyprpm] Couldn't update headers");
    } else if (command[0] == "enable") {
        if (ARGS.size() < 2) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for enable.\n";
            return 1;
        }

        if (!g_pPluginManager->enablePlugin(command[1])) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Couldn't enable plugin (missing?)\n";
            return 1;
        }

        auto ret = g_pPluginManager->ensurePluginsLoadState();
        if (ret != LOADSTATE_OK)
            return 1;
    } else if (command[0] == "disable") {
        if (command.size() < 2) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Not enough args for disable.\n";
            return 1;
        }

        if (!g_pPluginManager->disablePlugin(command[1])) {
            std::cerr << Colors::RED << "✖" << Colors::RESET << " Couldn't disable plugin (missing?)\n";
            return 1;
        }

        auto ret = g_pPluginManager->ensurePluginsLoadState();
        if (ret != LOADSTATE_OK)
            return 1;
    } else if (command[0] == "reload") {
        auto ret = g_pPluginManager->ensurePluginsLoadState();

        if (ret != LOADSTATE_OK && notify) {
            switch (ret) {
                case LOADSTATE_FAIL:
                case LOADSTATE_PARTIAL_FAIL: g_pPluginManager->notify(ICON_ERROR, 0, 10000, "[hyprpm] Failed to load plugins"); break;
                case LOADSTATE_HEADERS_OUTDATED:
                    g_pPluginManager->notify(ICON_ERROR, 0, 10000, "[hyprpm] Failed to load plugins: Outdated headers. Please run hyprpm update manually.");
                    break;
                default: break;
            }
        } else if (notify) {
            g_pPluginManager->notify(ICON_OK, 0, 4000, "[hyprpm] Loaded plugins");
        }
    } else if (command[0] == "list") {
        g_pPluginManager->listAllPlugins();
    } else {
        std::cout << HELP;
        return 1;
    }

    return 0;
}