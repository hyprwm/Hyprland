#include "FadingOutState.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../output/Monitor.hpp"

#include <algorithm>

using namespace Desktop;

const std::vector<SP<IFadeout>>& CFadingOutState::fadeouts() const {
    return m_fadeouts;
}

void CFadingOutState::add(SP<IFadeout> fadeout) {
    if (!fadeout)
        return;

    m_fadeouts.emplace_back(fadeout);
}

void CFadingOutState::cleanupForMonitor(PHLMONITOR monitor) {
    if (!monitor)
        return;

    const auto SIZEBEFORE = m_fadeouts.size();

    std::erase_if(m_fadeouts, [&](const auto& fadeout) { return !fadeout || fadeout->monitor().expired() || (fadeout->monitor() == monitor && fadeout->done()); });

    if (SIZEBEFORE != m_fadeouts.size())
        Log::logger->log(Log::DEBUG, "Cleanup: removed {} fadeouts", SIZEBEFORE - m_fadeouts.size());
}

void CFadingOutState::clear() {
    m_fadeouts.clear();
}

UP<CFadingOutState>& Desktop::fadingOutState() {
    static UP<CFadingOutState> state = makeUnique<CFadingOutState>();
    return state;
}
