#include "MonitorLayoutController.hpp"

#include "MonitorPositionController.hpp"
#include "MonitorState.hpp"

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
    const auto box = calculateUnifiedX11WorkArea();
    if (g_pXWayland && g_pXWayland->m_wm) {
        if (box)
            g_pXWayland->m_wm->updateWorkArea(box->x, box->y, box->w, box->h);
        else
            g_pXWayland->m_wm->updateWorkArea(0, 0, 0, 0);
    }

#endif
}

bool CMonitorLayoutController::isPointOnAnyMonitor(const Vector2D& point) {
    return std::ranges::any_of(State::monitorState()->monitors(), [&](const PHLMONITOR& m) {
        return VECINRECT(point, m->m_position.x, m->m_position.y, m->m_size.x + m->m_position.x, m->m_size.y + m->m_position.y);
    });
}

bool CMonitorLayoutController::isPointOnReservedArea(const Vector2D& point, const PHLMONITOR pMonitor) {
    const auto PMONITOR = pMonitor ? pMonitor : State::monitorState()->query().vec(point).run();

    auto       box = PMONITOR->logicalBox();
    if (VECNOTINRECT(point, box.x - 1, box.y - 1, box.x + box.w + 1, box.y + box.h + 1))
        return false;

    PMONITOR->m_reservedArea.applyip(box);

    return VECNOTINRECT(point, box.x, box.y, box.x + box.w, box.y + box.h);
}

std::optional<CBox> CMonitorLayoutController::calculateUnifiedX11WorkArea() const {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");
    // We more than likely won't be able to calculate one
    // and even if we could this is minor
    if (State::monitorState()->monitors().size() > 1 || State::monitorState()->monitors().empty())
        return std::nullopt;

    const auto M = State::monitorState()->monitors().front();

    // we ignore monitor->m_position on purpose
    CBox box = M->logicalBoxMinusReserved().translate(-M->m_position);
    if ((*PXWLFORCESCALEZERO))
        box.scale(M->m_scale);

    return box.translate(M->m_xwaylandPosition);
}

bool CMonitorLayoutController::isVRRActiveOnAnyMonitor() const {
    return std::ranges::any_of(State::monitorState()->monitors(), [](const PHLMONITOR& m) { return m->m_vrrActive; });
}
