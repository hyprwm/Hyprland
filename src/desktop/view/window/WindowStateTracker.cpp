#include "WindowStateTracker.hpp"
#include "Window.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"

using namespace Desktop::View;

CWindowStateEventTracker::CWindowStateEventTracker(CWindow& window) : m_window(window) {
    ;
}

void CWindowStateEventTracker::start() {
    stop();
    m_lastState = snapshot();
}

void CWindowStateEventTracker::stop() {
    m_lastState.reset();
    m_pending = false;
    ++m_sequence;
}

void CWindowStateEventTracker::scheduleUpdate() {
    if (!m_lastState || m_pending)
        return;

    m_pending = true;
    // Layout and group mutations must finish before listeners can act on the window.
    g_pEventLoopManager->doLater([weak = m_window.m_self, sequence = ++m_sequence] {
        const auto WINDOW = weak.lock();
        if (!WINDOW)
            return;

        WINDOW->stateTracker().emitUpdate(sequence);
    });
}

void CWindowStateEventTracker::emitUpdate(uint64_t sequence) {
    if (!m_lastState || !m_pending || sequence != m_sequence)
        return;

    const auto         STATE = snapshot();
    SWindowStateUpdate update;
    if (STATE.floating != m_lastState->floating)
        update.floating = STATE.floating;
    if (STATE.pinned != m_lastState->pinned)
        update.pinned = STATE.pinned;

    m_lastState = STATE;
    m_pending   = false;

    if (!update.floating.has_value() && !update.pinned.has_value())
        return;

    Event::bus()->m_events.window.updateState.emit(m_window.m_self.lock(), update);
}

CWindowStateEventTracker::SWindowStateSnapshot CWindowStateEventTracker::snapshot() const {
    return {
        .floating = m_window.isFloating(),
        .pinned   = sc<bool>(m_window.m_state & WINDOW_STATE_PINNED),
    };
}
