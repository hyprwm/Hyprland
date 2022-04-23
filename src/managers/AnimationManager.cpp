#include "AnimationManager.hpp"
#include "../Compositor.hpp"

void CAnimationManager::tick() {

    bool animationsDisabled = false;

    if (!g_pConfigManager->getInt("animations:enabled"))
        animationsDisabled = true;

    const float ANIMSPEED       = g_pConfigManager->getFloat("animations:speed");

    const auto BORDERSIZE       = g_pConfigManager->getInt("general:border_size");

    for (auto& av : m_lAnimatedVariables) {
        // first, we check if it's disabled, if so, warp
        if (av->m_pEnabled == 0 || animationsDisabled) {
            av->warp();
            continue;
        }

        // get speed
        const auto SPEED = *av->m_pSpeed == 0 ? ANIMSPEED : *av->m_pSpeed;

        // window stuff
        const auto PWINDOW = (CWindow*)av->m_pWindow;
        bool needsDamage = false;
        wlr_box WLRBOXPREV = {PWINDOW->m_vRealPosition.vec().x - BORDERSIZE - 1, PWINDOW->m_vRealPosition.vec().y - BORDERSIZE - 1, PWINDOW->m_vRealSize.vec().x + 2 * BORDERSIZE + 2, PWINDOW->m_vRealSize.vec().y + 2 * BORDERSIZE + 2};

        // TODO: curves

        // parabolic with a switch unforto
        // TODO: maybe do something cleaner
        switch (av->m_eVarType) {
            case AVARTYPE_FLOAT: {
                if (!deltazero(av->m_fValue, av->m_fGoal)) {
                    if (deltaSmallToFlip(av->m_fValue, av->m_fGoal)) {
                        av->warp();
                    } else {
                        av->m_fValue = parabolic(av->m_fValue, av->m_fGoal, SPEED);
                    }

                    needsDamage = true;
                }
                break;
            }
            case AVARTYPE_VECTOR: {
                if (!deltazero(av->m_vValue, av->m_vGoal)) {
                    if (deltaSmallToFlip(av->m_vValue, av->m_vGoal)) {
                        av->warp();
                    } else {
                        av->m_vValue.x = parabolic(av->m_vValue.x, av->m_vGoal.x, SPEED);
                        av->m_vValue.y = parabolic(av->m_vValue.y, av->m_vGoal.y, SPEED);
                    }
                    needsDamage = true;
                }
                break;
            }
            case AVARTYPE_COLOR: {
                if (!deltazero(av->m_cValue, av->m_cGoal)) {
                    if (deltaSmallToFlip(av->m_cValue, av->m_cGoal)) {
                        av->warp();
                    } else {
                        av->m_cValue = parabolic(SPEED, av->m_cValue, av->m_cGoal);
                    }
                    needsDamage = true;
                }
                break;
            }
            default: {
                ;
            }
        }

        // invalidate the window
        if (needsDamage) {
            g_pHyprRenderer->damageBox(&WLRBOXPREV);
            g_pHyprRenderer->damageWindow(PWINDOW);
        }
    }
}

bool CAnimationManager::deltaSmallToFlip(const Vector2D& a, const Vector2D& b) {
    return std::abs(a.x - b.x) < 0.5f && std::abs(a.y - b.y) < 0.5f;
}

bool CAnimationManager::deltaSmallToFlip(const CColor& a, const CColor& b) {
    return std::abs(a.r - b.r) < 0.5f && std::abs(a.g - b.g) < 0.5f && std::abs(a.b - b.b) < 0.5f && std::abs(a.a - b.a) < 0.5f;
}

bool CAnimationManager::deltaSmallToFlip(const float& a, const float& b) {
    return std::abs(a - b) < 0.5f;
}

bool CAnimationManager::deltazero(const Vector2D& a, const Vector2D& b) {
    return a.x == b.x && a.y == b.y;
}

bool CAnimationManager::deltazero(const float& a, const float& b) {
    return a == b;
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