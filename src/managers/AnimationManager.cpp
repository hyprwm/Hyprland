#include "AnimationManager.hpp"
#include "../Compositor.hpp"
#include "HookSystemManager.hpp"
#include "config/ConfigManager.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/AnimatedVariable.hpp"
#include "macros.hpp"
#include "../config/ConfigValue.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/LayerSurface.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../helpers/varlist/VarList.hpp"

#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/animation/AnimationManager.hpp>

static int wlTick(SP<CEventLoopTimer> self, void* data) {
    if (g_pAnimationManager)
        g_pAnimationManager->onTicked();

    if (g_pCompositor->m_bSessionActive && g_pAnimationManager && g_pHookSystem && !g_pCompositor->m_bUnsafeState &&
        std::ranges::any_of(g_pCompositor->m_vMonitors, [](const auto& mon) { return mon->m_bEnabled && mon->output; })) {
        g_pAnimationManager->tick();
        EMIT_HOOK_EVENT("tick", nullptr);
    }

    if (g_pAnimationManager && g_pAnimationManager->shouldTickForNext())
        g_pAnimationManager->scheduleTick();

    return 0;
}

CHyprAnimationManager::CHyprAnimationManager() {
    m_pAnimationTimer = SP<CEventLoopTimer>(new CEventLoopTimer(std::chrono::microseconds(500), wlTick, nullptr));
    g_pEventLoopManager->addTimer(m_pAnimationTimer);

    addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));
}

template <Animable VarType>
void updateVariable(CAnimatedVariable<VarType>& av, const float POINTY, bool warp = false) {
    if (POINTY >= 1.f || warp || av.value() == av.goal()) {
        av.warp();
        return;
    }

    const auto DELTA = av.goal() - av.begun();
    av.value()       = av.begun() + DELTA * POINTY;
}

void updateColorVariable(CAnimatedVariable<CHyprColor>& av, const float POINTY, bool warp) {
    if (POINTY >= 1.f || warp || av.value() == av.goal()) {
        av.warp();
        return;
    }

    // convert both to OkLab, then lerp that, and convert back.
    // This is not as fast as just lerping rgb, but it's WAY more precise...
    // Use the CHyprColor cache for OkLab

    const auto&                L1 = av.begun().asOkLab();
    const auto&                L2 = av.goal().asOkLab();

    static const auto          lerp = [](const float one, const float two, const float progress) -> float { return one + (two - one) * progress; };

    const Hyprgraphics::CColor lerped = Hyprgraphics::CColor::SOkLab{
        .l = lerp(L1.l, L2.l, POINTY),
        .a = lerp(L1.a, L2.a, POINTY),
        .b = lerp(L1.b, L2.b, POINTY),
    };

    av.value() = {lerped, lerp(av.begun().a, av.goal().a, POINTY)};
}

