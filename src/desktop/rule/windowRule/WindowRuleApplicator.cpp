#include "WindowRuleApplicator.hpp"
#include "WindowRule.hpp"
#include "../Engine.hpp"
#include "../utils/SetUtils.hpp"
#include "../../view/Window.hpp"
#include "../../types/OverridableVar.hpp"
#include "../../../event/EventBus.hpp"

#include <tuple>

using namespace Desktop;
using namespace Desktop::Rule;

template <typename T, typename TEffect>
static void resetRuleProp(std::pair<Desktop::Types::COverridableVar<T>, std::underlying_type_t<Desktop::Rule::eRuleProperty>>& prop,
                          std::underlying_type_t<Desktop::Rule::eRuleProperty> props, Desktop::Types::eOverridePriority prio,
                          std::unordered_set<Desktop::Rule::CWindowRuleEffectContainer::storageType>& effectsNuked, TEffect&& effect) {
    auto& [value, propMask] = prop;

    if (!(propMask & props))
        return;

    if (prio == Desktop::Types::PRIORITY_WINDOW_RULE) {
        effectsNuked.emplace(effect());
        propMask &= ~props;
    }

    value.unset(prio);
}

CWindowRuleApplicator::CWindowRuleApplicator(PHLWINDOW w) : m_window(w) {
    ;
}

std::unordered_set<CWindowRuleEffectContainer::storageType> CWindowRuleApplicator::resetProps(std::underlying_type_t<eRuleProperty> props, Types::eOverridePriority prio) {
    std::unordered_set<CWindowRuleEffectContainer::storageType> effectsNuked;

    std::apply([&](auto&&... prop) { (resetRuleProp(prop.first.get(), props, prio, effectsNuked, prop.second), ...); },
               std::make_tuple(
                   std::pair{std::ref(m_alpha), [this] { return alphaEffect(); }}, std::pair{std::ref(m_alphaInactive), [this] { return alphaInactiveEffect(); }},
                   std::pair{std::ref(m_alphaFullscreen), [this] { return alphaFullscreenEffect(); }}, std::pair{std::ref(m_allowsInput), [this] { return allowsInputEffect(); }},
                   std::pair{std::ref(m_decorate), [this] { return decorateEffect(); }}, std::pair{std::ref(m_focusOnActivate), [this] { return focusOnActivateEffect(); }},
                   std::pair{std::ref(m_keepAspectRatio), [this] { return keepAspectRatioEffect(); }},
                   std::pair{std::ref(m_nearestNeighbor), [this] { return nearestNeighborEffect(); }}, std::pair{std::ref(m_noAnim), [this] { return noAnimEffect(); }},
                   std::pair{std::ref(m_noBlur), [this] { return noBlurEffect(); }}, std::pair{std::ref(m_noDim), [this] { return noDimEffect(); }},
                   std::pair{std::ref(m_noFocus), [this] { return noFocusEffect(); }}, std::pair{std::ref(m_noMaxSize), [this] { return noMaxSizeEffect(); }},
                   std::pair{std::ref(m_noShadow), [this] { return noShadowEffect(); }}, std::pair{std::ref(m_noShortcutsInhibit), [this] { return noShortcutsInhibitEffect(); }},
                   std::pair{std::ref(m_opaque), [this] { return opaqueEffect(); }}, std::pair{std::ref(m_dimAround), [this] { return dimAroundEffect(); }},
                   std::pair{std::ref(m_RGBX), [this] { return RGBXEffect(); }}, std::pair{std::ref(m_syncFullscreen), [this] { return syncFullscreenEffect(); }},
                   std::pair{std::ref(m_tearing), [this] { return tearingEffect(); }}, std::pair{std::ref(m_xray), [this] { return xrayEffect(); }},
                   std::pair{std::ref(m_renderUnfocused), [this] { return renderUnfocusedEffect(); }},
                   std::pair{std::ref(m_noFollowMouse), [this] { return noFollowMouseEffect(); }}, std::pair{std::ref(m_noScreenShare), [this] { return noScreenShareEffect(); }},
                   std::pair{std::ref(m_noVRR), [this] { return noVRREffect(); }}, std::pair{std::ref(m_persistentSize), [this] { return persistentSizeEffect(); }},
                   std::pair{std::ref(m_stayFocused), [this] { return stayFocusedEffect(); }}, std::pair{std::ref(m_idleInhibitMode), [this] { return idleInhibitModeEffect(); }},
                   std::pair{std::ref(m_confinePointer), [this] { return confinePointerEffect(); }}, std::pair{std::ref(m_borderSize), [this] { return borderSizeEffect(); }},
                   std::pair{std::ref(m_rounding), [this] { return roundingEffect(); }}, std::pair{std::ref(m_roundingPower), [this] { return roundingPowerEffect(); }},
                   std::pair{std::ref(m_scrollMouse), [this] { return scrollMouseEffect(); }}, std::pair{std::ref(m_scrollTouchpad), [this] { return scrollTouchpadEffect(); }},
                   std::pair{std::ref(m_animationStyle), [this] { return animationStyleEffect(); }}, std::pair{std::ref(m_maxSize), [this] { return maxSizeEffect(); }},
                   std::pair{std::ref(m_minSize), [this] { return minSizeEffect(); }}, std::pair{std::ref(m_activeBorderColor), [this] { return activeBorderColorEffect(); }},
                   std::pair{std::ref(m_inactiveBorderColor), [this] { return inactiveBorderColorEffect(); }}));

    if (prio == Types::PRIORITY_WINDOW_RULE) {
        std::erase_if(m_dynamicTags, [props, this](const auto& el) {
            const bool REMOVE = el.second & props;

            if (REMOVE)
                m_tagKeeper.removeDynamicTag(el.first);

            return REMOVE;
        });

        std::erase_if(m_otherProps.props, [props](const auto& el) { return !el.second || el.second->propMask & props; });
    }

    return effectsNuked;
}

