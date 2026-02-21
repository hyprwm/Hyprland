#include "AnimationManager.hpp"
#include "../../Compositor.hpp"
#include "../HookSystemManager.hpp"
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

#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/animation/AnimationManager.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace {
    constexpr float SPRING_SETTLE_EPSILON  = 0.001F; // 0.1%
    constexpr float SPRING_MAX_RUNTIME_SEC = 10.F;
    constexpr float SPRING_MAX_DELTA_SEC   = 0.1F;
    constexpr float SPRING_MODE_SWITCH_EPS = 0.0001F;
    constexpr float SPRING_DEFAULT_ZETA    = 0.82F;
    constexpr float SPRING_DEFAULT_OMEGA   = 14.F;
    constexpr float SPRING_SNAPPY_ZETA     = 0.95F;
    constexpr float SPRING_SNAPPY_OMEGA    = 18.F;
    constexpr float SPRING_BOUNCY_ZETA     = 0.55F;
    constexpr float SPRING_BOUNCY_OMEGA    = 12.F;

    enum class eSpringTimeMode : uint8_t {
        NORMALIZED,
        PHYSICS,
    };

    struct SSpringParams {
        float dampingRatio     = 0.82F;
        float angularFrequency = 14.F;
    };

    struct SSpringCurveConfig {
        eSpringTimeMode mode = eSpringTimeMode::NORMALIZED;
        SSpringParams   params;
    };

    bool parseSpringCurveParams(const std::string& payload, SSpringParams& out) {
        const auto colon = payload.find(':');
        if (colon == std::string::npos || colon == 0 || colon == payload.length() - 1 || payload.find(':', colon + 1) != std::string::npos)
            return false;

        const auto dampingStr = payload.substr(0, colon);
        const auto omegaStr   = payload.substr(colon + 1);

        try {
            const auto damping = std::stof(dampingStr);
            const auto omega   = std::stof(omegaStr);

            if (!std::isfinite(damping) || !std::isfinite(omega) || damping < 0.F || omega <= 0.F)
                return false;

            out = SSpringParams{.dampingRatio = damping, .angularFrequency = omega};
            return true;
        } catch (const std::exception&) { return false; }
    }

    std::optional<SSpringCurveConfig> parseSpringCurveNameUncached(const std::string& name) {
        if (name == "spring")
            return SSpringCurveConfig{.mode = eSpringTimeMode::NORMALIZED, .params = {.dampingRatio = SPRING_DEFAULT_ZETA, .angularFrequency = SPRING_DEFAULT_OMEGA}};

        if (name == "spring-snappy")
            return SSpringCurveConfig{.mode = eSpringTimeMode::NORMALIZED, .params = {.dampingRatio = SPRING_SNAPPY_ZETA, .angularFrequency = SPRING_SNAPPY_OMEGA}};

        if (name == "spring-bouncy")
            return SSpringCurveConfig{.mode = eSpringTimeMode::NORMALIZED, .params = {.dampingRatio = SPRING_BOUNCY_ZETA, .angularFrequency = SPRING_BOUNCY_OMEGA}};

        if (name == "spring-physics")
            return SSpringCurveConfig{.mode = eSpringTimeMode::PHYSICS, .params = {.dampingRatio = SPRING_DEFAULT_ZETA, .angularFrequency = SPRING_DEFAULT_OMEGA}};

        if (name == "spring-physics-snappy")
            return SSpringCurveConfig{.mode = eSpringTimeMode::PHYSICS, .params = {.dampingRatio = SPRING_SNAPPY_ZETA, .angularFrequency = SPRING_SNAPPY_OMEGA}};

        if (name == "spring-physics-bouncy")
            return SSpringCurveConfig{.mode = eSpringTimeMode::PHYSICS, .params = {.dampingRatio = SPRING_BOUNCY_ZETA, .angularFrequency = SPRING_BOUNCY_OMEGA}};

        if (name.starts_with("spring:")) {
            SSpringParams params;
            if (!parseSpringCurveParams(name.substr(std::string("spring:").length()), params))
                return std::nullopt;
            return SSpringCurveConfig{.mode = eSpringTimeMode::NORMALIZED, .params = params};
        }

        if (name.starts_with("spring-physics:")) {
            SSpringParams params;
            if (!parseSpringCurveParams(name.substr(std::string("spring-physics:").length()), params))
                return std::nullopt;
            return SSpringCurveConfig{.mode = eSpringTimeMode::PHYSICS, .params = params};
        }

        return std::nullopt;
    }

    std::optional<SSpringCurveConfig> getSpringCurveConfig(const std::string& name) {
        static std::unordered_map<std::string, std::optional<SSpringCurveConfig>> CACHE;

        const auto                                                                it = CACHE.find(name);
        if (it != CACHE.end())
            return it->second;

        auto parsed = parseSpringCurveNameUncached(name);
        CACHE.emplace(name, parsed);
        return parsed;
    }

    bool isSpringPhysicsCurve(const Hyprutils::Animation::CBaseAnimatedVariable& av) {
        const auto cfg = getSpringCurveConfig(av.getBezierName());
        return cfg.has_value() && cfg->mode == eSpringTimeMode::PHYSICS;
    }

    float estimateSpringSettleTime(const SSpringParams& params) {
        const float zeta = std::max(0.F, params.dampingRatio);
        const float wn   = std::max(0.001F, params.angularFrequency);

        float       sigma = wn;
        if (zeta < 1.F)
            sigma = std::max(zeta * wn, 0.F);
        else if (zeta > 1.F)
            sigma = std::max(wn * (zeta - std::sqrt(zeta * zeta - 1.F)), 0.F);

        if (sigma < 0.001F)
            return SPRING_MAX_RUNTIME_SEC;

        const float settle = std::log(1.F / SPRING_SETTLE_EPSILON) / sigma;
        return std::clamp(settle, 0.05F, SPRING_MAX_RUNTIME_SEC);
    }

    float solveSpringStepResponse(float t, const SSpringParams& params, bool clampToUnitTime) {
        t = clampToUnitTime ? std::clamp(t, 0.F, 1.F) : std::max(0.F, t);

        const float zeta = std::max(0.F, params.dampingRatio);
        const float wn   = std::max(0.001F, params.angularFrequency);

        if (zeta < 0.9999F) {
            const float disc  = std::sqrt(1.F - zeta * zeta);
            const float wd    = wn * disc;
            const float alpha = zeta / disc;
            const float expT  = std::exp(-zeta * wn * t);
            return 1.F - expT * (std::cos(wd * t) + alpha * std::sin(wd * t));
        }

        if (zeta <= 1.0001F) {
            const float expT = std::exp(-wn * t);
            return 1.F - expT * (1.F + wn * t);
        }

        const float sqrtTerm = std::sqrt(zeta * zeta - 1.F);
        const float r1       = -wn * (zeta - sqrtTerm);
        const float r2       = -wn * (zeta + sqrtTerm);

        // L'Hôpital limit as r1 -> r2 becomes the critically damped closed form.
        if (std::abs(r2 - r1) < 0.000001F)
            return 1.F - std::exp(r1 * t) * (1.F - r1 * t);

        return 1.F - ((r2 * std::exp(r1 * t) - r1 * std::exp(r2 * t)) / (r2 - r1));
    }
}

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

