#include "FloatGesture.hpp"

#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../desktop/Window.hpp"

constexpr const float MAX_DISTANCE = 250.F;

//
static Vector2D lerpVal(const Vector2D& from, const Vector2D& to, const float& t) {
    return Vector2D{
        from.x + ((to.x - from.x) * t),
        from.y + ((to.y - from.y) * t),
    };
}

CFloatTrackpadGesture::CFloatTrackpadGesture(const std::string_view& data) {
    std::string lc = std::string{data};
    std::ranges::transform(lc, lc.begin(), ::tolower);

    if (lc.starts_with("float"))
        m_mode = FLOAT_MODE_FLOAT;
    else if (lc.starts_with("tile"))
        m_mode = FLOAT_MODE_TILE;
    else
        m_mode = FLOAT_MODE_TOGGLE;
}

void CFloatTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_window = Desktop::focusState()->window();

    if (!m_window)
        return;

    if ((m_window->m_isFloating && m_mode == FLOAT_MODE_FLOAT) || (!m_window->m_isFloating && m_mode == FLOAT_MODE_TILE)) {
        m_window.reset();
        return;
    }

    m_window->m_isFloating = !m_window->m_isFloating;
    g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(m_window.lock());

    m_posFrom  = m_window->m_realPosition->begun();
    m_sizeFrom = m_window->m_realSize->begun();

    m_posTo  = m_window->m_realPosition->goal();
    m_sizeTo = m_window->m_realSize->goal();

    m_lastDelta = 0.F;
}

void CFloatTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;

    g_pHyprRenderer->damageWindow(m_window.lock());

    m_lastDelta += distance(e);

    const auto FADEPERCENT = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    m_window->m_realPosition->setValueAndWarp(lerpVal(m_posFrom, m_posTo, FADEPERCENT));
    m_window->m_realSize->setValueAndWarp(lerpVal(m_sizeFrom, m_sizeTo, FADEPERCENT));

    g_pDecorationPositioner->onWindowUpdate(m_window.lock());

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CFloatTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_window)
        return;

    const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    if (COMPLETION < 0.2F) {
        // revert the animation
        g_pHyprRenderer->damageWindow(m_window.lock());
        m_window->m_isFloating = !m_window->m_isFloating;
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(m_window.lock());
        return;
    }

    *m_window->m_realPosition = m_posTo;
    *m_window->m_realSize     = m_sizeTo;
}
