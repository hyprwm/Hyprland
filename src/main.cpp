#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include <iostream>

// I am a bad bad boy and have used some global vars here,
// just for this file
bool ignoreSudo = false;

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throw std::runtime_error("XDG_RUNTIME_DIR is not set!");

    // parse some args
    std::string configPath;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--i-am-really-stupid"))
            ignoreSudo = true;
        else if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config")) && argc >= i + 2) {
            configPath = std::string(argv[++i]);
            Debug::log(LOG, "Using config location %s.", configPath.c_str());
        } else {
            std::cout << "Hyprland usage: Hyprland [arg [...]].\n\nArguments:\n" <<
                "--help         -h | Show this help message\n" <<
                "--config       -c | Specify config file to use\n";
            return 1;
        }
    }

    system("mkdir -p /tmp/hypr");

    if (!ignoreSudo) {
        if (Init::isSudo()) {
            std::cout << "Hyprland shall not be run as the root user. If you really want to, use the --i-am-really-stupid flag.\n";
            return 1;
    	}
    } else {
        std::cout << "Running with ignored root checks, I surely hope you know what you're doing.\n";
        sleep(1);
    }

    std::cout << "Welcome to Hyprland!\n";

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    g_pCompositor = std::make_unique<CCompositor>(); 
    g_pCompositor->explicitConfigPath = configPath;

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    // If we are here it means we got yote.
    Debug::log(LOG, "Hyprland reached the end.");

    g_pCompositor->cleanup();

    return EXIT_SUCCESS;
}