float CHyprAnimationManager::getCurveValueFor(const Hyprutils::Animation::CBaseAnimatedVariable& av) {
    if (!av.isBeingAnimated())
        return 1.F;

    const auto springCfg = getSpringCurveConfig(av.getBezierName());
    if (springCfg.has_value()) {
        if (springCfg->mode == eSpringTimeMode::NORMALIZED) {
            const auto spent = av.getPercent();
            if (spent >= 1.F)
                return 1.F;

            return solveSpringStepResponse(spent, springCfg->params, true);
        }

        const auto it      = m_springPhysicsElapsedSec.find(&av);
        const auto elapsed = it == m_springPhysicsElapsedSec.end() ? 0.F : it->second;
        return solveSpringStepResponse(elapsed, springCfg->params, false);
    }

    const auto spent = av.getPercent();
    if (spent >= 1.F)
        return 1.F;

    const auto PBEZIER = getBezier(av.getBezierName());
    if (!PBEZIER)
        return 1.F;

    return PBEZIER->getYForPoint(spent);
}

bool CHyprAnimationManager::shouldWarpAnimation(const Hyprutils::Animation::CBaseAnimatedVariable& av, bool animationsDisabled) {
    if (animationsDisabled || !av.isBeingAnimated())
        return true;

    const auto springCfg = getSpringCurveConfig(av.getBezierName());
    if (!springCfg.has_value() || springCfg->mode != eSpringTimeMode::PHYSICS)
        return av.getPercent() >= 1.F;

    const auto  elapsedIt = m_springPhysicsElapsedSec.find(&av);
    const float elapsed   = elapsedIt == m_springPhysicsElapsedSec.end() ? 0.F : elapsedIt->second;

    if (elapsed >= SPRING_MAX_RUNTIME_SEC)
        return true;

    const float settleAt = estimateSpringSettleTime(springCfg->params);
    if (elapsed < settleAt)
        return false;

    const float distance = std::abs(1.F - solveSpringStepResponse(elapsed, springCfg->params, false));
    return distance <= SPRING_SETTLE_EPSILON;
}

