#include "WindowRuleApplicator.hpp"
#include "WindowRule.hpp"
#include "../Engine.hpp"
#include "../utils/SetUtils.hpp"
#include "../../Window.hpp"
#include "../../types/OverridableVar.hpp"
#include "../../../managers/LayoutManager.hpp"
#include "../../../managers/HookSystemManager.hpp"

#include <hyprutils/string/String.hpp>

using namespace Hyprutils::String;

using namespace Desktop;
using namespace Desktop::Rule;

CWindowRuleApplicator::CWindowRuleApplicator(PHLWINDOW w) : m_window(w) {
    ;
}

std::unordered_set<CWindowRuleEffectContainer::storageType> CWindowRuleApplicator::resetProps(std::underlying_type_t<eRuleProperty> props, Types::eOverridePriority prio) {
    // TODO: fucking kill me, is there a better way to do this?

    std::unordered_set<CWindowRuleEffectContainer::storageType> effectsNuked;

#define UNSET(x)                                                                                                                                                                   \
    if (m_##x.second & props) {                                                                                                                                                    \
        if (prio == Types::PRIORITY_WINDOW_RULE) {                                                                                                                                 \
            effectsNuked.emplace(x##Effect());                                                                                                                                     \
            m_##x.second &= ~props;                                                                                                                                                \
        }                                                                                                                                                                          \
        m_##x.first.unset(prio);                                                                                                                                                   \
    }

    UNSET(alpha)
    UNSET(alphaInactive)
    UNSET(alphaFullscreen)
    UNSET(allowsInput)
    UNSET(decorate)
    UNSET(focusOnActivate)
    UNSET(keepAspectRatio)
    UNSET(nearestNeighbor)
    UNSET(noAnim)
    UNSET(noBlur)
    UNSET(noDim)
    UNSET(noFocus)
    UNSET(noMaxSize)
    UNSET(noShadow)
    UNSET(noShortcutsInhibit)
    UNSET(opaque)
    UNSET(dimAround)
    UNSET(RGBX)
    UNSET(syncFullscreen)
    UNSET(tearing)
    UNSET(xray)
    UNSET(renderUnfocused)
    UNSET(noFollowMouse)
    UNSET(noScreenShare)
    UNSET(noVRR)
    UNSET(persistentSize)
    UNSET(stayFocused)
    UNSET(idleInhibitMode)
    UNSET(borderSize)
    UNSET(rounding)
    UNSET(roundingPower)
    UNSET(scrollMouse)
    UNSET(scrollTouchpad)
    UNSET(animationStyle)
    UNSET(maxSize)
    UNSET(minSize)
    UNSET(activeBorderColor)
    UNSET(inactiveBorderColor)

#undef UNSET

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

CWindowRuleApplicator::SRuleResult CWindowRuleApplicator::applyDynamicRule(const SP<CWindowRule>& rule, Types::eOverridePriority prio) {
    SRuleResult result;

    for (const auto& [key, effect] : rule->effects()) {
        switch (key) {
            default: {
                if (key <= WINDOW_RULE_EFFECT_LAST_STATIC) {
                    Debug::log(TRACE, "CWindowRuleApplicator::applyDynamicRule: Skipping effect {}, not dynamic", sc<std::underlying_type_t<eWindowRuleEffect>>(key));
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
                Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: BUG THIS: WINDOW_RULE_EFFECT_NONE??");
                break;
            }
            case WINDOW_RULE_EFFECT_ROUNDING: {
                try {
                    Debug::log(LOG, "applying rounding rule? effect: {}", effect);
                    m_rounding.first.set(std::stoull(effect), prio);
                    m_rounding.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: invalid rounding {}", effect); }
                break;
            }
            case WINDOW_RULE_EFFECT_ROUNDING_POWER: {
                try {
                    m_roundingPower.first.set(std::clamp(std::stof(effect), 1.F, 10.F), prio);
                    m_roundingPower.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: invalid rounding_power {}", effect); }
                break;
            }
            case WINDOW_RULE_EFFECT_PERSISTENT_SIZE: {
                m_persistentSize.first.set(truthy(effect), prio);
                m_persistentSize.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_ANIMATION: {
                m_animationStyle.first.set(effect, prio);
                m_animationStyle.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_BORDER_COLOR: {
                try {
                    // Each vector will only get used if it has at least one color
                    CGradientValueData activeBorderGradient   = {};
                    CGradientValueData inactiveBorderGradient = {};
                    bool               active                 = true;
                    CVarList           colorsAndAngles        = CVarList(trim(effect.substr(effect.find_first_of(' ') + 1)), 0, 's', true);

                    // Basic form has only two colors, everything else can be parsed as a gradient
                    if (colorsAndAngles.size() == 2 && !colorsAndAngles[1].contains("deg")) {
                        m_activeBorderColor.first   = Types::COverridableVar(CGradientValueData(CHyprColor(configStringToInt(colorsAndAngles[0]).value_or(0))), prio);
                        m_inactiveBorderColor.first = Types::COverridableVar(CGradientValueData(CHyprColor(configStringToInt(colorsAndAngles[1]).value_or(0))), prio);
                        break;
                    }

                    for (auto const& token : colorsAndAngles) {
                        // The first angle, or an explicit "0deg", splits the two gradients
                        if (active && token.contains("deg")) {
                            activeBorderGradient.m_angle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                            active                       = false;
                        } else if (token.contains("deg"))
                            inactiveBorderGradient.m_angle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                        else if (active)
                            activeBorderGradient.m_colors.emplace_back(configStringToInt(token).value_or(0));
                        else
                            inactiveBorderGradient.m_colors.emplace_back(configStringToInt(token).value_or(0));
                    }

                    activeBorderGradient.updateColorsOk();

                    // Includes sanity checks for the number of colors in each gradient
                    if (activeBorderGradient.m_colors.size() > 10 || inactiveBorderGradient.m_colors.size() > 10)
                        Debug::log(WARN, "Bordercolor rule \"{}\" has more than 10 colors in one gradient, ignoring", effect);
                    else if (activeBorderGradient.m_colors.empty())
                        Debug::log(WARN, "Bordercolor rule \"{}\" has no colors, ignoring", effect);
                    else if (inactiveBorderGradient.m_colors.empty())
                        m_activeBorderColor.first = Types::COverridableVar(activeBorderGradient, prio);
                    else {
                        m_activeBorderColor.first   = Types::COverridableVar(activeBorderGradient, prio);
                        m_inactiveBorderColor.first = Types::COverridableVar(inactiveBorderGradient, prio);
                    }
                } catch (std::exception& e) { Debug::log(ERR, "BorderColor rule \"{}\" failed with: {}", effect, e.what()); }
                m_activeBorderColor.second   = rule->getPropertiesMask();
                m_inactiveBorderColor.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_IDLE_INHIBIT: {
                if (effect == "none")
                    m_idleInhibitMode.first.set(IDLEINHIBIT_NONE, prio);
                else if (effect == "always")
                    m_idleInhibitMode.first.set(IDLEINHIBIT_ALWAYS, prio);
                else if (effect == "focus")
                    m_idleInhibitMode.first.set(IDLEINHIBIT_FOCUS, prio);
                else if (effect == "fullscreen")
                    m_idleInhibitMode.first.set(IDLEINHIBIT_FULLSCREEN, prio);
                else
                    Debug::log(ERR, "Rule idleinhibit: unknown mode {}", effect);
                m_idleInhibitMode.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_OPACITY: {
                try {
                    CVarList2 vars(std::string{effect}, 0, ' ');

                    int       opacityIDX = 0;

                    for (const auto& r : vars) {
                        if (r == "opacity")
                            continue;

                        if (r == "override") {
                            if (opacityIDX == 1)
                                m_alpha.first = Types::COverridableVar(Types::SAlphaValue{.alpha = m_alpha.first.value().alpha, .overridden = true}, prio);
                            else if (opacityIDX == 2)
                                m_alphaInactive.first = Types::COverridableVar(Types::SAlphaValue{.alpha = m_alphaInactive.first.value().alpha, .overridden = true}, prio);
                            else if (opacityIDX == 3)
                                m_alphaFullscreen.first = Types::COverridableVar(Types::SAlphaValue{.alpha = m_alphaFullscreen.first.value().alpha, .overridden = true}, prio);
                        } else {
                            if (opacityIDX == 0)
                                m_alpha.first = Types::COverridableVar(Types::SAlphaValue{.alpha = std::stof(std::string{r}), .overridden = false}, prio);
                            else if (opacityIDX == 1)
                                m_alphaInactive.first = Types::COverridableVar(Types::SAlphaValue{.alpha = std::stof(std::string{r}), .overridden = false}, prio);
                            else if (opacityIDX == 2)
                                m_alphaFullscreen.first = Types::COverridableVar(Types::SAlphaValue{.alpha = std::stof(std::string{r}), .overridden = false}, prio);
                            else
                                throw std::runtime_error("more than 3 alpha values");

                            opacityIDX++;
                        }
                    }

                    if (opacityIDX == 1) {
                        m_alphaInactive.first   = m_alpha.first;
                        m_alphaFullscreen.first = m_alpha.first;
                    }
                } catch (std::exception& e) { Debug::log(ERR, "Opacity rule \"{}\" failed with: {}", effect, e.what()); }
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
                    static auto PCLAMP_TILED = CConfigValue<Hyprlang::INT>("misc:size_limits_tiled");

                    if (!m_window)
                        break;

                    if (!m_window->m_isFloating && !sc<bool>(*PCLAMP_TILED))
                        break;

                    const auto VEC = configStringToVector2D(effect);
                    if (VEC.x < 1 || VEC.y < 1) {
                        Debug::log(ERR, "Invalid size for maxsize");
                        break;
                    }

                    m_maxSize.first = Types::COverridableVar(VEC, prio);
                    m_window->clampWindowSize(std::nullopt, m_maxSize.first.value());

                } catch (std::exception& e) { Debug::log(ERR, "maxsize rule \"{}\" failed with: {}", effect, e.what()); }
                m_maxSize.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_MIN_SIZE: {
                try {
                    static auto PCLAMP_TILED = CConfigValue<Hyprlang::INT>("misc:size_limits_tiled");

                    if (!m_window)
                        break;

                    if (!m_window->m_isFloating && !sc<bool>(*PCLAMP_TILED))
                        break;

                    const auto VEC = configStringToVector2D(effect);
                    if (VEC.x < 1 || VEC.y < 1) {
                        Debug::log(ERR, "Invalid size for maxsize");
                        break;
                    }

                    m_minSize.first = Types::COverridableVar(VEC, prio);
                    m_window->clampWindowSize(std::nullopt, m_minSize.first.value());
                } catch (std::exception& e) { Debug::log(ERR, "minsize rule \"{}\" failed with: {}", effect, e.what()); }
                m_minSize.second = rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_BORDER_SIZE: {
                try {
                    auto oldBorderSize = m_borderSize.first.valueOrDefault();
                    m_borderSize.first.set(std::stoi(effect), prio);
                    m_borderSize.second |= rule->getPropertiesMask();
                    if (oldBorderSize != m_borderSize.first.valueOrDefault())
                        result.needsRelayout = true;
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: invalid border_size {}", effect); }
                break;
            }
            case WINDOW_RULE_EFFECT_ALLOWS_INPUT: {
                m_allowsInput.first.set(truthy(effect), prio);
                m_allowsInput.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_DIM_AROUND: {
                m_dimAround.first.set(truthy(effect), prio);
                m_dimAround.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_DECORATE: {
                m_decorate.first.set(truthy(effect), prio);
                m_decorate.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE: {
                m_focusOnActivate.first.set(truthy(effect), prio);
                m_focusOnActivate.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO: {
                m_keepAspectRatio.first.set(truthy(effect), prio);
                m_keepAspectRatio.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR: {
                m_nearestNeighbor.first.set(truthy(effect), prio);
                m_nearestNeighbor.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_ANIM: {
                m_noAnim.first.set(truthy(effect), prio);
                m_noAnim.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_BLUR: {
                m_noBlur.first.set(truthy(effect), prio);
                m_noBlur.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_DIM: {
                m_noDim.first.set(truthy(effect), prio);
                m_noDim.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_FOCUS: {
                m_noFocus.first.set(truthy(effect), prio);
                m_noFocus.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE: {
                m_noFollowMouse.first.set(truthy(effect), prio);
                m_noFollowMouse.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_MAX_SIZE: {
                m_noMaxSize.first.set(truthy(effect), prio);
                m_noMaxSize.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SHADOW: {
                m_noShadow.first.set(truthy(effect), prio);
                m_noShadow.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT: {
                m_noShortcutsInhibit.first.set(truthy(effect), prio);
                m_noShortcutsInhibit.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_OPAQUE: {
                m_opaque.first.set(truthy(effect), prio);
                m_opaque.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_FORCE_RGBX: {
                m_RGBX.first.set(truthy(effect), prio);
                m_RGBX.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_SYNC_FULLSCREEN: {
                m_syncFullscreen.first.set(truthy(effect), prio);
                m_syncFullscreen.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_IMMEDIATE: {
                m_tearing.first.set(truthy(effect), prio);
                m_tearing.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_XRAY: {
                m_xray.first.set(truthy(effect), prio);
                m_xray.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_RENDER_UNFOCUSED: {
                m_renderUnfocused.first.set(truthy(effect), prio);
                m_renderUnfocused.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_SCREEN_SHARE: {
                m_noScreenShare.first.set(truthy(effect), prio);
                m_noScreenShare.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_NO_VRR: {
                m_noVRR.first.set(truthy(effect), prio);
                m_noVRR.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_STAY_FOCUSED: {
                m_stayFocused.first.set(truthy(effect), prio);
                m_stayFocused.second |= rule->getPropertiesMask();
                break;
            }
            case WINDOW_RULE_EFFECT_SCROLL_MOUSE: {
                try {
                    m_scrollMouse.first.set(std::clamp(std::stof(effect), 0.01F, 10.F), prio);
                    m_scrollMouse.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: invalid scroll_mouse {}", effect); }
                break;
            }
            case WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD: {
                try {
                    m_scrollTouchpad.first.set(std::clamp(std::stof(effect), 0.01F, 10.F), prio);
                    m_scrollTouchpad.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyDynamicRule: invalid scroll_touchpad {}", effect); }
                break;
            }
        }
    }
    return result;
}

CWindowRuleApplicator::SRuleResult CWindowRuleApplicator::applyStaticRule(const SP<CWindowRule>& rule) {
    for (const auto& [key, effect] : rule->effects()) {

        switch (key) {
            default: {
                Debug::log(TRACE, "CWindowRuleApplicator::applyStaticRule: Skipping effect {}, not static", sc<std::underlying_type_t<eWindowRuleEffect>>(key));
                break;
            }

            case WINDOW_RULE_EFFECT_FLOAT: {
                static_.floating = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_TILE: {
                static_.floating = !truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_FULLSCREEN: {
                static_.fullscreen = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_MAXIMIZE: {
                static_.maximize = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_FULLSCREENSTATE: {
                CVarList2 vars(std::string{effect}, 0, 's');
                try {
                    static_.fullscreenStateInternal = std::stoi(std::string{vars[0]});
                    if (!vars[1].empty())
                        static_.fullscreenStateClient = std::stoi(std::string{vars[1]});
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyStaticRule: invalid fullscreen state {}", effect); }
                break;
            }
            case WINDOW_RULE_EFFECT_MOVE: {
                static_.position = effect;
                break;
            }
            case WINDOW_RULE_EFFECT_SIZE: {
                static_.size = effect;
                break;
            }
            case WINDOW_RULE_EFFECT_CENTER: {
                static_.center = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_PSEUDO: {
                static_.pseudo = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_MONITOR: {
                static_.monitor = effect;
                break;
            }
            case WINDOW_RULE_EFFECT_WORKSPACE: {
                static_.workspace = effect;
                break;
            }
            case WINDOW_RULE_EFFECT_NOINITIALFOCUS: {
                static_.noInitialFocus = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_PIN: {
                static_.pin = truthy(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_GROUP: {
                static_.group = effect;
                break;
            }
            case WINDOW_RULE_EFFECT_SUPPRESSEVENT: {
                CVarList2 varlist(std::string{effect}, 0, 's');
                for (const auto& e : varlist) {
                    static_.suppressEvent.emplace_back(e);
                }
                break;
            }
            case WINDOW_RULE_EFFECT_CONTENT: {
                static_.content = NContentType::fromString(effect);
                break;
            }
            case WINDOW_RULE_EFFECT_NOCLOSEFOR: {
                try {
                    static_.noCloseFor = std::stoi(effect);
                } catch (...) { Debug::log(ERR, "CWindowRuleApplicator::applyStaticRule: invalid no close for {}", effect); }
                break;
            }
        }
    }

    return SRuleResult{};
}

void CWindowRuleApplicator::readStaticRules() {
    if (!m_window)
        return;

    static_ = {};

    std::vector<SP<IRule>> toRemove;
    bool                   tagsWereChanged = false;

    for (const auto& r : ruleEngine()->rules()) {
        if (r->type() != RULE_TYPE_WINDOW)
            continue;

        auto wr = reinterpretPointerCast<CWindowRule>(r);

        if (!wr->matches(m_window.lock(), true))
            continue;

        applyStaticRule(wr);

        // Also apply dynamic, because we won't recheck it before layout gets data
        // If it is an exec rule, apply it as if it was a SET_PROP command to override existing window rules
        const auto RES  = applyDynamicRule(wr, wr->isExecRule() ? Types::PRIORITY_SET_PROP : Types::PRIORITY_WINDOW_RULE);
        tagsWereChanged = tagsWereChanged || RES.tagsChanged;

        if (wr->isExecRule())
            toRemove.emplace_back(wr);
    }

    for (const auto& wr : toRemove) {
        ruleEngine()->unregisterRule(wr);
    }

    // recheck some props people might wanna use for static rules.
    std::underlying_type_t<eRuleProperty> propsToRecheck = RULE_PROP_NONE;
    if (tagsWereChanged)
        propsToRecheck |= RULE_PROP_TAG;
    if (static_.content != NContentType::CONTENT_TYPE_NONE)
        propsToRecheck |= RULE_PROP_CONTENT;

    if (propsToRecheck != RULE_PROP_NONE) {
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

        const auto RES = applyDynamicRule(WR, Types::PRIORITY_WINDOW_RULE);
        needsRelayout  = needsRelayout || RES.needsRelayout;
    }

    m_window->updateDecorationValues();

    if (needsRelayout)
        g_pDecorationPositioner->forceRecalcFor(m_window.lock());

    // for plugins
    EMIT_HOOK_EVENT("windowUpdateRules", m_window.lock());
}
