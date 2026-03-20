#include "AnimationManager.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../macros.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../eventLoop/EventLoopManager.hpp"
#include "../../helpers/varlist/VarList.hpp"
#include "../../render/Renderer.hpp"
#include "../../event/EventBus.hpp"

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

static SAnimationContext& getContext(Hyprutils::Animation::CBaseAnimatedVariable* pAV) {
    switch (pAV->m_Type) {
        case AVARTYPE_FLOAT: return dc<CAnimatedVariable<float>*>(pAV)->m_Context;
        case AVARTYPE_VECTOR: return dc<CAnimatedVariable<Vector2D>*>(pAV)->m_Context;
        case AVARTYPE_COLOR: return dc<CAnimatedVariable<CHyprColor>*>(pAV)->m_Context;
        default: UNREACHABLE();
    }
}

static void damageWindowForPolicies(PHLWINDOW pWindow, bool entire, bool border, bool shadow) {
    if (entire)
        g_pHyprRenderer->damageWindow(pWindow); // damageWindow already damages all decorations
    else {
        if (border) {
            const auto PDECO = pWindow->getDecorationByType(DECORATION_BORDER);
            PDECO->damageEntire();
        }
        if (shadow) {
            const auto PDECO = pWindow->getDecorationByType(DECORATION_SHADOW);
            PDECO->damageEntire();
        }
    }
}

static void preDamageWorkspace(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor) {
    // don't damage the whole monitor on workspace change, unless it's a special workspace, because dim/blur etc
    if (pWorkspace->m_isSpecialWorkspace)
        g_pHyprRenderer->damageMonitor(pMonitor);

    // TODO: just make this into a damn callback already vax...
    for (auto const& w : g_pCompositor->m_windows) {
        if (!w->m_isMapped || w->isHidden() || w->m_workspace != pWorkspace)
            continue;

        if (w->m_isFloating && !w->m_pinned) {
            // still doing the full damage hack for floating because sometimes when the window
            // goes through multiple monitors the last rendered frame is missing damage somehow??
            const CBox windowBoxNoOffset = w->getFullWindowBoundingBox();
            const CBox monitorBox        = {pMonitor->m_position, pMonitor->m_size};
            if (windowBoxNoOffset.intersection(monitorBox) != windowBoxNoOffset) // on edges between multiple monitors
                g_pHyprRenderer->damageWindow(w, true);
        }

        if (pWorkspace->m_isSpecialWorkspace)
            g_pHyprRenderer->damageWindow(w, true); // hack for special too because it can cross multiple monitors
    }

    // damage any workspace window that is on any monitor
    for (auto const& w : g_pCompositor->m_windows) {
        if (!validMapped(w) || w->m_workspace != pWorkspace || w->m_pinned)
            continue;

        g_pHyprRenderer->damageWindow(w);
    }
}

