#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"

#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <filesystem>

#ifdef USES_SYSTEMD
    #include <systemd/sd-daemon.h> // for sd_notify
#endif


void help() {
    std::cout << "usage: Hyprland [arg [...]]." << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  --help              -h       - Show this message again" << std::endl;
    std::cout << "  --config FILE       -c FILE  - Specify config file to use" << std::endl;
    std::cout << "  --i-am-really-stupid         - Omits root user privileges check (why'd u do that?)" << std::endl;
}


int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throw std::runtime_error("XDG_RUNTIME_DIR is not set!");

    // export HYPRLAND_CMD
    std::string cmd = "";
    for (auto i = 0; i < argc; ++i)
        cmd += std::string(i == 0 ? "" : " ") + argv[i];

    setenv("HYPRLAND_CMD", cmd.c_str(), 1);
    setenv("XDG_BACKEND", "wayland", 1);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);
    setenv("XDG_CURRENT_DESKTOP", "Hyprland", 1);


    // parse some args
    std::string configPath;
    bool ignoreSudo = false;

    // Opinion: All of this code is a mess. Try to do something with that, folks? Pretty please?
    // Some code might be not so good. I'm working at that at 3 am.

    // You can remove that.
    std::cout << "Argument parsing and code reformatting patch (nitrogenez@github.com)" << std::endl;

    std::vector<std::string> args { argv + 1, argv + argc };

    for (auto it = args.begin(); it != args.end(); it++) {
        if (it->compare("--i-am-really-stupid") == 0 && !ignoreSudo) {
            std::cout << "[ WARNING ] Running Hyprland with superuser privileges might damage your system" << std::endl;

            ignoreSudo = true;
        }

        else if (it->compare("-c") == 0 || it->compare("--config") == 0) {
            if (std::next(it)->c_str() == nullptr) {
                help();
                return 1;
            }
            std::string next_arg = std::next(it)->c_str();

            if (!std::filesystem::exists(next_arg)) {
                std::cerr << "[ ERROR ] Config path '" << next_arg << "' doesn't exist!" << std::endl;
                help();

                return 1;
            }

            configPath = next_arg;
            Debug::log(LOG, "User-specified config location: '%s'", configPath.c_str());
            continue;
        }
    }

    // Funny thing.
    // We don't *really* need that check, because in most cases `root` user
    // doesn't have XDG_RUNTIME_DIR envar set, which causes runtime error.

    // But if set, then yes, that needs to be checked.

    // We don't need to display that message when no superuser privs are found
    if (!ignoreSudo && Init::isSudo()) {
        std::cerr << "[ ERROR ] Hyprland was launched with superuser priveleges, but the privileges check is not omitted." << std::endl;
        return 1;
    }
    else if (ignoreSudo && Init::isSudo()) {
        std::cout << "Superuser privileges check is omitted. I hope you know what you're doing" << std::endl;
    }

    std::cout << "Welcome to Hyprland!\n";

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    g_pCompositor                     = std::make_unique<CCompositor>();
    g_pCompositor->explicitConfigPath = configPath;

    g_pCompositor->initServer();

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    // If we are here it means we got yote.
    Debug::log(LOG, "Hyprland reached the end.");

#ifdef USES_SYSTEMD
    // tell systemd it destroy bound/related units
    if (sd_booted() > 0)
        sd_notify(0, "STOPPING=1");
#endif

    if (g_pCompositor->m_sWLDisplay)
        wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);

    if (g_pCompositor->m_sWLDisplay)
        wl_display_destroy(g_pCompositor->m_sWLDisplay);

    return EXIT_SUCCESS;
}
