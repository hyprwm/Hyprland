#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"
#include "../Compositor.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer = this;
    alpha.registerVar();
}

SLayerSurface::~SLayerSurface() {
    if (!g_pHyprOpenGL)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mLayerFramebuffers, [&](const auto& other) { return other.first == this; });
}

void SLayerSurface::applyRules() {
    noAnimations     = false;
    forceBlur        = false;
    ignoreAlpha      = false;
    ignoreAlphaValue = 0.f;
    xray             = -1;

    for (auto& rule : g_pConfigManager->getMatchingRules(this)) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule.starts_with("ignorealpha") || rule.rule.starts_with("ignorezero")) {
            const auto  FIRST_SPACE_POS = rule.rule.find_first_of(' ');
            std::string alphaValue      = "";
            if (FIRST_SPACE_POS != std::string::npos)
                alphaValue = rule.rule.substr(FIRST_SPACE_POS + 1);

            try {
                ignoreAlpha = true;
                if (!alphaValue.empty())
                    ignoreAlphaValue = std::stof(alphaValue);
            } catch (...) { Debug::log(ERR, "Invalid value passed to ignoreAlpha"); }
        } else if (rule.rule.starts_with("xray")) {
            CVarList vars{rule.rule, 0, ' '};
            try {
                xray = configStringToInt(vars[1]);
            } catch (...) {}
        }
    }
}

CRegion SConstraint::getLogicCoordsRegion() {
    CRegion result;

    if (!constraint)
        return result;

    const auto PWINDOWOWNER = g_pCompositor->getWindowFromSurface(constraint->surface);

    if (!PWINDOWOWNER)
        return result;

    result.add(&constraint->region); // surface-local coords

    if (!PWINDOWOWNER->m_bIsX11) {
        result.translate(PWINDOWOWNER->m_vRealPosition.goalv());
        return result;
    }

    const auto COORDS = PWINDOWOWNER->m_bIsMapped ? PWINDOWOWNER->m_vRealPosition.goalv() :
                                                    g_pXWaylandManager->xwaylandToWaylandCoords({PWINDOWOWNER->m_uSurface.xwayland->x, PWINDOWOWNER->m_uSurface.xwayland->y});

    const auto PMONITOR = PWINDOWOWNER->m_bIsMapped ? g_pCompositor->getMonitorFromID(PWINDOWOWNER->m_iMonitorID) : g_pCompositor->getMonitorFromVector(COORDS);

    if (!PMONITOR)
        return CRegion{};

    result.scale(PMONITOR->xwaylandScale);

    result.translate(COORDS);

    return result;
}

Vector2D SConstraint::getLogicConstraintPos() {
    if (!constraint)
        return {};

    const auto PWINDOWOWNER = g_pCompositor->getWindowFromSurface(constraint->surface);

    if (!PWINDOWOWNER)
        return {};

    if (!PWINDOWOWNER->m_bIsX11)
        return PWINDOWOWNER->m_vRealPosition.goalv();

    const auto COORDS = PWINDOWOWNER->m_bIsMapped ? PWINDOWOWNER->m_vRealPosition.goalv() :
                                                    g_pXWaylandManager->xwaylandToWaylandCoords({PWINDOWOWNER->m_uSurface.xwayland->x, PWINDOWOWNER->m_uSurface.xwayland->y});

    return COORDS;
}

Vector2D SConstraint::getLogicConstraintSize() {
    if (!constraint)
        return {};

    const auto PWINDOWOWNER = g_pCompositor->getWindowFromSurface(constraint->surface);

    if (!PWINDOWOWNER)
        return {};

    if (!PWINDOWOWNER->m_bIsX11)
        return PWINDOWOWNER->m_vRealSize.goalv();

    const auto PMONITOR = PWINDOWOWNER->m_bIsMapped ?
        g_pCompositor->getMonitorFromID(PWINDOWOWNER->m_iMonitorID) :
        g_pCompositor->getMonitorFromVector(g_pXWaylandManager->xwaylandToWaylandCoords({PWINDOWOWNER->m_uSurface.xwayland->x, PWINDOWOWNER->m_uSurface.xwayland->y}));

    if (!PMONITOR)
        return {};

    const auto SIZE = PWINDOWOWNER->m_bIsMapped ? PWINDOWOWNER->m_vRealSize.goalv() :
                                                  Vector2D{PWINDOWOWNER->m_uSurface.xwayland->width, PWINDOWOWNER->m_uSurface.xwayland->height} * PMONITOR->xwaylandScale;

    return SIZE;
}