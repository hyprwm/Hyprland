#include "AnimationManager.hpp"
#include "../Compositor.hpp"
#include "HookSystemManager.hpp"
#include "../config/ConfigManager.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../macros.hpp"
#include "../config/ConfigValue.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/LayerSurface.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../render/Renderer.hpp"

#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/animation/AnimationManager.hpp>

static int wlTick(SP<CEventLoopTimer> self, void* data) {
    if (g_pAnimationManager)
        g_pAnimationManager->onTicked();

    if (g_pCompositor->m_sessionActive && g_pAnimationManager && g_pHookSystem && !g_pCompositor->m_unsafeState &&
        std::ranges::any_of(g_pCompositor->m_monitors, [](const auto& mon) { return mon->m_enabled && mon->m_output; })) {
        g_pAnimationManager->tick();
        EMIT_HOOK_EVENT("tick", nullptr);
    }

    if (g_pAnimationManager && g_pAnimationManager->shouldTickForNext())
        g_pAnimationManager->scheduleTick();

    return 0;
}

CHyprAnimationManager::CHyprAnimationManager() {
    m_animationTimer = SP<CEventLoopTimer>(new CEventLoopTimer(std::chrono::microseconds(500), wlTick, nullptr));
    if (g_pEventLoopManager) // null in --verify-config mode
        g_pEventLoopManager->addTimer(m_animationTimer);

    addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));
}

template <Animable VarType>
static void updateVariable(CAnimatedVariable<VarType>& av, const float POINTY, bool warp = false) {
    if (warp || av.value() == av.goal()) {
        av.warp(true, false);
        return;
    }

    const auto DELTA = av.goal() - av.begun();
    av.value()       = av.begun() + DELTA * POINTY;
}

