#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include "debug/HyprCtl.hpp"

#include <cstdio>
#include <hyprutils/string/String.hpp>
#include <print>
using namespace Hyprutils::String;

#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <filesystem>

static void help() {
    std::println("usage: Hyprland [arg [...]].\n");
    std::println(R"(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use
    --socket NAME                - Sets the Wayland socket name (for Wayland socket handover)
    --wayland-fd FD              - Sets the Wayland socket fd (for Wayland socket handover)
    --systeminfo                 - Prints system infos
    --i-am-really-stupid         - Omits root user privileges check (why would you do that?)
    --version           -v       - Print this binary's version)");
}

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throwError("XDG_RUNTIME_DIR is not set!");

    // export HYPRLAND_CMD
    std::string cmd = argv[0];
    for (int i = 1; i < argc; ++i)
        cmd += std::string(" ") + argv[i];

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
        if (*it == "--i-am-really-stupid" && !ignoreSudo) {
            std::println("[ WARNING ] Running Hyprland with superuser privileges might damage your system");

            ignoreSudo = true;
        } else if (*it == "--socket") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            socketName = *std::next(it);
            it++;
        } else if (*it == "--wayland-fd") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            try {
                socketFd = std::stoi(*std::next(it));

                // check if socketFd is a valid file descriptor
                if (fcntl(socketFd, F_GETFD) == -1)
                    throw std::exception();
            } catch (...) {
                std::println(stderr, "[ ERROR ] Invalid Wayland FD!");
                help();

                return 1;
            }

            it++;
        } else if (*it == "-c" || *it == "--config") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }
            configPath = *std::next(it);

            try {
                configPath = std::filesystem::canonical(configPath);

                if (!std::filesystem::is_regular_file(configPath)) {
                    throw std::exception();
                }
            } catch (...) {
                std::println(stderr, "[ ERROR ] Config file '{}' doesn't exist!", configPath);
                help();

                return 1;
            }

            Debug::log(LOG, "User-specified config location: '{}'", configPath);

            it++;

            continue;
        } else if (*it == "-h" || *it == "--help") {
            help();

            return 0;
        } else if (*it == "-v" || *it == "--version") {
            std::println("{}", versionRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, ""));
            return 0;
        } else if (*it == "--systeminfo") {
            std::println("{}", systemInfoRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, ""));
            return 0;
        } else {
            std::println(stderr, "[ ERROR ] Unknown option '{}' !", *it);
            help();

            return 1;
        }
    }

    if (!ignoreSudo && NInit::isSudo()) {
        std::println(stderr,
                     "[ ERROR ] Hyprland was launched with superuser privileges, but the privileges check is not omitted.\n"
                     "          Hint: Use the --i-am-really-stupid flag to omit that check.");

        return 1;
    } else if (ignoreSudo && NInit::isSudo()) {
        std::println("Superuser privileges check is omitted. I hope you know what you're doing.");
    }

    if (socketName.empty() ^ (socketFd == -1)) {
        std::println(stderr,
                     "[ ERROR ] Hyprland was launched with only one of --socket and --wayland-fd.\n"
                     "          Hint: Pass both --socket and --wayland-fd to perform Wayland socket handover.");

        return 1;
    }

    std::println("Welcome to Hyprland!");

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    try {
        g_pCompositor                     = makeUnique<CCompositor>();
        g_pCompositor->explicitConfigPath = configPath;
    } catch (const std::exception& e) {
        std::println(stderr, "Hyprland threw in ctor: {}\nCannot continue.", e.what());
        return 1;
    }

    g_pCompositor->initServer(socketName, socketFd);

    if (!envEnabled("HYPRLAND_NO_RT"))
        NInit::gainRealTime();

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    g_pCompositor->cleanup();

    Debug::log(LOG, "Hyprland has reached the end.");

    return EXIT_SUCCESS;
}
