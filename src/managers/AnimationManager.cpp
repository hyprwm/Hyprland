#include "AnimationManager.hpp"
#include "../Compositor.hpp"

void CAnimationManager::tick() {

    bool animationsDisabled = false;

    if (!g_pConfigManager->getInt("animations:enabled"))
        animationsDisabled = true;

    const bool WINDOWSENABLED   = g_pConfigManager->getInt("animations:windows");
    const bool BORDERSENABLED   = g_pConfigManager->getInt("animations:borders");
    const bool FADEENABLED      = g_pConfigManager->getInt("animations:fadein");
    const float ANIMSPEED       = g_pConfigManager->getFloat("animations:speed");

    for (auto& w : g_pCompositor->m_lWindows) {
        if (animationsDisabled) {
            w.m_vRealPosition = w.m_vEffectivePosition;
            w.m_vRealSize = w.m_vEffectiveSize;
            continue;
        }

        // process the window
        if (WINDOWSENABLED) {
            if (deltazero(w.m_vRealPosition, w.m_vEffectivePosition) && deltazero(w.m_vRealSize, w.m_vEffectiveSize)) {
                continue;
            }

            if (deltaSmallToFlip(w.m_vRealPosition, w.m_vEffectivePosition) && deltaSmallToFlip(w.m_vRealSize, w.m_vEffectiveSize)) {
                w.m_vRealPosition = w.m_vEffectivePosition;
                w.m_vRealSize = w.m_vEffectiveSize;
                g_pXWaylandManager->setWindowSize(&w, w.m_vRealSize);
                continue;
            }

            // if it is to be animated, animate it.
            w.m_vRealPosition = Vector2D(parabolic(w.m_vRealPosition.x, w.m_vEffectivePosition.x, ANIMSPEED), parabolic(w.m_vRealPosition.y, w.m_vEffectivePosition.y, ANIMSPEED));
            w.m_vRealSize = Vector2D(parabolic(w.m_vRealSize.x, w.m_vEffectiveSize.x, ANIMSPEED), parabolic(w.m_vRealSize.y, w.m_vEffectiveSize.y, ANIMSPEED));
        }
    }
}

bool CAnimationManager::deltaSmallToFlip(const Vector2D& a, const Vector2D& b) {
    return std::abs(a.x - b.x) < 0.5f && std::abs(a.y - b.y) < 0.5f;
}

bool CAnimationManager::deltazero(const Vector2D& a, const Vector2D& b) {
    return a.x == b.x && a.y == b.y;
}

double CAnimationManager::parabolic(double from, double to, double incline) {
    return from + ((to - from) / incline);
}