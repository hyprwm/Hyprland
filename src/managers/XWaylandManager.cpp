#include "XWaylandManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/view/WLSurface.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../config/ConfigValue.hpp"
#include "../debug/log/Logger.hpp"
#include "../output/Monitor.hpp"
#include "../state/MonitorState.hpp"
#include <hyprutils/math/Vector2D.hpp>

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

CHyprXWaylandManager::CHyprXWaylandManager() = default;

CHyprXWaylandManager::~CHyprXWaylandManager() {
#ifndef NO_XWAYLAND
    unsetenv("DISPLAY");
#endif
}

void CHyprXWaylandManager::activateSurface(SP<CWLSurfaceResource> pSurface, bool activate) {
    if (!pSurface)
        return;

    auto HLSurface = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!HLSurface) {
        Log::logger->log(Log::TRACE, "CHyprXWaylandManager::activateSurface on non-desktop surface, ignoring");
        return;
    }

    const auto PWINDOW = Desktop::View::CWindow::fromView(HLSurface->view());
    if (!PWINDOW) {
        Log::logger->log(Log::TRACE, "CHyprXWaylandManager::activateSurface on non-window surface, ignoring");
        return;
    }

    if (activate) {
        PWINDOW->backend().setMinimized(false);
        PWINDOW->backend().restackToTop();
    }

    PWINDOW->backend().setActive(activate);
}

void CHyprXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (activate) {
        if (pWindow->backend().isX11())
            pWindow->sendWindowSize(true); // update xwayland output pos

        pWindow->backend().setMinimized(false);

        if (!pWindow->backend().traits().overrideRedirect)
            pWindow->backend().restackToTop();
    }

    pWindow->backend().setActive(activate);

    if (activate) {
        Desktop::focusState()->surface() = pWindow->wlSurface()->resource();
        Desktop::focusState()->window()  = pWindow;
    }

    if (!(pWindow->m_state & Desktop::View::WINDOW_STATE_PINNED))
        pWindow->m_workspace->m_lastFocusedWindow = pWindow;
}

Vector2D CHyprXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord) {
    return waylandToXWaylandCoords(coord, nullptr);
}

Vector2D CHyprXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord, PHLMONITOR preferredMonitor) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor = preferredMonitor;
    if (!pMonitor) {
        double bestDistance = __FLT_MAX__;
        for (const auto& m : State::monitorState()->monitors()) {
            const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

            double     distance = vecToRectDistanceSquared(coord, {m->m_position.x, m->m_position.y}, {m->m_position.x + SIZ.x - 1, m->m_position.y + SIZ.y - 1});

            if (distance < bestDistance) {
                bestDistance = distance;
                pMonitor     = m;
            }
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_position;
    // if scaled, scale
    if (*PXWLFORCESCALEZERO)
        result *= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_xwaylandPosition;

    return result;
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {
    return xwaylandToWaylandCoords(coord, nullptr);
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord, PHLMONITOR preferredMonitor) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor = preferredMonitor;
    if (!pMonitor) {
        double bestDistance = __FLT_MAX__;
        for (const auto& m : State::monitorState()->monitors()) {
            const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

            double     distance =
                vecToRectDistanceSquared(coord, {m->m_xwaylandPosition.x, m->m_xwaylandPosition.y}, {m->m_xwaylandPosition.x + SIZ.x - 1, m->m_xwaylandPosition.y + SIZ.y - 1});

            if (distance < bestDistance) {
                bestDistance = distance;
                pMonitor     = m;
            }
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_xwaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result /= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_position;

    return result;
}
