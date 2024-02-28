#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"
#include "../Compositor.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeLayers"), nullptr, AVARDAMAGE_ENTIRE);
    realPosition.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("layers"), nullptr, AVARDAMAGE_ENTIRE);
    realSize.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("layers"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer        = this;
    realPosition.m_pLayer = this;
    realSize.m_pLayer     = this;
    alpha.registerVar();
    realPosition.registerVar();
    realSize.registerVar();

    alpha.setValueAndWarp(0.f);
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
    animationStyle.reset();

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
        } else if (rule.rule.starts_with("animation")) {
            CVarList vars{rule.rule, 2, 's'};
            animationStyle = vars[1];
        }
    }
}

void SLayerSurface::startAnimation(bool in, bool instant) {
    const auto ANIMSTYLE = animationStyle.value_or(realPosition.m_pConfig->pValues->internalStyle);

    if (ANIMSTYLE == "slide") {
        // get closest edge
        const auto                    MIDDLE = geometry.middle();

        const auto                    PMONITOR = g_pCompositor->getMonitorFromVector(MIDDLE);

        const std::array<Vector2D, 4> edgePoints = {
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, 0},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{0, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x, PMONITOR->vecSize.y / 2},
        };

        float  closest = std::numeric_limits<float>::max();
        size_t leader  = 0;
        for (size_t i = 0; i < 4; ++i) {
            float dist = MIDDLE.distance(edgePoints[i]);
            if (dist < closest) {
                leader  = i;
                closest = dist;
            }
        }

        realSize.setValueAndWarp(geometry.size());
        alpha.setValueAndWarp(1.f);

        Vector2D prePos;

        switch (leader) {
            case 0:
                // TOP
                prePos = {geometry.x, PMONITOR->vecPosition.y - geometry.h};
                break;
            case 1:
                // BOTTOM
                prePos = {geometry.x, PMONITOR->vecPosition.y + PMONITOR->vecPosition.y};
                break;
            case 2:
                // LEFT
                prePos = {PMONITOR->vecPosition.x - geometry.w, geometry.y};
                break;
            case 3:
                // RIGHT
                prePos = {PMONITOR->vecPosition.x + PMONITOR->vecSize.x, geometry.y};
                break;
            default: UNREACHABLE();
        }

        if (in) {
            realPosition.setValueAndWarp(prePos);
            realPosition = geometry.pos();
        } else {
            realPosition.setValueAndWarp(geometry.pos());
            realPosition = prePos;
        }

    } else if (ANIMSTYLE.starts_with("popin")) {
        float minPerc = 0.f;
        if (ANIMSTYLE.find("%") != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) {
                ; // oops
            }
        }

        minPerc *= 0.01;

        const auto GOALSIZE = (geometry.size() * minPerc).clamp({5, 5});
        const auto GOALPOS  = geometry.pos() + (geometry.size() - GOALSIZE) / 2.f;

        alpha.setValueAndWarp(in ? 0.f : 1.f);
        alpha = in ? 1.f : 0.f;

        if (in) {
            realSize.setValueAndWarp(GOALSIZE);
            realPosition.setValueAndWarp(GOALPOS);
            realSize     = geometry.size();
            realPosition = geometry.pos();
        } else {
            realSize.setValueAndWarp(geometry.size());
            realPosition.setValueAndWarp(geometry.pos());
            realSize     = GOALSIZE;
            realPosition = GOALPOS;
        }
    } else {
        // fade
        realPosition.setValueAndWarp(geometry.pos());
        realSize.setValueAndWarp(geometry.size());
        alpha = in ? 1.f : 0.f;
    }

    if (!in)
        fadingOut = true;
}

bool SLayerSurface::isFadedOut() {
    if (!fadingOut)
        return false;

    return !realPosition.isBeingAnimated() && !realSize.isBeingAnimated() && !alpha.isBeingAnimated();
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