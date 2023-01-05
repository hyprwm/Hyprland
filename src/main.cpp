#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include <iostream>
#ifdef USES_SYSTEMD
#include <systemd/sd-daemon.h> // for sd_notify
#endif

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throw std::runtime_error("XDG_RUNTIME_DIR is not set!");

    // export HYPRLAND_CMD
    std::string cmd = "";
    for (auto i = 0; i < argc; ++i)
        cmd += std::string(i == 0 ? "" : " ") + argv[i];
    setenv("HYPRLAND_CMD", cmd.c_str(), 1);
    setenv("XDG_BACKEND", "wayland", 1);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 0);

    // parse some args
    std::string configPath;
    bool        ignoreSudo = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--i-am-really-stupid"))
            ignoreSudo = true;
        else if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config")) && argc >= i + 2) {
            configPath = std::string(argv[++i]);
            Debug::log(LOG, "Using config location %s.", configPath.c_str());
        } else {
            std::cout << "Hyprland usage: Hyprland [arg [...]].\n\nArguments:\n"
                      << "--help         -h | Show this help message\n"
                      << "--config       -c | Specify config file to use\n";
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

    const auto LOGWLR = getenv("HYPRLAND_LOG_WLR");
    if (LOGWLR && std::string(LOGWLR) == "1")
        wlr_log_init(WLR_DEBUG, Debug::wlrLog);

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    g_pCompositor                     = std::make_unique<CCompositor>();
    g_pCompositor->explicitConfigPath = configPath;

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

    wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);

    // kill all clients
    for (auto& c : g_pCompositor->m_dProcessPIDsOnShutdown)
        kill(c, SIGKILL);

    wl_display_destroy(g_pCompositor->m_sWLDisplay);

    return EXIT_SUCCESS;
}
