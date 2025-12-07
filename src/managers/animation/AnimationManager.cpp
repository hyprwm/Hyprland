#include "AnimationManager.hpp"
#include "../../Compositor.hpp"
#include "../HookSystemManager.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../macros.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/Window.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../eventLoop/EventLoopManager.hpp"
#include "../../helpers/varlist/VarList.hpp"
#include "../../render/Renderer.hpp"

#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/animation/AnimationManager.hpp>

static int wlTick(SP<CEventLoopTimer> self, void* data) {
    if (g_pAnimationManager)
        g_pAnimationManager->frameTick();

    return 0;
}

CHyprAnimationManager::CHyprAnimationManager() {
    m_animationTimer = makeShared<CEventLoopTimer>(std::chrono::microseconds(500), wlTick, nullptr);
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

        animationsDisabled = PWINDOW->m_ruleApplicator->noAnim().valueOr(animationsDisabled);
    } else if (PWORKSPACE) {
        PMONITOR = PWORKSPACE->m_monitor.lock();
        if (!PMONITOR)
            return;

        // don't damage the whole monitor on workspace change, unless it's a special workspace, because dim/blur etc
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
        animationsDisabled = animationsDisabled || PLAYER->m_ruleApplicator->noanim().valueOrDefault();
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
                auto pTypedAV = dc<CAnimatedVariable<float>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated float");
                handleUpdate(*pTypedAV, warp);
            } break;
            case AVARTYPE_VECTOR: {
                auto pTypedAV = dc<CAnimatedVariable<Vector2D>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated Vector2D");
                handleUpdate(*pTypedAV, warp);
            } break;
            case AVARTYPE_COLOR: {
                auto pTypedAV = dc<CAnimatedVariable<CHyprColor>*>(PAV.get());
                RASSERT(pTypedAV, "Failed to upcast animated CHyprColor");
                handleUpdate(*pTypedAV, warp);
            } break;
            default: UNREACHABLE();
        }
    }

    tickDone();
}

void CHyprAnimationManager::frameTick() {
    onTicked();

    if (!shouldTickForNext())
        return;

    if (!g_pCompositor->m_sessionActive || !g_pHookSystem || g_pCompositor->m_unsafeState ||
        !std::ranges::any_of(g_pCompositor->m_monitors, [](const auto& mon) { return mon->m_enabled && mon->m_output; }))
        return;

    if (!m_lastTickValid || m_lastTickTimer.getMillis() >= 1.0f) {
        m_lastTickTimer.reset();
        m_lastTickValid = true;

        tick();
        EMIT_HOOK_EVENT("tick", nullptr);
    }

    if (shouldTickForNext())
        scheduleTick();
}

void CHyprAnimationManager::scheduleTick() {
    if (m_tickScheduled)
        return;

    m_tickScheduled = true;

    if (!m_animationTimer || !g_pEventLoopManager) {
        m_tickScheduled = false;
        return;
    }

    m_animationTimer->updateTimeout(std::chrono::milliseconds(1));
}

void CHyprAnimationManager::onTicked() {
    m_tickScheduled = false;
}

std::string CHyprAnimationManager::styleValidInConfigVar(const std::string& config, const std::string& style) {
    if (config.starts_with("window")) {
        if (style.starts_with("slide") || style == "gnome" || style == "gnomed")
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
        else if (style.starts_with("slide")) {
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
        return "unknown style";
    } else {
        return "animation has no styles";
    }

    return "";
}
