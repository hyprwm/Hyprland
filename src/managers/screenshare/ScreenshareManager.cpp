#include "ScreenshareManager.hpp"
#include "../../render/Renderer.hpp"

// TODO: do transform and scale for both constructor's m_box

CScreenshareManager::CScreenshareManager() {
    ;
}

void CScreenshareManager::onOutputCommit(PHLMONITOR monitor) {
    if (m_frames.empty()) {
        g_pHyprRenderer->m_directScanoutBlocked = false;
        return; // nothing to share
    }

    std::ranges::remove_if(m_sessions, [&](const WP<CScreenshareSession>& session) { return session.expired(); });

    std::ranges::for_each(m_frames, [&](CScreenshareFrame& frame) {
        if (!frame.done())
            frame.share();
    });

    std::ranges::remove_if(m_frames, [&](const CScreenshareFrame& frame) { return frame.done(); });
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor) {
    UP<CScreenshareSession> session = makeUnique<CScreenshareSession>(monitor, client, overlayCursor);

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor) {
    UP<CScreenshareSession> session = makeUnique<CScreenshareSession>(monitor, captureRegion, client, overlayCursor);

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(PHLWINDOW window, wl_client* client, bool overlayCursor) {
    UP<CScreenshareSession> session = makeUnique<CScreenshareSession>(window, client, overlayCursor);

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}
