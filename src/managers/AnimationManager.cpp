#include "AnimationManager.hpp"
#include "../Compositor.hpp"

CAnimationManager::CAnimationManager() {
    std::vector<Vector2D> points = {Vector2D(0, 0.75f), Vector2D(0.15f, 1.f)};
    m_mBezierCurves["default"].setup(&points);
}

void CAnimationManager::removeAllBeziers() {
    m_mBezierCurves.clear();

    // add the default one
    std::vector<Vector2D> points = {Vector2D(0, 0.75f), Vector2D(0.15f, 1.f)};
    m_mBezierCurves["default"].setup(&points);
}

void CAnimationManager::addBezierWithName(std::string name, const Vector2D& p1, const Vector2D& p2) {
    std::vector points = {p1, p2};
    m_mBezierCurves[name].setup(&points);
}

void CAnimationManager::tick() {

    bool animationsDisabled = false;

    if (!g_pConfigManager->getInt("animations:enabled"))
        animationsDisabled = true;

    const float ANIMSPEED       = g_pConfigManager->getFloat("animations:speed");
    const auto BORDERSIZE       = g_pConfigManager->getInt("general:border_size");
    const auto BEZIERSTR        = g_pConfigManager->getString("animations:curve");

    auto DEFAULTBEZIER = m_mBezierCurves.find(BEZIERSTR);
    if (DEFAULTBEZIER == m_mBezierCurves.end())
        DEFAULTBEZIER = m_mBezierCurves.find("default");

    for (auto& av : m_lAnimatedVariables) {
        // get speed
        const auto SPEED = *av->m_pSpeed == 0 ? ANIMSPEED : *av->m_pSpeed;

        // window stuff
        const auto PWINDOW = (CWindow*)av->m_pWindow;
        wlr_box WLRBOXPREV = {PWINDOW->m_vRealPosition.vec().x - BORDERSIZE - 1, PWINDOW->m_vRealPosition.vec().y - BORDERSIZE - 1, PWINDOW->m_vRealSize.vec().x + 2 * BORDERSIZE + 2, PWINDOW->m_vRealSize.vec().y + 2 * BORDERSIZE + 2};

        // check if it's disabled, if so, warp
        if (av->m_pEnabled == 0 || animationsDisabled) {
            av->warp();
            g_pHyprRenderer->damageBox(&WLRBOXPREV);
            g_pHyprRenderer->damageWindow(PWINDOW);

            // set size and pos if valid
            if (g_pCompositor->windowValidMapped(PWINDOW))
                g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());
            continue;
        }

        // beziers are with a switch unforto
        // TODO: maybe do something cleaner

        // get the spent % (0 - 1)
        const auto DURATIONPASSED = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - av->animationBegin).count();
        const float SPENT = std::clamp((DURATIONPASSED / 100.f) / SPEED, 0.f, 1.f);

        switch (av->m_eVarType) {
            case AVARTYPE_FLOAT: {
                if (!deltazero(av->m_fValue, av->m_fGoal)) {
                    const auto DELTA = av->m_fGoal - av->m_fBegun;
                    const auto BEZIER = m_mBezierCurves.find(*av->m_pBezier);

                    if (BEZIER != m_mBezierCurves.end())
                        av->m_fValue = av->m_fBegun + BEZIER->second.getYForPoint(SPENT) * DELTA;
                    else
                        av->m_fValue = av->m_fBegun + DEFAULTBEZIER->second.getYForPoint(SPENT) * DELTA;

                    if (SPENT >= 1.f) {
                        av->warp();
                    }
                } else {
                    continue; // dont process
                }
                break;
            }
            case AVARTYPE_VECTOR: {
                if (!deltazero(av->m_vValue, av->m_vGoal)) {
                    const auto DELTA = av->m_vGoal - av->m_vBegun;
                    const auto BEZIER = m_mBezierCurves.find(*av->m_pBezier);

                    if (BEZIER != m_mBezierCurves.end())
                        av->m_vValue = av->m_vBegun + DELTA * BEZIER->second.getYForPoint(SPENT);
                    else
                        av->m_vValue = av->m_vBegun + DELTA * DEFAULTBEZIER->second.getYForPoint(SPENT);

                    if (SPENT >= 1.f) {
                        av->warp();
                    }
                } else {
                    continue;  // dont process
                }
                break;
            }
            case AVARTYPE_COLOR: {
                if (!deltazero(av->m_cValue, av->m_cGoal)) {
                    const auto DELTA = av->m_cGoal - av->m_cBegun;
                    const auto BEZIER = m_mBezierCurves.find(*av->m_pBezier);

                    if (BEZIER != m_mBezierCurves.end())
                        av->m_cValue = av->m_cBegun + DELTA * BEZIER->second.getYForPoint(SPENT);
                    else
                        av->m_cValue = av->m_cBegun + DELTA * DEFAULTBEZIER->second.getYForPoint(SPENT);

                    if (SPENT >= 1.f) {
                        av->warp();
                    }
                } else {
                    continue;  // dont process
                }
                break;
            }
            default: {
                ;
            }
        }

        // damage the window
        g_pHyprRenderer->damageBox(&WLRBOXPREV);
        g_pHyprRenderer->damageWindow(PWINDOW);

        // set size and pos if valid
        if (g_pCompositor->windowValidMapped(PWINDOW))
            g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());
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