static void updateColorVariable(CAnimatedVariable<CHyprColor>& av, const float POINTY, bool warp) {
    if (warp || av.value() == av.goal()) {
        av.warp(true, false);
        return;
    }

    // convert both to OkLab, then lerp that, and convert back.
    // This is not as fast as just lerping rgb, but it's WAY more precise...
    // Use the CHyprColor cache for OkLab

    const auto&                L1 = av.begun().asOkLab();
    const auto&                L2 = av.goal().asOkLab();

    static const auto          lerp = [](const float one, const float two, const float progress) -> float { return one + ((two - one) * progress); };

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

        PMONITOR = PWINDOW->m_monitor.lock();
        if (!PMONITOR)
            return;

        animationsDisabled = PWINDOW->m_windowData.noAnim.valueOr(animationsDisabled);
    } else if (PWORKSPACE) {
        PMONITOR = PWORKSPACE->m_monitor.lock();
        if (!PMONITOR)
            return;

        // dont damage the whole monitor on workspace change, unless it's a special workspace, because dim/blur etc
        if (PWORKSPACE->m_isSpecialWorkspace)
            g_pHyprRenderer->damageMonitor(PMONITOR);

        // TODO: just make this into a damn callback already vax...
        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->isHidden() || w->m_workspace != PWORKSPACE)
                continue;

            if (w->m_isFloating && !w->m_pinned) {
                // still doing the full damage hack for floating because sometimes when the window
                // goes through multiple monitors the last rendered frame is missing damage somehow??
                const CBox windowBoxNoOffset = w->getFullWindowBoundingBox();
                const CBox monitorBox        = {PMONITOR->m_position, PMONITOR->m_size};
                if (windowBoxNoOffset.intersection(monitorBox) != windowBoxNoOffset) // on edges between multiple monitors
                    g_pHyprRenderer->damageWindow(w, true);
            }

            if (PWORKSPACE->m_isSpecialWorkspace)
                g_pHyprRenderer->damageWindow(w, true); // hack for special too because it can cross multiple monitors
        }

        // damage any workspace window that is on any monitor
        for (auto const& w : g_pCompositor->m_windows) {
            if (!validMapped(w) || w->m_workspace != PWORKSPACE || w->m_pinned)
                continue;

            g_pHyprRenderer->damageWindow(w);
        }
    } else if (PLAYER) {
        // "some fucking layers miss 1 pixel???" -- vaxry
        CBox expandBox = CBox{PLAYER->m_realPosition->value(), PLAYER->m_realSize->value()};
        expandBox.expand(5);
        g_pHyprRenderer->damageBox(expandBox);

        PMONITOR = g_pCompositor->getMonitorFromVector(PLAYER->m_realPosition->goal() + PLAYER->m_realSize->goal() / 2.F);
        if (!PMONITOR)
            return;
        animationsDisabled = animationsDisabled || PLAYER->m_noAnimations;
    }

    const auto SPENT   = av.getPercent();
    const auto PBEZIER = g_pAnimationManager->getBezier(av.getBezierName());
    const auto POINTY  = PBEZIER->getYForPoint(SPENT);
    const bool WARP    = animationsDisabled || SPENT >= 1.f;

    if constexpr (std::same_as<VarType, CHyprColor>)
        updateColorVariable(av, POINTY, WARP);
    else
        updateVariable<VarType>(av, POINTY, WARP);

    av.onUpdate();

    switch (av.m_Context.eDamagePolicy) {
        case AVARDAMAGE_ENTIRE: {
            if (PWINDOW) {
                PWINDOW->updateWindowDecos();
                g_pHyprRenderer->damageWindow(PWINDOW);
            } else if (PWORKSPACE) {
                for (auto const& w : g_pCompositor->m_windows) {
                    if (!validMapped(w) || w->m_workspace != PWORKSPACE)
                        continue;

                    w->updateWindowDecos();

                    // damage any workspace window that is on any monitor
                    if (!w->m_pinned)
                        g_pHyprRenderer->damageWindow(w);
                }
            } else if (PLAYER) {
                if (PLAYER->m_layer <= 1)
                    g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR);

                // some fucking layers miss 1 pixel???
                CBox expandBox = CBox{PLAYER->m_realPosition->value(), PLAYER->m_realSize->value()};
                expandBox.expand(5);
                g_pHyprRenderer->damageBox(expandBox);
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
    m_lastTickTimeMs                        = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    static auto PANIMENABLED = CConfigValue<Hyprlang::INT>("animations:enabled");

    for (size_t i = 0; i < m_vActiveAnimatedVariables.size(); i++) {
        const auto PAV = m_vActiveAnimatedVariables[i].lock();
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
    if (m_tickScheduled)
        return;

    m_tickScheduled = true;

    const auto PMOSTHZ = g_pHyprRenderer->m_mostHzMonitor;

    if (!PMOSTHZ) {
        m_animationTimer->updateTimeout(std::chrono::milliseconds(16));
        return;
    }

    float       refreshDelayMs = std::floor(1000.f / PMOSTHZ->m_refreshRate);

    const float SINCEPRES = std::chrono::duration_cast<std::chrono::microseconds>(Time::steadyNow() - PMOSTHZ->m_lastPresentationTimer.chrono()).count() / 1000.F;

    const auto  TOPRES = std::clamp(refreshDelayMs - SINCEPRES, 1.1f, 1000.f); // we can't send 0, that will disarm it

    m_animationTimer->updateTimeout(std::chrono::milliseconds((int)std::floor(TOPRES)));
}

void CHyprAnimationManager::onTicked() {
    m_tickScheduled = false;
}

//
// Anims
//
//

void CHyprAnimationManager::animationPopin(PHLWINDOW pWindow, bool close, float minPerc) {
    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    if (!close) {
        pWindow->m_realSize->setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_realPosition->setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_realSize->value() / 2.f);
    } else {
        *pWindow->m_realSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        *pWindow->m_realPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_realSize->goal() / 2.f;
    }
}

void CHyprAnimationManager::animationSlide(PHLWINDOW pWindow, std::string force, bool close) {
    pWindow->m_realSize->warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR)
        return; // unsafe state most likely

    Vector2D posOffset;

    if (!force.empty()) {
        if (force == "bottom")
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y + PMONITOR->m_size.y);
        else if (force == "left")
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
        else if (force == "right")
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y - GOALSIZE.y);

        if (!close)
            pWindow->m_realPosition->setValue(posOffset);
        else
            *pWindow->m_realPosition = posOffset;

        return;
    }

    const auto MIDPOINT = GOALPOS + GOALSIZE / 2.f;

    // check sides it touches
    const bool DISPLAYLEFT   = STICKS(pWindow->m_position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pWindow->m_position.x + pWindow->m_size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pWindow->m_position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pWindow->m_position.y + pWindow->m_size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

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
        if (MIDPOINT.y > PMONITOR->m_position.y + PMONITOR->m_size.y / 2.f)
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y + PMONITOR->m_size.y);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y - GOALSIZE.y);
    }

    if (!close)
        pWindow->m_realPosition->setValue(posOffset);
    else
        *pWindow->m_realPosition = posOffset;
}

