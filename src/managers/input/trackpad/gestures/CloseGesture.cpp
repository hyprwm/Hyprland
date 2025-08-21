#include "CloseGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../managers/animation/DesktopAnimationManager.hpp"
#include "../../../../render/Renderer.hpp"

constexpr const float MAX_DISTANCE = 200.F;

//
static Vector2D lerpVal(const Vector2D& from, const Vector2D& to, const float& t) {
    return Vector2D{
        from.x + ((to.x - from.x) * t),
        from.y + ((to.y - from.y) * t),
    };
}

static float lerpVal(const float& from, const float& to, const float& t) {
    return from + ((to - from) * t);
}

void CCloseTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    m_window = g_pCompositor->m_lastWindow;

    m_alphaFrom = m_window->m_alpha->goal();
    m_posFrom   = m_window->m_realPosition->goal();
    m_sizeFrom  = m_window->m_realSize->goal();

    g_pDesktopAnimationManager->startAnimation(m_window.lock(), CDesktopAnimationManager::ANIMATION_TYPE_OUT, true);
    *m_window->m_alpha = 0.f;

    m_alphaTo = m_window->m_alpha->goal();
    m_posTo   = m_window->m_realPosition->goal();
    m_sizeTo  = m_window->m_realSize->goal();

    m_window->m_alpha->setValueAndWarp(m_alphaFrom);
    m_window->m_realPosition->setValueAndWarp(m_posFrom);
    m_window->m_realSize->setValueAndWarp(m_sizeFrom);

    m_lastDelta = 0.F;
}

void CCloseTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;

    g_pHyprRenderer->damageWindow(m_window.lock());

    m_lastDelta += distance(e);

    const auto FADEPERCENT = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    m_window->m_alpha->setValueAndWarp(lerpVal(m_alphaFrom, m_alphaTo, FADEPERCENT));
    m_window->m_realPosition->setValueAndWarp(lerpVal(m_posFrom, m_posTo, FADEPERCENT));
    m_window->m_realSize->setValueAndWarp(lerpVal(m_sizeFrom, m_sizeTo, FADEPERCENT));

    g_pDecorationPositioner->onWindowUpdate(m_window.lock());

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CCloseTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!m_window)
        return;

    const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);

    if (COMPLETION < 0.2F) {
        // revert the animation
        g_pHyprRenderer->damageWindow(m_window.lock());
        *m_window->m_alpha        = m_alphaFrom;
        *m_window->m_realPosition = m_posFrom;
        *m_window->m_realSize     = m_sizeFrom;
        return;
    }

    // commence. Close the window and restore our current state to avoid a harsh anim
    const auto CURRENT_ALPHA = m_window->m_alpha->value();
    const auto CURRENT_POS   = m_window->m_realPosition->value();
    const auto CURRENT_SIZE  = m_window->m_realSize->value();

    // FIXME: what if we close a window but it gives a popup and refuses to close?
    /// maybe like a timer? xdg ping?

    g_pCompositor->closeWindow(m_window.lock());

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(m_window.lock());

    m_window->m_noOutAnim = true;

    const auto GOAL_ALPHA = m_window->m_alpha->goal();
    const auto GOAL_POS   = m_window->m_realPosition->goal();
    const auto GOAL_SIZE  = m_window->m_realSize->goal();

    m_window->m_alpha->setValueAndWarp(CURRENT_ALPHA);
    m_window->m_realPosition->setValueAndWarp(CURRENT_POS);
    m_window->m_realSize->setValueAndWarp(CURRENT_SIZE);

    *m_window->m_alpha        = GOAL_ALPHA;
    *m_window->m_realPosition = GOAL_POS;
    *m_window->m_realSize     = GOAL_SIZE;

    m_window.reset();
}
