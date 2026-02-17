#include "ScreenshareManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/view/Window.hpp"
#include "../HookSystemManager.hpp"
#include "../EventManager.hpp"
#include "../eventLoop/EventLoopManager.hpp"
#include "../../protocols/core/Seat.hpp"

CScreenshareManager::CScreenshareManager() {
    m_tickTimer = makeShared<CEventLoopTimer>(std::chrono::microseconds(500), [this](SP<CEventLoopTimer> self, void* data) { onTick(); }, nullptr);
    if (g_pEventLoopManager) // null in --verify-config mode
        g_pEventLoopManager->addTimer(m_tickTimer);
}

void CScreenshareManager::onOutputCommit(PHLMONITOR monitor) {
    std::erase_if(m_sessions, [&](const WP<CScreenshareSession>& session) { return session.expired(); });

    if (m_frames.empty()) {
        for (const auto& session : m_sessions) {
            if (session->m_framesInLastHalfSecond > 0)
                return;
        }

        g_pHyprRenderer->m_directScanoutBlocked = false;
        return; // nothing to share
    }

    std::ranges::for_each(m_frames, [&](WP<CScreenshareFrame>& frame) {
        if (frame.expired() || !frame->m_shared || frame->done())
            return;

        if (frame->m_session->monitor() != monitor)
            return;

        if (frame->m_session->m_type == SHARE_WINDOW) {
            CBox geometry = {frame->m_session->m_window->m_realPosition->value(), frame->m_session->m_window->m_realSize->value()};
            if (geometry.intersection({monitor->m_position, monitor->m_size}).empty())
                return;
        }

        frame->copy();
        frame->m_session->m_lastFrame.reset();
        frame->m_session->m_frameCounter++;
    });

    std::erase_if(m_frames, [&](const WP<CScreenshareFrame>& frame) { return frame.expired(); });
}

void CScreenshareManager::onTick() {
    m_tickTimer->updateTimeout(std::chrono::microseconds(500));

    std::ranges::for_each(m_managedSessions, [this](auto& session) {
        if (session->m_lastMeasure.getMillis() < 500)
            return;

        session->m_session->m_framesInLastHalfSecond = session->m_session->m_frameCounter;
        session->m_session->m_frameCounter           = 0;
        session->m_lastMeasure.reset();

        const auto LASTFRAMEDELTA = session->m_session->m_lastFrame.getMillis() / 1000.0;
        const bool FRAMEAWAITING = std::ranges::any_of(m_frames, [&](const auto& frame) { return !frame.expired() && frame->m_session->m_client == session->m_session->m_client; });

        if (session->m_session->m_framesInLastHalfSecond > 3 && !session->m_sentScreencast) {
            screenshareEvents(session->m_session, true);
            session->m_sentScreencast = true;
        } else if (session->m_session->m_framesInLastHalfSecond < 4 && session->m_sentScreencast && LASTFRAMEDELTA > 1.0 && !FRAMEAWAITING) {
            screenshareEvents(session->m_session, false);
            session->m_sentScreencast = false;
        }
    });
}

void CScreenshareManager::screenshareEvents(WP<CScreenshareSession> session, bool started) {
    if (session.expired()) {
        LOGM(Log::ERR, "screenshareEvents: FAILED");
        return;
    }

    if (started) {
        EMIT_HOOK_EVENT("screencastv2", (std::vector<std::any>{1, session->m_type, session->m_name}));
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencastv2", .data = std::format("1,{},{}", session->m_type, session->m_name)});
        LOGM(Log::INFO, "New screenshare session for ({}): {}", session->m_type, session->m_name);
    } else {
        EMIT_HOOK_EVENT("screencastv2", (std::vector<std::any>{0, session->m_type, session->m_name}));
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencastv2", .data = std::format("0,{},{}", session->m_type, session->m_name)});
        LOGM(Log::INFO, "Stopped screenshare session for ({}): {}", session->m_type, session->m_name);
    }
}

