#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include "debug/HyprCtl.hpp"

#include <cstdlib>
#include <hyprutils/string/String.hpp>
#include <print>
using namespace Hyprutils::String;

#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <filesystem>

void help() {
    std::print("usage: Hyprland [arg [...]].\n\n");
    std::print(R"(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use
    --socket NAME       -s NAME  - Sets the Wayland socket name (for Wayland socket handover)
    --wayland-fd FD     -F FD    - Sets the Wayland socket fd (for Wayland socket handover)
    --i-am-really-stupid         - Omits root user privileges check (why would you do that?)
    --systeminfo        -i       - Prints system infos
    --version           -v       - Print this binary's version)");
}

void version()
{
    auto commitMsg = trim(GIT_COMMIT_MESSAGE);
    std::replace(commitMsg.begin(), commitMsg.end(), '#', ' ');
    std::print("Hyprland {} built from branch {} at commit {} {} ({}).\n"
               "Date: {}\n"
               "Tag: {}, commits: {}\n"
               "built against aquamarine {}\n\n\n",
               HYPRLAND_VERSION, GIT_BRANCH, GIT_COMMIT_HASH, GIT_DIRTY, commitMsg,
               GIT_COMMIT_DATE, GIT_TAG, GIT_COMMITS, AQUAMARINE_VERSION);
#if (!defined(LEGACY_RENDERER) && !defined(ISDEBUG) && !defined(NO_XWAYLAND))
    std::print("no flags were set\n");
#else
    std::print("flags set:\n");
# ifdef LEGACY_RENDERER
    std::print("legacyrenderer\n");
# endif
# ifdef ISDEBUG
    std::print("debug\n");
# endif
# ifdef NO_XWAYLAND
    std::print("no xwayland\n");
# endif
#endif
}

int main(int argc, char *argv[]) {

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
    std::string  configPath;
    std::string  socketName;
    int          socketFd   = -1;
    bool         ignoreSudo = false;

    int opt = 0;
    int option_index = 0;
    const char *optstring = "-hvic:s:F:";
    static const struct option opts[] = {
        {"version",            no_argument,       0, 'v'},
        {"help",               no_argument,       0, 'h'},
        {"systeminfo",         no_argument,       0, 'i'},
        {"i-am-really-stupid", no_argument,       0, 6969},
        {"config",             required_argument, 0, 'c'},
        {"socket",             required_argument, 0, 's'},
        {"wayland-fd",         required_argument, 0, 'F'},
        {0,0,0,0}
    };

    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1)
    {
        switch (opt)
        {
            case 0:
                break;
            case '?':
                help();
                return EXIT_FAILURE;

            case 'v':
                version();
                return EXIT_SUCCESS;

            case 'h':
                help();
                return EXIT_SUCCESS;

            case 'c':
            {
                configPath = optarg;

                try {
                    configPath = std::filesystem::canonical(configPath);

                    if (!std::filesystem::is_regular_file(configPath)) {
                        throw std::exception();
                    }
                } catch (...) {
                    std::cerr << "[ ERROR ] Config file '" << configPath << "' doesn't exist!\n";
                    help();

                    return EXIT_FAILURE;
                }

                Debug::log(LOG, "User-specified config location: '{}'", configPath);
            }
                break;

            case 'i':
                std::cout << systemInfoRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, "") << std::endl;
                return EXIT_SUCCESS;

            case 'F':
                try {
                    socketFd = std::stoi(optarg);

                    // check if socketFd is a valid file descriptor
                    if (fcntl(socketFd, F_GETFD) == -1)
                        throw std::exception();
                } catch (...) {
                    std::cerr << "[ ERROR ] Invalid Wayland FD!\n";
                    help();

                    return EXIT_FAILURE;
                }
                break;

            case 6969:
                if (!ignoreSudo) {
                    std::cout << "[ WARNING ] Running Hyprland with superuser privileges might damage your system\n";
                    ignoreSudo = true;
                }
                break;

            default:
                return EXIT_FAILURE;
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