template <Animable VarType>
static void handleUpdate(CAnimatedVariable<VarType>& av, bool warp) {
    PHLWINDOW    PWINDOW            = av.m_Context.pWindow.lock();
    PHLWORKSPACE PWORKSPACE         = av.m_Context.pWorkspace.lock();
    PHLLS        PLAYER             = av.m_Context.pLayer.lock();
    PHLMONITOR   PMONITOR           = nullptr;
    bool         animationsDisabled = warp;

    if (PWINDOW) {
        if (av.m_Context.eDamagePolicy == AVARDAMAGE_ENTIRE)
            g_pHyprRenderer->damageWindow(PWINDOW);
        else if (av.m_Context.eDamagePolicy == AVARDAMAGE_BORDER) {
            const auto PDECO = PWINDOW->getDecorationByType(DECORATION_BORDER);
            PDECO->damageEntire();
        } else if (av.m_Context.eDamagePolicy == AVARDAMAGE_SHADOW) {
            const auto PDECO = PWINDOW->getDecorationByType(DECORATION_SHADOW);
            PDECO->damageEntire();
        }

        PMONITOR = PWINDOW->m_pMonitor.lock();
        if (!PMONITOR)
            return;

        animationsDisabled = PWINDOW->m_sWindowData.noAnim.valueOr(animationsDisabled);
    } else if (PWORKSPACE) {
        PMONITOR = PWORKSPACE->m_pMonitor.lock();
        if (!PMONITOR)
            return;

        // dont damage the whole monitor on workspace change, unless it's a special workspace, because dim/blur etc
        if (PWORKSPACE->m_bIsSpecialWorkspace)
            g_pHyprRenderer->damageMonitor(PMONITOR);

        // TODO: just make this into a damn callback already vax...
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->isHidden() || w->m_pWorkspace != PWORKSPACE)
                continue;

            if (w->m_bIsFloating && !w->m_bPinned) {
                // still doing the full damage hack for floating because sometimes when the window
                // goes through multiple monitors the last rendered frame is missing damage somehow??
                const CBox windowBoxNoOffset = w->getFullWindowBoundingBox();
                const CBox monitorBox        = {PMONITOR->vecPosition, PMONITOR->vecSize};
                if (windowBoxNoOffset.intersection(monitorBox) != windowBoxNoOffset) // on edges between multiple monitors
                    g_pHyprRenderer->damageWindow(w, true);
            }

            if (PWORKSPACE->m_bIsSpecialWorkspace)
                g_pHyprRenderer->damageWindow(w, true); // hack for special too because it can cross multiple monitors
        }

        // damage any workspace window that is on any monitor
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (!validMapped(w) || w->m_pWorkspace != PWORKSPACE || w->m_bPinned)
                continue;

            g_pHyprRenderer->damageWindow(w);
        }
    } else if (PLAYER) {
        // "some fucking layers miss 1 pixel???" -- vaxry
        CBox expandBox = CBox{PLAYER->realPosition->value(), PLAYER->realSize->value()};
        expandBox.expand(5);
        g_pHyprRenderer->damageBox(&expandBox);

        PMONITOR = g_pCompositor->getMonitorFromVector(PLAYER->realPosition->goal() + PLAYER->realSize->goal() / 2.F);
        if (!PMONITOR)
            return;
        animationsDisabled = animationsDisabled || PLAYER->noAnimations;
    }

    const auto SPENT   = av.getPercent();
    const auto PBEZIER = g_pAnimationManager->getBezier(av.getBezierName());
    const auto POINTY  = PBEZIER->getYForPoint(SPENT);

    if constexpr (std::same_as<VarType, CHyprColor>) {
        updateColorVariable(av, POINTY, animationsDisabled);
    } else {
        updateVariable<VarType>(av, POINTY, animationsDisabled);
    }

    av.onUpdate();

    switch (av.m_Context.eDamagePolicy) {
        case AVARDAMAGE_ENTIRE: {
            if (PWINDOW) {
                PWINDOW->updateWindowDecos();
                g_pHyprRenderer->damageWindow(PWINDOW);
            } else if (PWORKSPACE) {
                for (auto const& w : g_pCompositor->m_vWindows) {
                    if (!validMapped(w) || w->m_pWorkspace != PWORKSPACE)
                        continue;

                    w->updateWindowDecos();

                    // damage any workspace window that is on any monitor
                    if (!w->m_bPinned)
                        g_pHyprRenderer->damageWindow(w);
                }
            } else if (PLAYER) {
                if (PLAYER->layer <= 1)
                    g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR);

                // some fucking layers miss 1 pixel???
                CBox expandBox = CBox{PLAYER->realPosition->value(), PLAYER->realSize->value()};
                expandBox.expand(5);
                g_pHyprRenderer->damageBox(&expandBox);
            }
            break;
        }
        case AVARDAMAGE_BORDER: {
            RASSERT(PWINDOW, "Tried to AVARDAMAGE_BORDER a non-window AVAR!");

            const auto PDECO = PWINDOW->getDecorationByType(DECORATION_BORDER);
            PDECO->damageEntire();

            break;
        }
        case AVARDAMAGE_SHADOW: {
            RASSERT(PWINDOW, "Tried to AVARDAMAGE_SHADOW a non-window AVAR!");

            const auto PDECO = PWINDOW->getDecorationByType(DECORATION_SHADOW);

            PDECO->damageEntire();

            break;
        }
        default: {
            break;
        }
    }

    // manually schedule a frame
    if (PMONITOR)
        g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

