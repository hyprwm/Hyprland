#include "OverviewGesture.hpp"

#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../overview/Overview.hpp"

#include <algorithm>

constexpr const float MAX_DISTANCE = 200.F;
constexpr const float COMMIT_RATIO = 0.3F;

void                  COverviewTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_distance = 0.F;
    m_overview = Overview::overview().get();

    const auto GESTURE = dynamic_cast<Overview::IOverviewGesture*>(Overview::overview().get());
    m_interactive      = GESTURE;
    m_active           = !GESTURE || GESTURE->beginGesture(Desktop::focusState()->monitor());
    if (!m_active)
        m_overview = nullptr;
}

void COverviewTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_active)
        return;

    m_distance += distance(e);

    if (Overview::overview().get() != m_overview || !m_interactive)
        return;

    if (const auto GESTURE = dynamic_cast<Overview::IOverviewGesture*>(Overview::overview().get()))
        GESTURE->updateGesture(std::clamp(m_distance / MAX_DISTANCE, 0.F, 1.F));
}

void COverviewTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_active)
        return;

    m_active             = false;
    const bool CANCELLED = (e.swipe && e.swipe->cancelled) || (e.pinch && e.pinch->cancelled);
    const bool COMMITTED = !CANCELLED && m_distance / MAX_DISTANCE >= COMMIT_RATIO;

    if (Overview::overview().get() != m_overview) {
        m_overview = nullptr;
        return;
    }

    m_overview = nullptr;
    if (const auto GESTURE = m_interactive ? dynamic_cast<Overview::IOverviewGesture*>(Overview::overview().get()) : nullptr) {
        GESTURE->endGesture(COMMITTED);
        return;
    }

    if (!COMMITTED)
        return;

    if (Overview::overview()->isOpen())
        Overview::overview()->close();
    else
        Overview::overview()->open(Desktop::focusState()->monitor());
}
