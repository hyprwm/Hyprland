#include "AnimationManager.hpp"
#include "../Compositor.hpp"

void CAnimationManager::tick() {

    bool animationsDisabled = false;

    if (!g_pConfigManager->getInt("animations:enabled"))
        animationsDisabled = true;

    const bool WINDOWSENABLED   = g_pConfigManager->getInt("animations:windows") && !animationsDisabled;
    const bool BORDERSENABLED   = g_pConfigManager->getInt("animations:borders") && !animationsDisabled;
    const bool FADEENABLED      = g_pConfigManager->getInt("animations:fadein") && !animationsDisabled;
    const float ANIMSPEED       = g_pConfigManager->getFloat("animations:speed");

    // Process speeds
    const float WINDOWSPEED     = g_pConfigManager->getFloat("animations:windows_speed") == 0 ? ANIMSPEED : g_pConfigManager->getFloat("animations:windows_speed");
    const float BORDERSPEED     = g_pConfigManager->getFloat("animations:borders_speed") == 0 ? ANIMSPEED : g_pConfigManager->getFloat("animations:borders_speed");
    const float FADESPEED       = g_pConfigManager->getFloat("animations:fadein_speed")  == 0 ? ANIMSPEED : g_pConfigManager->getFloat("animations:fadein_speed");

    const auto BORDERACTIVECOL  = CColor(g_pConfigManager->getInt("general:col.active_border"));
    const auto BORDERINACTIVECOL = CColor(g_pConfigManager->getInt("general:col.inactive_border"));

    for (auto& w : g_pCompositor->m_lWindows) {

        // process fadeinout
        if (FADEENABLED) {
            const auto GOALALPHA = w.m_bIsMapped ? 255.f : 0.f;
            w.m_bFadingOut = false;

            if (!deltazero(w.m_fAlpha, GOALALPHA)) {
                if (deltaSmallToFlip(w.m_fAlpha, GOALALPHA)) {
                    w.m_fAlpha = GOALALPHA;
                } else {
                    if (w.m_fAlpha > GOALALPHA)
                        w.m_bFadingOut = true;
                    w.m_fAlpha = parabolic(w.m_fAlpha, GOALALPHA, FADESPEED);
                }
            }

        } else {
            if (w.m_bIsMapped)
                w.m_fAlpha = 255.f;
            else {
                w.m_fAlpha = 0.f;
                w.m_bFadingOut = false;
            }
        }

        // process the borders
        const auto& COLOR = g_pCompositor->isWindowActive(&w) ? BORDERACTIVECOL : BORDERINACTIVECOL;

        if (BORDERSENABLED) {
            if (!deltazero(COLOR, w.m_cRealBorderColor)) {
                if (deltaSmallToFlip(COLOR, w.m_cRealBorderColor)) {
                    w.m_cRealBorderColor = COLOR;
                } else {
                    w.m_cRealBorderColor = parabolic(BORDERSPEED, w.m_cRealBorderColor, COLOR);
                }
            }
        } else {
            w.m_cRealBorderColor = COLOR;
        }

        // process the window
        if (WINDOWSENABLED) {
            if (!deltazero(w.m_vRealPosition, w.m_vEffectivePosition) || !deltazero(w.m_vRealSize, w.m_vEffectiveSize)) {
                if (deltaSmallToFlip(w.m_vRealPosition, w.m_vEffectivePosition) && deltaSmallToFlip(w.m_vRealSize, w.m_vEffectiveSize)) {
                    w.m_vRealPosition = w.m_vEffectivePosition;
                    w.m_vRealSize = w.m_vEffectiveSize;
                    g_pXWaylandManager->setWindowSize(&w, w.m_vRealSize);
                } else {
                    // if it is to be animated, animate it.
                    w.m_vRealPosition = Vector2D(parabolic(w.m_vRealPosition.x, w.m_vEffectivePosition.x, WINDOWSPEED), parabolic(w.m_vRealPosition.y, w.m_vEffectivePosition.y, WINDOWSPEED));
                    w.m_vRealSize = Vector2D(parabolic(w.m_vRealSize.x, w.m_vEffectiveSize.x, WINDOWSPEED), parabolic(w.m_vRealSize.y, w.m_vEffectiveSize.y, WINDOWSPEED));
                }
            }
        } else {
            w.m_vRealPosition = w.m_vEffectivePosition;
            w.m_vRealSize = w.m_vEffectiveSize;
        }
    }
}

bool CAnimationManager::deltaSmallToFlip(const Vector2D& a, const Vector2D& b) {
    return std::abs(a.x - b.x) < 0.5f && std::abs(a.y - b.y) < 0.5f;
}

bool CAnimationManager::deltaSmallToFlip(const CColor& a, const CColor& b) {
    return std::abs(a.r - b.r) < 0.5f && std::abs(a.g - b.g) < 0.5f && std::abs(a.b - b.b) < 0.5f && std::abs(a.a - b.a) < 0.5f;
}

bool CAnimationManager::deltazero(const Vector2D& a, const Vector2D& b) {
    return a.x == b.x && a.y == b.y;
}

bool CAnimationManager::deltazero(const CColor& a, const CColor& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

double CAnimationManager::parabolic(const double from, const double to, const double incline) {
    return from + ((to - from) / incline);
}

CColor CAnimationManager::parabolic(const double incline, const CColor& from, const CColor& to) {
    CColor newColor;

    newColor.r = parabolic(from.r, to.r, incline);
    newColor.g = parabolic(from.g, to.g, incline);
    newColor.b = parabolic(from.b, to.b, incline);
    newColor.a = parabolic(from.a, to.a, incline);

    return newColor;
}