void CHyprAnimationManager::tick() {
    static std::chrono::time_point lastTick = std::chrono::high_resolution_clock::now();
    m_fLastTickTime                         = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    static auto PANIMENABLED = CConfigValue<Hyprlang::INT>("animations:enabled");
    for (auto const& pav : m_vActiveAnimatedVariables) {
        const auto PAV = pav.lock();
        if (!PAV)
            continue;

        // for disabled anims just warp
        bool warp = !*PANIMENABLED || !PAV->enabled();

        switch (PAV->m_Type) {
            case AVARTYPE_FLOAT: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<float>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated float");
                handleUpdate(*pTypedAV, warp);
            } break;
            case AVARTYPE_VECTOR: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<Vector2D>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated Vector2D");
                handleUpdate(*pTypedAV, warp);
            } break;
            case AVARTYPE_COLOR: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<CHyprColor>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated CHyprColor");
                handleUpdate(*pTypedAV, warp);
            } break;
            default: UNREACHABLE();
        }
    }

    tickDone();
}

void CHyprAnimationManager::scheduleTick() {
    if (m_bTickScheduled)
        return;

    m_bTickScheduled = true;

    const auto PMOSTHZ = g_pHyprRenderer->m_pMostHzMonitor;

    if (!PMOSTHZ) {
        m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(16));
        return;
    }

    float       refreshDelayMs = std::floor(1000.f / PMOSTHZ->refreshRate);

    const float SINCEPRES = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - PMOSTHZ->lastPresentationTimer.chrono()).count() / 1000.f;

    const auto  TOPRES = std::clamp(refreshDelayMs - SINCEPRES, 1.1f, 1000.f); // we can't send 0, that will disarm it

    m_pAnimationTimer->updateTimeout(std::chrono::milliseconds((int)std::floor(TOPRES)));
}

void CHyprAnimationManager::onTicked() {
    m_bTickScheduled = false;
}

//
// Anims
//
//

void CAnimationManager::animationPopin(PHLWINDOW pWindow, bool close, float minPerc) {
    const auto GOALPOS  = pWindow->m_vRealPosition.goal();
    const auto GOALSIZE = pWindow->m_vRealSize.goal();

    if (!close) {
        pWindow->m_vRealSize.setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_vRealPosition.setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_Value / 2.f);
    } else {
        pWindow->m_vRealSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        pWindow->m_vRealPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_Goal / 2.f;
    }
}

void CAnimationManager::animationSlide(PHLWINDOW pWindow, std::string force, bool close) {
    pWindow->m_vRealSize.warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_vRealPosition.goal();
    const auto GOALSIZE = pWindow->m_vRealSize.goal();

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!PMONITOR)
        return; // unsafe state most likely

    Vector2D posOffset;

    if (force != "") {
        if (force == "bottom")
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y);
        else if (force == "left")
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
        else if (force == "right")
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y - GOALSIZE.y);

        if (!close)
            pWindow->m_vRealPosition.setValue(posOffset);
        else
            pWindow->m_vRealPosition = posOffset;

        return;
    }

    const auto MIDPOINT = GOALPOS + GOALSIZE / 2.f;

    // check sides it touches
    const bool DISPLAYLEFT   = STICKS(pWindow->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pWindow->m_vPosition.x + pWindow->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pWindow->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pWindow->m_vPosition.y + pWindow->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    if (DISPLAYBOTTOM && DISPLAYTOP) {
        if (DISPLAYLEFT && DISPLAYRIGHT) {
            posOffset = GOALPOS + Vector2D(0.0, GOALSIZE.y);
        } else if (DISPLAYLEFT) {
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
        } else {
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
        }
    } else if (DISPLAYTOP) {
        posOffset = GOALPOS - Vector2D(0.0, GOALSIZE.y);
    } else if (DISPLAYBOTTOM) {
        posOffset = GOALPOS + Vector2D(0.0, GOALSIZE.y);
    } else {
        if (MIDPOINT.y > PMONITOR->vecPosition.y + PMONITOR->vecSize.y / 2.f)
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y - GOALSIZE.y);
    }

    if (!close)
        pWindow->m_vRealPosition.setValue(posOffset);
    else
        pWindow->m_vRealPosition = posOffset;
}