void CHyprAnimationManager::animationGnomed(PHLWINDOW pWindow, bool close) {
    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    if (close) {
        *pWindow->m_realPosition = GOALPOS + Vector2D{0.F, GOALSIZE.y / 2.F};
        *pWindow->m_realSize     = Vector2D{GOALSIZE.x, 0.F};
    } else {
        pWindow->m_realPosition->setValueAndWarp(GOALPOS + Vector2D{0.F, GOALSIZE.y / 2.F});
        pWindow->m_realSize->setValueAndWarp(Vector2D{GOALSIZE.x, 0.F});
        *pWindow->m_realPosition = GOALPOS;
        *pWindow->m_realSize     = GOALSIZE;
    }
}

void CHyprAnimationManager::animationWobbly(PHLWINDOW pWindow, bool close, float intensity) {
    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();
    
    if (close) {
        // For closing: animate alpha from current to 0
        // The shader will handle the wobbly reveal effect based on alpha
        *pWindow->m_alpha = 0.f;
        
        // Keep position and size stable during close
        pWindow->m_realPosition->setValueAndWarp(GOALPOS);
        pWindow->m_realSize->setValueAndWarp(GOALSIZE);
    } else {
        // For opening: start with alpha 0, animate to 1
        pWindow->m_alpha->setValueAndWarp(0.f);
        pWindow->m_realPosition->setValueAndWarp(GOALPOS);
        pWindow->m_realSize->setValueAndWarp(GOALSIZE);
        
        *pWindow->m_alpha = 1.f;
    }
}

void CHyprAnimationManager::onWindowPostCreateClose(PHLWINDOW pWindow, bool close) {
    if (!close) {
        pWindow->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));
    } else {
        pWindow->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
    }

    std::string ANIMSTYLE = pWindow->m_realPosition->getStyle();
    std::ranges::transform(ANIMSTYLE, ANIMSTYLE.begin(), ::tolower);

    CVarList animList(ANIMSTYLE, 0, 's');

    // if the window is not being animated, that means the layout set a fixed size for it, don't animate.
    if (!pWindow->m_realPosition->isBeingAnimated() && !pWindow->m_realSize->isBeingAnimated())
        return;

    // if the animation is disabled and we are leaving, ignore the anim to prevent the snapshot being fucked
    if (!pWindow->m_realPosition->enabled())
        return;

    if (pWindow->m_windowData.animationStyle.hasValue()) {
        const auto STYLE = pWindow->m_windowData.animationStyle.value();
        // the window has config'd special anim
        if (STYLE.starts_with("slide")) {
            CVarList animList2(STYLE, 0, 's');
            animationSlide(pWindow, animList2[1], close);
        } else if (STYLE == "gnomed" || STYLE == "gnome") {
            animationGnomed(pWindow, close);
        } else if (STYLE.starts_with("wobbly")) {
            // Parse intensity parameter if provided
            float intensity = 1.0f;
            if (STYLE.find('%') != std::string::npos) {
                try {
                    auto percstr = STYLE.substr(STYLE.find_last_of(' ') + 1);
                    intensity = std::stof(percstr.substr(0, percstr.length() - 1)) / 100.f;
                    intensity = std::clamp(intensity, 0.1f, 2.0f); // Clamp between 10% and 200%
                } catch (std::exception& e) {
                    intensity = 1.0f; // fallback to default
                }
            }
            animationWobbly(pWindow, close, intensity);
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
        if (animList[0] == "slide") {
            animationSlide(pWindow, animList[1], close);
        } else if (animList[0] == "gnomed" || animList[0] == "gnome") {
            animationGnomed(pWindow, close);
        } else if (animList[0] == "wobbly") {
            // Parse intensity parameter if provided
            float intensity = 1.0f;
            if (animList.size() > 1) {
                try {
                    std::string intensityStr = animList[1];
                    if (intensityStr.ends_with("%")) {
                        intensityStr = intensityStr.substr(0, intensityStr.length() - 1);
                        intensity = std::stof(intensityStr) / 100.f;
                        intensity = std::clamp(intensity, 0.1f, 2.0f);
                    }
                } catch (std::exception& e) {
                    intensity = 1.0f; // fallback
                }
            }
            animationWobbly(pWindow, close, intensity);
        } else {
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
        if (style.starts_with("slide") || style == "gnome" || style == "gnomed") {
            return "";
        } else if (style.starts_with("wobbly")) {
            // Validate wobbly intensity parameter
            if (style.find('%') != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' ') + 1);
                    float intensity = std::stof(percstr.substr(0, percstr.length() - 1));
                    if (intensity < 10.0f || intensity > 200.0f) {
                        return "intensity must be between 10% and 200%";
                    }
                } catch (std::exception& e) { 
                    return "invalid intensity parameter"; 
                }
            }
            return "";
        } else if (style.starts_with("popin")) {
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
        if (style.empty() || style == "fade" || style == "slide")
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
    } else {
        return "animation has no styles";
    }

    return "";
}
