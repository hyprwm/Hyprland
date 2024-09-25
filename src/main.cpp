#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include "debug/HyprCtl.hpp"

#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <filesystem>

void help() {
    std::cout << "usage: Hyprland [arg [...]].\n";
    std::cout << "\nArguments:\n";
    std::cout << "  --help              -h       - Show this message again\n";
    std::cout << "  --config FILE       -c FILE  - Specify config file to use\n";
    std::cout << "  --socket NAME                - Sets the Wayland socket name (for Wayland socket handover)\n";
    std::cout << "  --wayland-fd FD              - Sets the Wayland socket fd (for Wayland socket handover)\n";
    std::cout << "  --i-am-really-stupid         - Omits root user privileges check (why would you do that?)\n";
    std::cout << "  --version           -v       - Print this binary's version\n";
}

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throwError("XDG_RUNTIME_DIR is not set!");

    // export HYPRLAND_CMD
    std::string cmd = "";
    for (auto i = 0; i < argc; ++i)
        cmd += std::string(i == 0 ? "" : " ") + argv[i];

    setenv("HYPRLAND_CMD", cmd.c_str(), 1);
    setenv("XDG_BACKEND", "wayland", 1);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);

    // parse some args
    std::string              configPath;
    std::string              socketName;
    int                      socketFd   = -1;
    bool                     ignoreSudo = false;

    std::vector<std::string> args{argv + 1, argv + argc};

    for (auto it = args.begin(); it != args.end(); it++) {
        if (it->compare("--i-am-really-stupid") == 0 && !ignoreSudo) {
            std::cout << "[ WARNING ] Running Hyprland with superuser privileges might damage your system\n";

            ignoreSudo = true;
        } else if (it->compare("--socket") == 0) {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            socketName = *std::next(it);
            it++;
        } else if (it->compare("--wayland-fd") == 0) {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            try {
                socketFd = std::stoi(std::next(it)->c_str());

                // check if socketFd is a valid file descriptor
                if (fcntl(socketFd, F_GETFD) == -1)
                    throw std::exception();
            } catch (...) {
                std::cerr << "[ ERROR ] Invalid Wayland FD!\n";
                help();

                return 1;
            }

            it++;
        } else if (it->compare("-c") == 0 || it->compare("--config") == 0) {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }
            configPath = std::next(it)->c_str();

            try {
                configPath = std::filesystem::canonical(configPath);

                if (!std::filesystem::is_regular_file(configPath)) {
                    throw std::exception();
                }
            } catch (...) {
                std::cerr << "[ ERROR ] Config file '" << configPath << "' doesn't exist!\n";
                help();

                return 1;
            }

            Debug::log(LOG, "User-specified config location: '{}'", configPath);

            it++;

            continue;
        } else if (it->compare("-h") == 0 || it->compare("--help") == 0) {
            help();

            return 0;
        } else if (it->compare("-v") == 0 || it->compare("--version") == 0) {
            auto commitMsg = trim(GIT_COMMIT_MESSAGE);
            std::replace(commitMsg.begin(), commitMsg.end(), '#', ' ');
            std::string result = "Hyprland, built from branch " + std::string(GIT_BRANCH) + " at commit " + GIT_COMMIT_HASH + " " + GIT_DIRTY + " (" + commitMsg +
                ").\nDate: " + GIT_COMMIT_DATE + "\nTag: " + GIT_TAG + ", commits: " + GIT_COMMITS + std::string{"\nbuilt against aquamarine "} + AQUAMARINE_VERSION + "\n" +
                "\n\nflags: (if any)\n";

#ifdef LEGACY_RENDERER
            result += "legacyrenderer\n";
#endif
#ifndef ISDEBUG
            result += "debug\n";
#endif
#ifdef NO_XWAYLAND
            result += "no xwayland\n";
#endif

            std::cout << result;
            return 0;
        } else if (it->compare("--systeminfo") == 0) {
            const auto SYSINFO = systemInfoRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, "");
            std::cout << SYSINFO << "\n";
            return 0;
        } else {
            std::cerr << "[ ERROR ] Unknown option '" << it->c_str() << "'!\n";
            help();

            return 1;
        }
    }

    if (!ignoreSudo && Init::isSudo()) {
        std::cerr << "[ ERROR ] Hyprland was launched with superuser privileges, but the privileges check is not omitted.\n";
        std::cerr << "          Hint: Use the --i-am-really-stupid flag to omit that check.\n";

        return 1;
    } else if (ignoreSudo && Init::isSudo()) {
        std::cout << "Superuser privileges check is omitted. I hope you know what you're doing.\n";
    }

    if (socketName.empty() ^ (socketFd == -1)) {
        std::cerr << "[ ERROR ] Hyprland was launched with only one of --socket and --wayland-fd.\n";
        std::cerr << "          Hint: Pass both --socket and --wayland-fd to perform Wayland socket handover.\n";

        return 1;
    }

    std::cout << "Welcome to Hyprland!\n";

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    try {
        g_pCompositor                     = std::make_unique<CCompositor>();
        g_pCompositor->explicitConfigPath = configPath;
    } catch (std::exception& e) {
        std::cout << "Hyprland threw in ctor: " << e.what() << "\nCannot continue.\n";
        return 1;
    }

    g_pCompositor->initServer(socketName, socketFd);

    if (!envEnabled("HYPRLAND_NO_RT"))
        Init::gainRealTime();

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    g_pCompositor->cleanup();

    Debug::log(LOG, "Hyprland has reached the end.");

    return EXIT_SUCCESS;
}