template <Animable VarType>
static void handleUpdate(CAnimatedVariable<VarType>& av, bool warp) {
    bool animationsDisabled = warp;

    if (auto w = av.m_Context.pWindow.lock()) {
        if (!w->m_monitor.lock())
            return;
        animationsDisabled = w->m_ruleApplicator->noAnim().valueOr(animationsDisabled);
    } else if (auto ws = av.m_Context.pWorkspace.lock()) {
        if (!ws->m_monitor.lock())
            return;
    } else if (auto ls = av.m_Context.pLayer.lock()) {
        if (!g_pCompositor->getMonitorFromVector(ls->m_realPosition->goal() + ls->m_realSize->goal() / 2.F))
            return;
        animationsDisabled = animationsDisabled || ls->m_ruleApplicator->noanim().valueOrDefault();
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
}

void CHyprAnimationManager::tick() {
    static std::chrono::time_point lastTick = std::chrono::high_resolution_clock::now();
    m_lastTickTimeMs                        = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    static auto PANIMENABLED = CConfigValue<Config::INTEGER>("animations:enabled");

    if (m_vActiveAnimatedVariables.empty()) {
        tickDone();
        return;
    }

    const auto CPY = m_vActiveAnimatedVariables;

    // batch damage per owner to avoid redundant damage calls, otherwise
    // we could be damaging many many times too much
    struct SDamageOwner {
        PHLWINDOW    window;
        PHLWORKSPACE workspace;
        PHLLS        layer;
        PHLMONITOR   monitor;
        bool         entire = false;
        bool         border = false;
        bool         shadow = false;
    };

    std::vector<SDamageOwner> owners;
    owners.reserve(4);

    // Collect per-owner damage policies
    for (const auto& PAV : CPY) {
        if (!PAV)
            continue;

        const auto&   ctx = getContext(PAV.get());

        SDamageOwner* owner = nullptr;

        if (auto w = ctx.pWindow.lock()) {
            for (auto& o : owners) {
                if (o.window == w) {
                    owner = &o;
                    break;
                }
            }
            if (!owner) {
                auto monitor = w->m_monitor.lock();
                if (!monitor)
                    continue;
                owners.emplace_back(SDamageOwner{.window = w, .monitor = monitor});
                owner = &owners.back();
            }
        } else if (auto ws = ctx.pWorkspace.lock()) {
            for (auto& o : owners) {
                if (o.workspace == ws) {
                    owner = &o;
                    break;
                }
            }
            if (!owner) {
                auto monitor = ws->m_monitor.lock();
                if (!monitor)
                    continue;
                owners.emplace_back(SDamageOwner{.workspace = ws, .monitor = monitor});
                owner = &owners.back();
            }
        } else if (auto ls = ctx.pLayer.lock()) {
            for (auto& o : owners) {
                if (o.layer == ls) {
                    owner = &o;
                    break;
                }
            }
            if (!owner) {
                auto monitor = g_pCompositor->getMonitorFromVector(ls->m_realPosition->goal() + ls->m_realSize->goal() / 2.F);
                if (!monitor)
                    continue;
                owners.emplace_back(SDamageOwner{.layer = ls, .monitor = monitor});
                owner = &owners.back();
            }
        } else
            continue;

        switch (ctx.eDamagePolicy) {
            case AVARDAMAGE_ENTIRE: owner->entire = true; break;
            case AVARDAMAGE_BORDER: owner->border = true; break;
            case AVARDAMAGE_SHADOW: owner->shadow = true; break;
            default: break;
        }
    }

    // pre-damage each owner once (old state)
    for (const auto& owner : owners) {
        if (owner.window)
            damageWindowForPolicies(owner.window, owner.entire, owner.border, owner.shadow);
        else if (owner.workspace)
            preDamageWorkspace(owner.workspace, owner.monitor);
        else if (owner.layer) {
            CBox expandBox = CBox{owner.layer->m_realPosition->value(), owner.layer->m_realSize->value()};
            expandBox.expand(5);
            g_pHyprRenderer->damageBox(expandBox);
        }
    }

    // update all variable values
    for (const auto& PAV : CPY) {
        if (!PAV)
            continue;

        const auto LOCK = PAV.lock();
        bool       warp = !*PANIMENABLED || !PAV->enabled();

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

    // post-damage each owner once (new state) + schedule frames
    for (const auto& owner : owners) {
        if (owner.window)
            damageWindowForPolicies(owner.window, owner.entire, owner.border, owner.shadow);
        else if (owner.workspace) {
            if (owner.entire) {
                for (auto const& w : g_pCompositor->m_windows) {
                    if (!validMapped(w) || w->m_workspace != owner.workspace || w->m_pinned)
                        continue;

                    g_pHyprRenderer->damageWindow(w);
                }
            }
        } else if (owner.layer) {
            if (owner.entire) {
                if (owner.layer->m_layer <= 1)
                    owner.monitor->m_blurFBDirty = true;

                CBox expandBox = CBox{owner.layer->m_realPosition->value(), owner.layer->m_realSize->value()};
                expandBox.expand(5);
                g_pHyprRenderer->damageBox(expandBox);
            }
        }

        if (!owner.monitor->inFullscreenMode())
            g_pCompositor->scheduleFrameForMonitor(owner.monitor, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
    }

    tickDone();
}

void CHyprAnimationManager::frameTick() {
    onTicked();

    if (!shouldTickForNext())
        return;

    if UNLIKELY (!g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState ||
                 !std::ranges::any_of(g_pCompositor->m_monitors, [](const auto& mon) { return mon->m_enabled && mon->m_output; }))
        return;

    if (!m_lastTickValid || m_lastTickTimer.getMillis() >= 1.0f) {
        m_lastTickTimer.reset();
        m_lastTickValid = true;

        tick();
        Event::bus()->m_events.tick.emit();
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

void CHyprAnimationManager::resetTickState() {
    m_lastTickValid = false;
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
