#include "FullscreenGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"

constexpr const float MAX_DISTANCE = 250.F;

//
static Vector2D lerpVal(const Vector2D& from, const Vector2D& to, const float& t) {
    return Vector2D{
        from.x + ((to.x - from.x) * t),
        from.y + ((to.y - from.y) * t),
    };
}

CFullscreenTrackpadGesture::CFullscreenTrackpadGesture(const std::string_view& mode) {
    std::string lc = std::string{mode};
    std::ranges::transform(lc, lc.begin(), ::tolower);

    if (lc.starts_with("fullscreen"))
        m_mode = MODE_FULLSCREEN;
    else if (lc.starts_with("maximize"))
        m_mode == MODE_MAXIMIZE;
    else
        m_mode = MODE_FULLSCREEN;
}

void CFullscreenTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    m_window = g_pCompositor->m_lastWindow;

    m_posFrom  = m_window->m_realPosition->goal();
    m_sizeFrom = m_window->m_realSize->goal();

    m_originalMode = m_window->m_fullscreenState.internal;

    g_pCompositor->setWindowFullscreenInternal(m_window.lock(), m_window->m_fullscreenState.internal == FSMODE_NONE ? fsModeForMode(m_mode) : FSMODE_NONE);

    m_posTo  = m_window->m_realPosition->goal();
    m_sizeTo = m_window->m_realSize->goal();

    m_lastDelta = 0.F;
}

void CFullscreenTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
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

void CFullscreenTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_window)
        return;

    const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    if (COMPLETION < 0.2F) {
        // revert the animation
        g_pHyprRenderer->damageWindow(m_window.lock());
        m_window->m_isFloating = !m_window->m_isFloating;
        g_pCompositor->setWindowFullscreenInternal(m_window.lock(), m_window->m_fullscreenState.internal == FSMODE_NONE ? m_originalMode : FSMODE_NONE);
        return;
    }

    *m_window->m_realPosition = m_posTo;
    *m_window->m_realSize     = m_sizeTo;
}

eFullscreenMode CFullscreenTrackpadGesture::fsModeForMode(eMode mode) {
    switch (mode) {
        case MODE_FULLSCREEN: return FSMODE_FULLSCREEN;
        case MODE_MAXIMIZE: return FSMODE_MAXIMIZED;
        default: break;
    }
    return FSMODE_FULLSCREEN;
}
