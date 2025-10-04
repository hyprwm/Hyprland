#include "ScreenshareManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/Window.hpp"

CScreenshareManager::CScreenshareManager() {
    ;
}

void CScreenshareManager::onOutputCommit(PHLMONITOR monitor) {
    if (m_frames.empty()) {
        g_pHyprRenderer->m_directScanoutBlocked = false;
        return; // nothing to share
    }

    std::ranges::remove_if(m_sessions, [&](const WP<CScreenshareSession>& session) { return session.expired(); });

    std::ranges::for_each(m_frames, [&](UP<CScreenshareFrame>& frame) {
        if (!frame->done())
            frame->share();
    });

    std::ranges::remove_if(m_frames, [&](const UP<CScreenshareFrame>& frame) { return frame->done(); });
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor) {
    if UNLIKELY (!monitor || !g_pCompositor->monitorExists(monitor)) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(monitor, client, overlayCursor));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor) {
    if UNLIKELY (!monitor || !g_pCompositor->monitorExists(monitor)) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(monitor, captureRegion, client, overlayCursor));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLWINDOW window, wl_client* client, bool overlayCursor) {
    if UNLIKELY (!window || !window->m_isMapped) {
        LOGM(ERR, "Client requested sharing of window that is gone or not shareable!");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(window, client, overlayCursor));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}