UP<CScreenshareSession> CScreenshareManager::newSession(wl_client* client, PHLMONITOR monitor) {
    if UNLIKELY (!monitor || !g_pCompositor->monitorExists(monitor)) {
        LOGM(Log::ERR, "Client requested sharing of a monitor that is gone");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(monitor, client, false));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(wl_client* client, PHLMONITOR monitor, CBox captureRegion) {
    if UNLIKELY (!monitor || !g_pCompositor->monitorExists(monitor)) {
        LOGM(Log::ERR, "Client requested sharing of a monitor that is gone");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(monitor, captureRegion, client, false));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CScreenshareSession> CScreenshareManager::newSession(wl_client* client, PHLWINDOW window) {
    if UNLIKELY (!window || !window->m_isMapped) {
        LOGM(Log::ERR, "Client requested sharing of window that is gone or not shareable!");
        return nullptr;
    }

    UP<CScreenshareSession> session = UP<CScreenshareSession>(new CScreenshareSession(window, client, false));

    session->m_self = session;
    m_sessions.emplace_back(session);

    return session;
}

UP<CCursorshareSession> CScreenshareManager::newCursorSession(wl_client* client, WP<CWLPointerResource> pointer) {
    UP<CCursorshareSession> session = UP<CCursorshareSession>(new CCursorshareSession(client, pointer));

    session->m_self = session;
    m_cursorSessions.emplace_back(session);

    return session;
}

WP<CScreenshareSession> CScreenshareManager::getManagedSession(wl_client* client, PHLMONITOR monitor) {
    return getManagedSession(SHARE_MONITOR, client, monitor, nullptr, {});
}

WP<CScreenshareSession> CScreenshareManager::getManagedSession(wl_client* client, PHLMONITOR monitor, CBox captureBox) {

    return getManagedSession(SHARE_REGION, client, monitor, nullptr, captureBox);
}

WP<CScreenshareSession> CScreenshareManager::getManagedSession(wl_client* client, PHLWINDOW window) {
    return getManagedSession(SHARE_WINDOW, client, nullptr, window, {});
}

WP<CScreenshareSession> CScreenshareManager::getManagedSession(eScreenshareType type, wl_client* client, PHLMONITOR monitor, PHLWINDOW window, CBox captureBox) {
    if (type == SHARE_NONE)
        return {};

    auto it = std::ranges::find_if(m_managedSessions, [&](const auto& session) {
        if (session->m_session->m_client != client || session->m_session->m_type != type)
            return false;

        switch (type) {
            case SHARE_MONITOR: return session->m_session->m_monitor == monitor;
            case SHARE_WINDOW: return session->m_session->m_window == window;
            case SHARE_REGION: return session->m_session->m_monitor == monitor && session->m_session->m_captureBox == captureBox;
            case SHARE_NONE:
            default: return false;
        }

        return false;
    });

    if (it == m_managedSessions.end()) {
        UP<CScreenshareSession> session;
        switch (type) {
            case SHARE_MONITOR: session = UP<CScreenshareSession>(new CScreenshareSession(monitor, client, true)); break;
            case SHARE_WINDOW: session = UP<CScreenshareSession>(new CScreenshareSession(window, client, true)); break;
            case SHARE_REGION: session = UP<CScreenshareSession>(new CScreenshareSession(monitor, captureBox, client, true)); break;
            case SHARE_NONE:
            default: return {};
        }

        session->m_self = session;
        m_sessions.emplace_back(session);

        it = m_managedSessions.emplace(m_managedSessions.end(), makeUnique<SManagedSession>(std::move(session)));
    }

    auto& session = *it;

    session->stoppedListener = session->m_session->m_events.stopped.listen([session = WP<SManagedSession>(session)]() {
        std::erase_if(g_pScreenshareManager->m_managedSessions, [&](const auto& s) { return !s || session.expired() || s->m_session == session->m_session; });
    });

    return session->m_session;
}

void CScreenshareManager::destroyClientSessions(wl_client* client) {
    LOGM(Log::TRACE, "Destroy client sessions for {:x}", (uintptr_t)client);
    std::erase_if(m_managedSessions, [&](const auto& session) { return !session || session->m_session->m_client == client; });
}

CScreenshareManager::SManagedSession::SManagedSession(UP<CScreenshareSession>&& session) : m_session(std::move(session)) {
    m_lastMeasure.reset();
}