CWindowRuleApplicator::SRuleResult CWindowRuleApplicator::applyDynamicRule(const SP<CWindowRule>& rule) {
    SRuleResult result;

    for (const auto& effectData : rule->effects()) {
        const auto  key    = effectData.key;
        const auto& effect = effectData.raw;
        const auto& value  = effectData.value;

        switch (key) {
            default: {
                if (key <= WINDOW_RULE_EFFECT_LAST_STATIC) {
                    Log::logger->log(Log::TRACE, "CWindowRuleApplicator::applyDynamicRule: Skipping effect {}, not dynamic", sc<std::underlying_type_t<eWindowRuleEffect>>(key));
                    break;
                }

                // custom type, add to our vec
                if (!m_otherProps.props.contains(key)) {
                    m_otherProps.props.emplace(key,
                                               makeUnique<SCustomPropContainer>(SCustomPropContainer{
                                                   .idx      = key,
                                                   .propMask = rule->getPropertiesMask(),
                                                   .effect   = effect,
                                               }));
                } else {
                    auto& e = m_otherProps.props[key];
                    e->propMask |= rule->getPropertiesMask();
                    e->effect = effect;
                }

                break;
            }

            case WINDOW_RULE_EFFECT_NONE: {
                Log::logger->log(Log::ERR, "CWindowRuleApplicator::applyDynamicRule: BUG THIS: WINDOW_RULE_EFFECT_NONE??");
                break;
            }
            case WINDOW_RULE_EFFECT_ROUNDING: {
                m_rounding.first.set(std::get<int64_t>(value), Types::PRIORITY_WINDOW_RULE);
                m_rounding.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_ROUNDING_POWER: {
                m_roundingPower.first.set(std::get<float>(value), Types::PRIORITY_WINDOW_RULE);
                m_roundingPower.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_PERSISTENT_SIZE: {
                m_persistentSize.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_persistentSize.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_ANIMATION: {
                m_animationStyle.first.set(std::get<std::string>(value), Types::PRIORITY_WINDOW_RULE);
                m_animationStyle.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_BORDER_COLOR: {
                const auto& borderColor   = std::get<SBorderColorRule>(value);
                m_activeBorderColor.first = Types::COverridableVar(borderColor.active, Types::PRIORITY_WINDOW_RULE);
                if (borderColor.inactive)
                    m_inactiveBorderColor.first = Types::COverridableVar(*borderColor.inactive, Types::PRIORITY_WINDOW_RULE);
                m_activeBorderColor.second   = rule->getPropertiesMask();
                m_inactiveBorderColor.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_IDLE_INHIBIT: {
                m_idleInhibitMode.first.set(std::get<int64_t>(value), Types::PRIORITY_WINDOW_RULE);
                m_idleInhibitMode.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_OPACITY: {
                const auto& opacity      = std::get<SOpacityRule>(value);
                m_alpha.first            = Types::COverridableVar(opacity.alpha, Types::PRIORITY_WINDOW_RULE);
                m_alphaInactive.first    = Types::COverridableVar(opacity.alphaInactive, Types::PRIORITY_WINDOW_RULE);
                m_alphaFullscreen.first  = Types::COverridableVar(opacity.alphaFullscreen, Types::PRIORITY_WINDOW_RULE);
                m_alpha.second           = rule->getPropertiesMask();
                m_alphaInactive.second   = rule->getPropertiesMask();
                m_alphaFullscreen.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_TAG: {
                m_dynamicTags.emplace_back(std::make_pair<>(effect, rule->getPropertiesMask()));
                m_tagKeeper.applyTag(effect, true);
                result.tagsChanged = true;
                break;
            }
            case WINDOW_RULE_EFFECT_MAX_SIZE: {
                try {
                    static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");

                    if (!m_window)
                        break;

                    const auto& expr = std::get<Math::SExpressionVec2>(value);
                    if (expr.empty())
                        break;

                    const auto VEC = m_window->calculateExpression(expr);
                    if (!VEC) {
                        Log::logger->log(Log::ERR, "failed to parse {} as an expression", expr.toString());
                        break;
                    }
                    if (VEC->x < 1 || VEC->y < 1) {
                        Log::logger->log(Log::ERR, "Invalid size for maxsize");
                        break;
                    }

                    m_maxSize.first = Types::COverridableVar(*VEC, Types::PRIORITY_WINDOW_RULE);

                    if (*PCLAMP_TILED || m_window->m_isFloating)
                        m_window->clampWindowSize(std::nullopt, m_maxSize.first.value());
                } catch (std::exception& e) { Log::logger->log(Log::ERR, "maxsize rule \"{}\" failed with: {}", std::get<Math::SExpressionVec2>(value).toString(), e.what()); }
                m_maxSize.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_MIN_SIZE: {
                try {
                    static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");

                    if (!m_window)
                        break;

                    const auto& expr = std::get<Math::SExpressionVec2>(value);
                    if (expr.empty())
                        break;

                    const auto VEC = m_window->calculateExpression(expr);
                    if (!VEC) {
                        Log::logger->log(Log::ERR, "failed to parse {} as an expression", expr.toString());
                        break;
                    }

                    if (VEC->x < 1 || VEC->y < 1) {
                        Log::logger->log(Log::ERR, "Invalid size for maxsize");
                        break;
                    }

                    m_minSize.first = Types::COverridableVar(*VEC, Types::PRIORITY_WINDOW_RULE);
                    if (*PCLAMP_TILED || m_window->m_isFloating)
                        m_window->clampWindowSize(m_minSize.first.value(), std::nullopt);
                } catch (std::exception& e) { Log::logger->log(Log::ERR, "minsize rule \"{}\" failed with: {}", std::get<Math::SExpressionVec2>(value).toString(), e.what()); }
                m_minSize.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_BORDER_SIZE: {
                auto oldBorderSize = m_borderSize.first.valueOrDefault();
                m_borderSize.first.set(std::get<int64_t>(value), Types::PRIORITY_WINDOW_RULE);
                m_borderSize.second |= rule->getPropertiesMask();
                if (oldBorderSize != m_borderSize.first.valueOrDefault())
                    result.needsRelayout = true;
                break;
            }
            case WINDOW_RULE_EFFECT_ALLOWS_INPUT: {
                m_allowsInput.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_allowsInput.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_DIM_AROUND: {
                m_dimAround.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_dimAround.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_DECORATE: {
                m_decorate.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_decorate.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE: {
                m_focusOnActivate.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_focusOnActivate.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO: {
                m_keepAspectRatio.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_keepAspectRatio.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR: {
                m_nearestNeighbor.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_nearestNeighbor.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_ANIM: {
                m_noAnim.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noAnim.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_BLUR: {
                m_noBlur.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noBlur.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_DIM: {
                m_noDim.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noDim.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_FOCUS: {
                m_noFocus.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noFocus.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE: {
                m_noFollowMouse.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noFollowMouse.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_MAX_SIZE: {
                m_noMaxSize.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noMaxSize.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SHADOW: {
                m_noShadow.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noShadow.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT: {
                m_noShortcutsInhibit.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noShortcutsInhibit.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_OPAQUE: {
                m_opaque.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_opaque.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_FORCE_RGBX: {
                m_RGBX.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_RGBX.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_SYNC_FULLSCREEN: {
                m_syncFullscreen.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_syncFullscreen.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_IMMEDIATE: {
                m_tearing.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_tearing.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_XRAY: {
                m_xray.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_xray.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_RENDER_UNFOCUSED: {
                m_renderUnfocused.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_renderUnfocused.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SCREEN_SHARE: {
                m_noScreenShare.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noScreenShare.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_VRR: {
                m_noVRR.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_noVRR.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_STAY_FOCUSED: {
                m_stayFocused.first.set(std::get<bool>(value), Types::PRIORITY_WINDOW_RULE);
                m_stayFocused.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_CONFINE_POINTER: {
                m_confinePointer.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_confinePointer.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_SCROLL_MOUSE: {
                m_scrollMouse.first.set(std::get<float>(value), Types::PRIORITY_WINDOW_RULE);
                m_scrollMouse.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD: {
                m_scrollTouchpad.first.set(std::get<float>(value), Types::PRIORITY_WINDOW_RULE);
                m_scrollTouchpad.second |= rule->getPropertiesMask();
                break;
            }
        }
    }
    return result;
}

CWindowRuleApplicator::SRuleResult CWindowRuleApplicator::applyStaticRule(const SP<CWindowRule>& rule) {
    for (const auto& effectData : rule->effects()) {
        const auto  key   = effectData.key;
        const auto& value = effectData.value;

        switch (key) {
            default: {
                Log::logger->log(Log::TRACE, "CWindowRuleApplicator::applyStaticRule: Skipping effect {}, not static", sc<std::underlying_type_t<eWindowRuleEffect>>(key));
                break;
            }

            case WINDOW_RULE_EFFECT_FLOAT: {
                static_.floating = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_TILE: {
                static_.floating = !std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_FULLSCREEN: {
                static_.fullscreen = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_MAXIMIZE: {
                static_.maximize = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_FULLSCREENSTATE: {
                const auto& fullscreenState     = std::get<SFullscreenStateRule>(value);
                static_.fullscreenStateInternal = fullscreenState.internal;
                if (fullscreenState.client)
                    static_.fullscreenStateClient = *fullscreenState.client;
                break;
            }
            case WINDOW_RULE_EFFECT_MOVE: {
                static_.center   = std::nullopt;
                const auto& expr = std::get<Math::SExpressionVec2>(value);
                if (expr.empty())
                    static_.position.reset();
                else
                    static_.position = expr;
                break;
            }
            case WINDOW_RULE_EFFECT_SIZE: {
                const auto& expr = std::get<Math::SExpressionVec2>(value);
                if (expr.empty())
                    static_.size.reset();
                else
                    static_.size = expr;
                break;
            }
            case WINDOW_RULE_EFFECT_CENTER: {
                static_.position.reset();
                static_.center = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_PSEUDO: {
                static_.pseudo = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_MONITOR: {
                static_.monitor = std::get<std::string>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_WORKSPACE: {
                static_.workspace = std::get<std::string>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_NOINITIALFOCUS: {
                static_.noInitialFocus = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_PIN: {
                static_.pin = std::get<bool>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_GROUP: {
                static_.group = std::get<std::string>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_SUPPRESSEVENT: {
                for (const auto& e : std::get<std::vector<std::string>>(value)) {
                    static_.suppressEvent.emplace_back(e);
                }
                break;
            }
            case WINDOW_RULE_EFFECT_CONTENT: {
                static_.content = std::get<int64_t>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_NOCLOSEFOR: {
                static_.noCloseFor = std::get<int64_t>(value);
                break;
            }
            case WINDOW_RULE_EFFECT_SCROLLING_WIDTH: {
                static_.scrollingWidth = std::get<float>(value);
                break;
            }
        }
    }

    return SRuleResult{};
}

//
bool CWindowRuleApplicator::readStaticRules(bool preRead) {
    if (!m_window)
        return false;

    static_ = {};

    m_execRules.clear();
    bool tagsWereChanged = false;

    for (const auto& r : ruleEngine()->rules()) {
        if (r->type() != RULE_TYPE_WINDOW)
            continue;

        auto wr = reinterpretPointerCast<CWindowRule>(r);

        if (!wr->matches(m_window.lock(), true))
            continue;

        if (wr->isExecRule()) {
            m_execRules.emplace_back(wr);
            continue;
        }

        applyStaticRule(wr);
        const auto RES  = applyDynamicRule(wr);
        tagsWereChanged = tagsWereChanged || RES.tagsChanged;
    }

    // set a recheck for some props people might wanna use for static rules.
    if (tagsWereChanged)
        propsToRecheck |= RULE_PROP_TAG;
    if (static_.content != NContentType::CONTENT_TYPE_NONE)
        propsToRecheck |= RULE_PROP_CONTENT;

    for (const auto& wr : m_execRules) {
        applyStaticRule(wr);
        applyDynamicRule(wr);
        if (!preRead)
            ruleEngine()->unregisterRule(wr);
    }
    return (propsToRecheck != RULE_PROP_NONE);
}

void CWindowRuleApplicator::recheckStaticRules() {
    for (const auto& r : ruleEngine()->rules()) {
        if (r->type() != RULE_TYPE_WINDOW)
            continue;

        if (!(r->getPropertiesMask() & propsToRecheck))
            continue;

        auto wr = reinterpretPointerCast<CWindowRule>(r);

        if (!wr->matches(m_window.lock(), true))
            continue;

        applyStaticRule(wr);
    }

    for (const auto& wr : m_execRules) {
        applyStaticRule(wr);
    }
}

void CWindowRuleApplicator::propertiesChanged(std::underlying_type_t<eRuleProperty> props) {
    if (!m_window || !m_window->m_isMapped || m_window->isHidden())
        return;

    bool                                                        needsRelayout         = false;
    std::unordered_set<CWindowRuleEffectContainer::storageType> effectsNeedingRecheck = resetProps(props);

    for (const auto& r : ruleEngine()->rules()) {
        if (r->type() != RULE_TYPE_WINDOW)
            continue;

        const auto WR = reinterpretPointerCast<CWindowRule>(r);

        if (!(WR->getPropertiesMask() & props) && !setsIntersect(WR->effectsSet(), effectsNeedingRecheck))
            continue;

        if (!WR->matches(m_window.lock()))
            continue;

        const auto RES = applyDynamicRule(WR);
        needsRelayout  = needsRelayout || RES.needsRelayout;
    }

    for (const auto& wr : m_execRules) {
        if (!(wr->getPropertiesMask() & props) && !setsIntersect(wr->effectsSet(), effectsNeedingRecheck))
            continue;

        const auto RES = applyDynamicRule(wr);
        needsRelayout  = needsRelayout || RES.needsRelayout;
    }

    m_window->updateWindowData();
    m_window->updateWindowDecos();
    m_window->updateDecorationValues();

    if (needsRelayout)
        g_pDecorationPositioner->forceRecalcFor(m_window.lock());

    // for plugins
    Event::bus()->m_events.window.updateRules.emit(m_window.lock());
}
