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
void updateVariable(CAnimatedVariable<VarType>* pav, const float POINTY, bool warp = false) {
    if (POINTY >= 1.f || warp || pav->value() == pav->goal()) {
        pav->warp();
        return;
    }

    const auto DELTA = pav->goal() - pav->begun();
    pav->value()     = pav->begun() + DELTA * POINTY;
}

void updateColorVariable(CAnimatedVariable<CHyprColor>* pav, const float POINTY, bool warp) {
    if (POINTY >= 1.f || warp || pav->value() == pav->goal()) {
        pav->warp();
        return;
    }

    // convert both to OkLab, then lerp that, and convert back.
    // This is not as fast as just lerping rgb, but it's WAY more precise...
    // Use the CHyprColor cache for OkLab

    const auto&                L1 = pav->begun().asOkLab();
    const auto&                L2 = pav->goal().asOkLab();

    static const auto          lerp = [](const float one, const float two, const float progress) -> float { return one + (two - one) * progress; };

    const Hyprgraphics::CColor lerped = Hyprgraphics::CColor::SOkLab{
        .l = lerp(L1.l, L2.l, POINTY),
        .a = lerp(L1.a, L2.a, POINTY),
        .b = lerp(L1.b, L2.b, POINTY),
    };

    pav->value() = {lerped, lerp(pav->begun().a, pav->goal().a, POINTY)};
}

static void damageAnimatedWindow(PHLWINDOW PWINDOW, eAVarDamagePolicy policy) {
    const bool VISIBLE = PWINDOW && PWINDOW->m_pWorkspace ? PWINDOW->m_pWorkspace->isVisible() : true;
    if (!VISIBLE)
        return;

    if (policy == AVARDAMAGE_ENTIRE) {
        PWINDOW->updateWindowDecos();
        g_pHyprRenderer->damageWindow(PWINDOW);
    } else if (policy == AVARDAMAGE_BORDER) {
        const auto PDECO = PWINDOW->getDecorationByType(DECORATION_BORDER);
        PDECO->damageEntire();
    } else if (policy == AVARDAMAGE_SHADOW) {
        const auto PDECO = PWINDOW->getDecorationByType(DECORATION_SHADOW);
        PDECO->damageEntire();
    }

    // set size and pos if valid, but only if damage policy entire (dont if border for example)
    if (validMapped(PWINDOW) && policy == AVARDAMAGE_ENTIRE && !PWINDOW->isX11OverrideRedirect())
        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize->goal());

    if (PHLMONITOR PMONITOR = PWINDOW->m_pMonitor.lock())
        g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

static void damageAnimatedWorkspace(PHLWORKSPACE PWORKSPACE, eAVarDamagePolicy policy) {
    RASSERT(policy == AVARDAMAGE_ENTIRE, "Workspace animations should be AVARDAMAGE_ENTIRE");

    PHLMONITOR PMONITOR = PWORKSPACE->m_pMonitor.lock();
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

        w->updateWindowDecos();
        g_pHyprRenderer->damageWindow(w);
    }

    g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

static void damageAnimatedLayer(PHLLS PLAYER, eAVarDamagePolicy policy) {
    RASSERT(policy == AVARDAMAGE_ENTIRE, "Layer animations should be AVARDAMAGE_ENTIRE");

    // "some fucking layers miss 1 pixel???" -- vaxry
    CBox expandBox = CBox{PLAYER->realPosition->value(), PLAYER->realSize->value()};
    expandBox.expand(5);
    g_pHyprRenderer->damageBox(&expandBox);

    PHLMONITOR PMONITOR = g_pCompositor->getMonitorFromVector(PLAYER->realPosition->goal() + PLAYER->realSize->goal() / 2.F);
    if (!PMONITOR)
        return;

    if (PLAYER->layer <= 1)
        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR);

    g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

static void damageBefore(const SAnimationContext& ctx) {
    PHLWINDOW    PWINDOW    = ctx.pWindow.lock();
    PHLWORKSPACE PWORKSPACE = ctx.pWorkspace.lock();
    PHLLS        PLAYER     = ctx.pLayer.lock();

    if (ctx.eDamagePolicy == AVARDAMAGE_NONE)
        return;

    if (PWINDOW) {
        damageAnimatedWindow(PWINDOW, ctx.eDamagePolicy);
    } else if (PWORKSPACE)
        damageAnimatedWorkspace(PWORKSPACE, ctx.eDamagePolicy);
    else if (PLAYER) {
        damageAnimatedLayer(PLAYER, ctx.eDamagePolicy);
    }
}

