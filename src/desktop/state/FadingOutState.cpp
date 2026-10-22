#include "FadingOutState.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"

#include <algorithm>

using namespace Desktop;

CFadingOutState::CFadingOutState() {
    // Collect after animation updates and their final damage callbacks have unwound,
    // even when DPMS or direct scanout prevents a composited frame.
    m_tickListener = Event::bus()->m_events.tick.listen([this] { cleanup(); });
}

const std::vector<SP<IFadeout>>& CFadingOutState::fadeouts() const {
    return m_fadeouts;
}

void CFadingOutState::add(SP<IFadeout> fadeout) {
    if (!fadeout || fadeout->monitor().expired() || fadeout->done())
        return;

    m_fadeouts.emplace_back(fadeout);
}

void CFadingOutState::cleanupForMonitor(PHLMONITOR monitor) {
    if (!monitor)
        return;

    cleanup(monitor);
}

void CFadingOutState::cleanup(PHLMONITOR monitor) {
    const auto SIZEBEFORE = m_fadeouts.size();

    std::erase_if(m_fadeouts, [&](const auto& fadeout) { return !fadeout || fadeout->monitor().expired() || ((!monitor || fadeout->monitor() == monitor) && fadeout->done()); });

    if (SIZEBEFORE != m_fadeouts.size())
        LOG(Log::DEBUG, "Cleanup: removed {} fadeouts", SIZEBEFORE - m_fadeouts.size());
}

void CFadingOutState::clear() {
    m_fadeouts.clear();
}

UP<CFadingOutState>& Desktop::fadingOutState() {
    static UP<CFadingOutState> state = makeUnique<CFadingOutState>();
    return state;
}
