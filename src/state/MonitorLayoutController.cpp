#include "MonitorLayoutController.hpp"

#include "MonitorPositionController.hpp"
#include "MonitorState.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../debug/log/Logger.hpp"
#include "../event/EventBus.hpp"
#include "../i18n/Engine.hpp"
#include "../macros.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../notification/NotificationOverlay.hpp"
#include "../output/Monitor.hpp"
#include "../protocols/XDGOutput.hpp"
#include "../xwayland/XWayland.hpp"

#include <vector>

using namespace State;

UP<CMonitorLayoutController>& State::monitorLayoutController() {
    static UP<CMonitorLayoutController> p = makeUnique<CMonitorLayoutController>();
    return p;
}

void CMonitorLayoutController::scheduleRecheck() {
    if (m_scheduled)
        return;

    m_scheduled = true;
    g_pEventLoopManager->doLater([this] {
        arrange();
        checkOverlapsAndNotify();

        m_scheduled = false;
    });
}

void CMonitorLayoutController::checkOverlapsAndNotify() const {
    CRegion monitorRegion;

    for (const auto& m : State::monitorState()->monitors()) {
        if (!monitorRegion.copy().intersect(m->logicalBox()).empty()) {
            Log::logger->log(Log::ERR, "Monitor {}: detected overlap with layout", m->m_name);
            Notification::overlay()->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT, {{"name", m->m_name}}), CHyprColor{}, 15000,
                                                     ICON_WARNING);

            break;
        }

        monitorRegion.add(m->logicalBox());
    }
}

void CMonitorLayoutController::arrange() const {
    static auto                                   PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    std::vector<SP<Monitor::IMonitorArrangeable>> arrangeableMonitors;
    arrangeableMonitors.reserve(State::monitorState()->monitors().size());
    for (const auto& m : State::monitorState()->monitors()) {
        auto arrangeable = dynamicPointerCast<Monitor::IMonitorArrangeable>(m);
        RASSERT(arrangeable, "CMonitor does not implement IMonitorArrangeable");
        arrangeableMonitors.push_back(arrangeable);
    }

    Log::logger->log(Log::DEBUG, "arrangeMonitors: {} to arrange", arrangeableMonitors.size());
    State::monitorPositionController()->arrange(arrangeableMonitors, *PXWLFORCESCALEZERO);

    PROTO::xdgOutput->updateAllOutputs();
    Event::bus()->m_events.monitor.layoutChanged.emit();

#ifndef NO_XWAYLAND
    const auto box = g_pCompositor->calculateX11WorkArea();
    if (g_pXWayland && g_pXWayland->m_wm) {
        if (box)
            g_pXWayland->m_wm->updateWorkArea(box->x, box->y, box->w, box->h);
        else
            g_pXWayland->m_wm->updateWorkArea(0, 0, 0, 0);
    }

#endif

    // Recheck floating windows after layout changes to prevent them going offscreen
    g_pEventLoopManager->doLater([]() { g_pCompositor->recheckFloatingWindowsOnScreen(); });
}