static void damageAfter(const SAnimationContext& ctx) {
    PHLWINDOW PWINDOW = ctx.pWindow.lock();

    if (PWINDOW) {
        if (ctx.eDamagePolicy == AVARDAMAGE_ENTIRE) {
            PWINDOW->updateWindowDecos();
            g_pHyprRenderer->damageWindow(PWINDOW);
        }
    }
}

static bool shouldWarp(const SAnimationContext& ctx) {
    PHLWINDOW PWINDOW = ctx.pWindow.lock();
    PHLLS     PLAYER  = ctx.pLayer.lock();

    if (PWINDOW)
        return PWINDOW->m_sWindowData.noAnim.valueOr(false);
    else if (PLAYER)
        return PLAYER->noAnimations;

    return false;
}

void CHyprAnimationManager::tick() {
    static std::chrono::time_point lastTick = std::chrono::high_resolution_clock::now();
    m_fLastTickTime                         = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    static auto PANIMENABLED = CConfigValue<Hyprlang::INT>("animations:enabled");
    for (auto const& pav : m_vActiveAnimatedVariables) {
        const auto PAV = pav.lock();
        // for disabled anims just warp
        bool       warp = !*PANIMENABLED || !PAV->enabled();

        const auto SPENT   = PAV->getPercent();
        const auto PBEZIER = getBezier(PAV->getBezierName());
        const auto POINTY  = PBEZIER->getYForPoint(SPENT);

        switch (PAV->m_Type) {
            case AVARTYPE_FLOAT: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<float>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated float");

                damageBefore(pTypedAV->m_Context);
                updateVariable(pTypedAV, POINTY, warp || shouldWarp(pTypedAV->m_Context));
                damageAfter(pTypedAV->m_Context);
            } break;
            case AVARTYPE_VECTOR: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<Vector2D>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated Vector2D");

                damageBefore(pTypedAV->m_Context);
                updateVariable(pTypedAV, POINTY, warp || shouldWarp(pTypedAV->m_Context));
                damageAfter(pTypedAV->m_Context);
            } break;
            case AVARTYPE_COLOR: {
                auto pTypedAV = dynamic_cast<CAnimatedVariable<CHyprColor>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated CHyprColor");

                damageBefore(pTypedAV->m_Context);
                updateColorVariable(pTypedAV, POINTY, warp || shouldWarp(pTypedAV->m_Context));
                damageAfter(pTypedAV->m_Context);
            } break;
            default: UNREACHABLE();
        }

        PAV->onUpdate();
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

void CHyprAnimationManager::animationPopin(PHLWINDOW pWindow, bool close, float minPerc) {
    const auto GOALPOS  = pWindow->m_vRealPosition->goal();
    const auto GOALSIZE = pWindow->m_vRealSize->goal();

    if (!close) {
        pWindow->m_vRealSize->setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_vRealPosition->setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize->value() / 2.f);
    } else {
        *pWindow->m_vRealSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        *pWindow->m_vRealPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize->goal() / 2.f;
    }
}

void CHyprAnimationManager::animationSlide(PHLWINDOW pWindow, std::string force, bool close) {
    pWindow->m_vRealSize->warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_vRealPosition->goal();
    const auto GOALSIZE = pWindow->m_vRealSize->goal();

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
            pWindow->m_vRealPosition->setValue(posOffset);
        else
            *pWindow->m_vRealPosition = posOffset;

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
        pWindow->m_vRealPosition->setValue(posOffset);
    else
        *pWindow->m_vRealPosition = posOffset;
}

void CHyprAnimationManager::onWindowPostCreateClose(PHLWINDOW pWindow, bool close) {
    if (!close) {
        pWindow->m_vRealPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_vRealSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_fAlpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));
    } else {
        pWindow->m_vRealPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_vRealSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_fAlpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
    }

    std::string ANIMSTYLE = pWindow->m_vRealPosition->getStyle();
    transform(ANIMSTYLE.begin(), ANIMSTYLE.end(), ANIMSTYLE.begin(), ::tolower);

    CVarList animList(ANIMSTYLE, 0, 's');

    // if the window is not being animated, that means the layout set a fixed size for it, don't animate.
    if (!pWindow->m_vRealPosition->isBeingAnimated() && !pWindow->m_vRealSize->isBeingAnimated())
        return;

    // if the animation is disabled and we are leaving, ignore the anim to prevent the snapshot being fucked
    if (!pWindow->m_vRealPosition->enabled())
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

std::string CHyprAnimationManager::styleValidInConfigVar(const std::string& config, const std::string& style) {
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