void CAnimationManager::onWindowPostCreateClose(PHLWINDOW pWindow, bool close) {
    if (!close) {
        pWindow->m_vRealPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("windowsIn");
        pWindow->m_vRealSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("windowsIn");
        pWindow->m_fAlpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeIn");
    } else {
        pWindow->m_vRealPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("windowsOut");
        pWindow->m_vRealSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("windowsOut");
        pWindow->m_fAlpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeOut");
    }

    auto ANIMSTYLE = pWindow->m_vRealPosition.m_pConfig->pValues->internalStyle;
    transform(ANIMSTYLE.begin(), ANIMSTYLE.end(), ANIMSTYLE.begin(), ::tolower);

    CVarList animList(ANIMSTYLE, 0, 's');

    // if the window is not being animated, that means the layout set a fixed size for it, don't animate.
    if (!pWindow->m_vRealPosition.isBeingAnimated() && !pWindow->m_vRealSize.isBeingAnimated())
        return;

    // if the animation is disabled and we are leaving, ignore the anim to prevent the snapshot being fucked
    if (!pWindow->m_vRealPosition.m_pConfig->pValues->internalEnabled)
        return;

    if (pWindow->m_sWindowData.animationStyle.hasValue()) {
        const auto STYLE = pWindow->m_sWindowData.animationStyle.value();
        // the window has config'd special anim
        if (STYLE.starts_with("slide")) {
            CVarList animList2(STYLE, 0, 's');
            animationSlide(pWindow, animList2[1], close);
        } else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (STYLE.find("%") != std::string::npos) {
                try {
                    auto percstr = STYLE.substr(STYLE.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, close, minPerc / 100.f);
        }
    } else {
        if (animList[0] == "slide")
            animationSlide(pWindow, animList[1], close);
        else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (!ANIMSTYLE.starts_with("%")) {
                try {
                    auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, close, minPerc / 100.f);
        }
    }
}

std::string CAnimationManager::styleValidInConfigVar(const std::string& config, const std::string& style) {
    if (config.starts_with("window")) {
        if (style.starts_with("slide"))
            return "";
        else if (style.starts_with("popin")) {
            // try parsing
            float minPerc = 0.f;
            if (style.find('%') != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid minperc"; }

                return "";
            }

            minPerc; // fix warning

            return "";
        }

        return "unknown style";
    } else if (config.starts_with("workspaces") || config.starts_with("specialWorkspace")) {
        if (style == "slide" || style == "slidevert" || style == "fade")
            return "";
        else if (style.starts_with("slidefade")) {
            // try parsing
            float movePerc = 0.f;
            if (style.find('%') != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' ') + 1);
                    movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid movePerc"; }

                return "";
            }

            movePerc; // fix warning

            return "";
        }

        return "unknown style";
    } else if (config == "borderangle") {
        if (style == "loop" || style == "once")
            return "";
        return "unknown style";
    } else if (config.starts_with("layers")) {
        if (style == "fade" || style == "" || style == "slide")
            return "";
        else if (style.starts_with("popin")) {
            // try parsing
            float minPerc = 0.f;
            if (style.find('%') != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid minperc"; }

                return "";
            }

            minPerc; // fix warning

            return "";
        }
        return "";
        return "unknown style";
    } else {
        return "animation has no styles";
    }

    return "";
}

CBezierCurve* CAnimationManager::getBezier(const std::string& name) {
    const auto BEZIER = std::find_if(m_mBezierCurves.begin(), m_mBezierCurves.end(), [&](const auto& other) { return other.first == name; });

    return BEZIER == m_mBezierCurves.end() ? &m_mBezierCurves["default"] : &BEZIER->second;
}

std::unordered_map<std::string, CBezierCurve> CAnimationManager::getAllBeziers() {
    return m_mBezierCurves;
}

bool CAnimationManager::shouldTickForNext() {
    return !m_vActiveAnimatedVariables.empty();
}

void CAnimationManager::scheduleTick() {
    if (m_bTickScheduled)
        return;

    m_bTickScheduled = true;

    const auto PMOSTHZ = g_pHyprRenderer->m_pMostHzMonitor;

    if (!PMOSTHZ) {
        m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(16));
        return;
    }

    float       refreshDelayMs = std::floor(1000.f / PMOSTHZ->refreshRate);

    const float SINCEPRES = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - PMOSTHZ->lastPresentationTimer.chrono()).count() / 1000.f;

    const auto  TOPRES = std::clamp(refreshDelayMs - SINCEPRES, 1.1f, 1000.f); // we can't send 0, that will disarm it

    m_pAnimationTimer->updateTimeout(std::chrono::milliseconds((int)std::floor(TOPRES)));
}
