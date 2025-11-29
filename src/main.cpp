#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include "debug/HyprCtl.hpp"

#include <csignal>
#include <cstdio>
#include <hyprutils/string/String.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <print>
using namespace Hyprutils::String;
using namespace Hyprutils::Memory;

#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <string_view>
#include <span>
#include <filesystem>

static void help() {
    std::println("usage: Hyprland [arg [...]].\n");
    std::println(R"#(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use
    --socket NAME                - Sets the Wayland socket name (for Wayland socket handover)
    --wayland-fd FD              - Sets the Wayland socket fd (for Wayland socket handover)
    --systeminfo                 - Prints system infos
    --i-am-really-stupid         - Omits root user privileges check (why would you do that?)
    --verify-config              - Do not run Hyprland, only print if the config has any errors
    --version           -v       - Print this binary's version
    --version-json               - Print this binary's version as json)#");
}

static void reapZombieChildrenAutomatically() {
    struct sigaction act;
    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDWAIT;
#ifdef SA_RESTORER
    act.sa_restorer = NULL;
#endif
    sigaction(SIGCHLD, &act, nullptr);
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
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);

    // parse some args
    std::string configPath;
    std::string socketName;
    int         socketFd   = -1;
    bool        ignoreSudo = false, verifyConfig = false;

    if (argc > 1) {
        std::span<char*> args{argv + 1, sc<std::size_t>(argc - 1)};

        for (auto it = args.begin(); it != args.end(); it++) {
            std::string_view value = *it;

            if (value == "--i-am-really-stupid" && !ignoreSudo) {
                std::println("[ WARNING ] Running Hyprland with superuser privileges might damage your system");

                ignoreSudo = true;
            } else if (value == "--socket") {
                if (std::next(it) == args.end()) {
                    help();

                    return 1;
                }

                socketName = *std::next(it);
                it++;
            } else if (value == "--wayland-fd") {
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
            } else if (value == "-c" || value == "--config") {
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
            } else if (value == "-h" || value == "--help") {
                help();

                return 0;
            } else if (value == "-v" || value == "--version") {
                std::println("{}", versionRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, ""));
                return 0;
            } else if (value == "--version-json") {
                std::println("{}", versionRequest(eHyprCtlOutputFormat::FORMAT_JSON, ""));
                return 0;
            } else if (value == "--systeminfo") {
                std::println("{}", systemInfoRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, ""));
                return 0;
            } else if (value == "--verify-config") {
                verifyConfig = true;
                continue;
            } else {
                std::println(stderr, "[ ERROR ] Unknown option '{}' !", value);
                help();

                return 1;
            }
        }
    }

    if (!ignoreSudo && NInit::isSudo()) {
        std::println(stderr,
                     "[ ERROR ] Hyprland was launched with superuser privileges, but the privileges check is not omitted.\n"
                     "          Hint: Use the --i-am-really-stupid flag to omit that check.");

        return 1;
    } else if (ignoreSudo && NInit::isSudo())
        std::println("Superuser privileges check is omitted. I hope you know what you're doing.");

    if (socketName.empty() ^ (socketFd == -1)) {
        std::println(stderr,
                     "[ ERROR ] Hyprland was launched with only one of --socket and --wayland-fd.\n"
                     "          Hint: Pass both --socket and --wayland-fd to perform Wayland socket handover.");

        return 1;
    }

    if (!verifyConfig)
        std::println("Welcome to Hyprland!");

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    try {
        g_pCompositor                       = makeUnique<CCompositor>(verifyConfig);
        g_pCompositor->m_explicitConfigPath = configPath;
    } catch (const std::exception& e) {
        std::println(stderr, "Hyprland threw in ctor: {}\nCannot continue.", e.what());
        return 1;
    }

    reapZombieChildrenAutomatically();

    g_pCompositor->initServer(socketName, socketFd);

    if (verifyConfig)
        return !g_pConfigManager->m_lastConfigVerificationWasSuccessful;

    if (!envEnabled("HYPRLAND_NO_RT"))
        NInit::gainRealTime();

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    g_pCompositor->cleanup();

    g_pCompositor.reset();

    Debug::log(LOG, "Hyprland has reached the end.");

    return EXIT_SUCCESS;
}
