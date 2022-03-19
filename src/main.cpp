#include "defines.hpp"
#include "debug/Log.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"

int main(int argc, char** argv) {
    const std::string DEBUGPATH = "/tmp/hypr/hyprland.log";
    const std::string DEBUGPATH2 = "/tmp/hypr/hyprlandd.log";
    unlink(DEBUGPATH2.c_str());
    unlink(DEBUGPATH.c_str());

    if (!getenv("XDG_RUNTIME_DIR"))
        RIP("XDG_RUNTIME_DIR not set!");

    Debug::log(LOG, "Welcome to Hyprland!");

    // let's init the compositor.
    // it initializes basic Wayland stuff in the constructor.
    g_pCompositor = std::make_unique<CCompositor>(); 

    Debug::log(LOG, "Hyprland init finished.");

    // If all's good to go, start.
    g_pCompositor->startCompositor();

    // If we are here it means we got yote.
    Debug::log(LOG, "Hyprland reached the end.");

    wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);
    wl_display_destroy(g_pCompositor->m_sWLDisplay);

    return EXIT_SUCCESS;
}
