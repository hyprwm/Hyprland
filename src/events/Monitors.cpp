#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"
#include "../debug/HyprCtl.hpp"
#include "../config/ConfigValue.hpp"

// --------------------------------------------------------- //
//   __  __  ____  _   _ _____ _______ ____  _____   _____   //
//  |  \/  |/ __ \| \ | |_   _|__   __/ __ \|  __ \ / ____|  //
//  | \  / | |  | |  \| | | |    | | | |  | | |__) | (___    //
//  | |\/| | |  | | . ` | | |    | | | |  | |  _  / \___ \   //
//  | |  | | |__| | |\  |_| |_   | | | |__| | | \ \ ____) |  //
//  |_|  |_|\____/|_| \_|_____|  |_|  \____/|_|  \_\_____/   //
//                                                           //
// --------------------------------------------------------- //

static void checkDefaultCursorWarp(PHLMONITOR PNEWMONITOR, std::string monitorName) {

    static auto PCURSORMONITOR    = CConfigValue<std::string>("cursor:default_monitor");
    static auto firstMonitorAdded = std::chrono::system_clock::now();
    static bool cursorDefaultDone = false;
    static bool firstLaunch       = true;

    const auto  POS = PNEWMONITOR->middle();

    // by default, cursor should be set to first monitor detected
    // this is needed as a default if the monitor given in config above doesn't exist
    if (firstLaunch) {
        firstLaunch = false;
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
    }

    if (cursorDefaultDone || *PCURSORMONITOR == STRVAL_EMPTY)
        return;

    // after 10s, don't set cursor to default monitor
    auto timePassedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - firstMonitorAdded);
    if (timePassedSec.count() > 10) {
        cursorDefaultDone = true;
        return;
    }

    if (*PCURSORMONITOR == monitorName) {
        cursorDefaultDone = true;
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
    }
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accommodate for that.
    const auto OUTPUT = (wlr_output*)data;

    if (!OUTPUT->name) {
        Debug::log(ERR, "New monitor has no name?? Ignoring");
        return;
    }

    // add it to real
    auto PNEWMONITOR = g_pCompositor->m_vRealMonitors.emplace_back(makeShared<CMonitor>());
    if (std::string("HEADLESS-1") == OUTPUT->name)
        g_pCompositor->m_pUnsafeOutput = PNEWMONITOR;

    PNEWMONITOR->output           = OUTPUT;
    PNEWMONITOR->self             = PNEWMONITOR;
    const bool FALLBACK           = g_pCompositor->m_pUnsafeOutput ? OUTPUT == g_pCompositor->m_pUnsafeOutput->output : false;
    PNEWMONITOR->ID               = FALLBACK ? -1 : g_pCompositor->getNextAvailableMonitorID(OUTPUT->name);
    PNEWMONITOR->isUnsafeFallback = FALLBACK;

    EMIT_HOOK_EVENT("newMonitor", PNEWMONITOR);

    if (!FALLBACK)
        PNEWMONITOR->onConnect(false);

    if (!PNEWMONITOR->m_bEnabled || FALLBACK)
        return;

    // ready to process if we have a real monitor

    if ((!g_pHyprRenderer->m_pMostHzMonitor || PNEWMONITOR->refreshRate > g_pHyprRenderer->m_pMostHzMonitor->refreshRate) && PNEWMONITOR->m_bEnabled)
        g_pHyprRenderer->m_pMostHzMonitor = PNEWMONITOR;

    g_pCompositor->m_bReadyToProcess = true;

    g_pConfigManager->m_bWantsMonitorReload = true;
    g_pCompositor->scheduleFrameForMonitor(PNEWMONITOR);

    checkDefaultCursorWarp(PNEWMONITOR, OUTPUT->name);

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iMonitorID == PNEWMONITOR->ID) {
            w->m_iLastSurfaceMonitorID = -1;
            w->updateSurfaceScaleTransformDetails();
        }
    }
}