bool CHyprAnimationManager::isBezierNameValid(const std::string& name) {
    return bezierExists(name) || getSpringCurveConfig(name).has_value();
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

    const auto POINTY = g_pAnimationManager->getCurveValueFor(av);
    const bool WARP   = g_pAnimationManager->shouldWarpAnimation(av, animationsDisabled);

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
    if (PMONITOR && !PMONITOR->inFullscreenMode())
        g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_ANIMATION);
}

void CHyprAnimationManager::tick() {
    static std::chrono::time_point lastTick = std::chrono::high_resolution_clock::now();
    m_lastTickTimeMs                        = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    static auto PANIMENABLED = CConfigValue<Hyprlang::INT>("animations:enabled");

    if (!m_vActiveAnimatedVariables.empty()) {
        const auto                                                             CPY      = m_vActiveAnimatedVariables;
        const auto                                                             DELTASEC = std::clamp(m_lastTickTimeMs / 1000.F, 0.F, SPRING_MAX_DELTA_SEC);
        std::unordered_set<const Hyprutils::Animation::CBaseAnimatedVariable*> activeThisTick;

        for (const auto& PAV : CPY) {
            if (!PAV)
                continue;

            // lock this value while we are doing handleUpdate to avoid a UAF if an update callback destroys it
            const auto LOCK = PAV.lock();
            if (!LOCK)
                continue;

            auto* AVPTR = LOCK.get();
            activeThisTick.insert(AVPTR);

            if (isSpringPhysicsCurve(*LOCK)) {
                auto& elapsed = m_springPhysicsElapsedSec[AVPTR];
                if (LOCK->getPercent() <= SPRING_MODE_SWITCH_EPS)
                    elapsed = 0.F;
            } else {
                m_springPhysicsElapsedSec.erase(AVPTR);
            }

            // for disabled anims just warp
            bool warp = !*PANIMENABLED || !LOCK->enabled();

            switch (LOCK->m_Type) {
                case AVARTYPE_FLOAT: {
                    auto pTypedAV = dc<CAnimatedVariable<float>*>(LOCK.get());
                    RASSERT(pTypedAV, "Failed to upcast animated float");
                    handleUpdate(*pTypedAV, warp);
                } break;
                case AVARTYPE_VECTOR: {
                    auto pTypedAV = dc<CAnimatedVariable<Vector2D>*>(LOCK.get());
                    RASSERT(pTypedAV, "Failed to upcast animated Vector2D");
                    handleUpdate(*pTypedAV, warp);
                } break;
                case AVARTYPE_COLOR: {
                    auto pTypedAV = dc<CAnimatedVariable<CHyprColor>*>(LOCK.get());
                    RASSERT(pTypedAV, "Failed to upcast animated CHyprColor");
                    handleUpdate(*pTypedAV, warp);
                } break;
                default: UNREACHABLE();
            }

            const auto ELAPSEDIT = m_springPhysicsElapsedSec.find(AVPTR);
            if (ELAPSEDIT != m_springPhysicsElapsedSec.end()) {
                if (LOCK->isBeingAnimated())
                    ELAPSEDIT->second += DELTASEC;
                else
                    m_springPhysicsElapsedSec.erase(ELAPSEDIT);
            }
        }

        std::erase_if(m_springPhysicsElapsedSec, [&activeThisTick](const auto& pair) { return !activeThisTick.contains(pair.first); });
    }

    tickDone();
}

void CHyprAnimationManager::frameTick() {
    onTicked();

    if (!shouldTickForNext())
        return;

    if UNLIKELY (!g_pCompositor->m_sessionActive || !g_pHookSystem || g_pCompositor->m_unsafeState ||
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

void CHyprAnimationManager::resetTickState() {
    m_lastTickValid = false;
    m_tickScheduled = false;
    m_springPhysicsElapsedSec.clear();
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
