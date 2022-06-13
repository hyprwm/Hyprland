#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include <iostream>

// I am a bad bad boy and have used some global vars here,
// just for this file
bool ignoreSudo = false;
bool ignoreSudoWarnMessage = false;

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        RIP("XDG_RUNTIME_DIR not set!");

    // parse some args
    std::string arg1 = argv[1];

    if (arg1 == "--i-am-really-stupid")
        ignoreSudo = true;
        if (!strcmp(argv[2], "no-warn-message"))
            ignoreSudoWarnMessage = true;
    else {
        std::cout << "Available commands:\n";
        std::cout << "  help                 - show this help text\n";
        std::cout << "  --i-am-really-stupid - ignore sudo run check\n";
        std::cout << "More commands can be found by running hyprctrl\n";
        std::cout << "When using --i-am-really-stupid you can ignore the warning message and startup delay with additional argument no-warn-message\n";
        return 0;
     }

    system("mkdir -p /tmp/hypr");

    if (!ignoreSudo) {
        if (Init::isSudo()) {
            std::cout << "Hyprland shall not be run as the root user. If you really want to, use the --i-am-really-stupid flag.\n";
            return 1;
    	}
    }
    else if (!ignoreSudoWarnMessage) {
        std::cout << "Running with ignored root checks, I surely hope you know what you're doing.\n";
        sleep(1);
    }

    std::cout << "Welcome to Hyprland!\n";

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    g_pCompositor = std::make_unique<CCompositor>(); 

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    // If we are here it means we got yote.
    Debug::log(LOG, "Hyprland reached the end.");

    g_pCompositor->cleanupExit();

    return EXIT_SUCCESS;
}
