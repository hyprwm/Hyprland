#pragma once

#include <unordered_map>
#include <unordered_set>

#include "WindowRuleEffectContainer.hpp"
#include "../../DesktopTypes.hpp"
#include "../Rule.hpp"
#include "../../types/OverridableVar.hpp"
#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/TagKeeper.hpp"
#include "../../../config/ConfigDataValues.hpp"

namespace Desktop::Rule {
    class CWindowRule;

    enum eIdleInhibitMode : uint8_t {
        IDLEINHIBIT_NONE = 0,
        IDLEINHIBIT_ALWAYS,
        IDLEINHIBIT_FULLSCREEN,
        IDLEINHIBIT_FOCUS
    };

    class CWindowRuleApplicator {
      public:
        CWindowRuleApplicator(PHLWINDOW w);
        ~CWindowRuleApplicator() = default;

        CWindowRuleApplicator(const CWindowRuleApplicator&) = delete;
        CWindowRuleApplicator(CWindowRuleApplicator&)       = delete;
        CWindowRuleApplicator(CWindowRuleApplicator&&)      = delete;

        void                                                        propertiesChanged(std::underlying_type_t<eRuleProperty> props);
        std::unordered_set<CWindowRuleEffectContainer::storageType> resetProps(std::underlying_type_t<eRuleProperty> props,
                                                                               Types::eOverridePriority              prio = Types::PRIORITY_WINDOW_RULE);
        void                                                        readStaticRules();
        void                                                        applyStaticRules();

        // static props
        struct {
            std::string              monitor, workspace, group;

            std::optional<bool>      floating;

            bool                     fullscreen     = false;
            bool                     maximize       = false;
            bool                     pseudo         = false;
            bool                     pin            = false;
            bool                     noInitialFocus = false;

            std::optional<int>       fullscreenStateClient;
            std::optional<int>       fullscreenStateInternal;
            std::optional<int>       center;
            std::optional<int>       content;
            std::optional<int>       noCloseFor;

            std::string              size, position;

            std::vector<std::string> suppressEvent;
        } static_;

        struct SCustomPropContainer {
            CWindowRuleEffectContainer::storageType idx      = WINDOW_RULE_EFFECT_NONE;
            std::underlying_type_t<eRuleProperty>   propMask = RULE_PROP_NONE;
            std::string                             effect;
        };

        // This struct holds props that were dynamically registered. Plugins may read this.
        struct {
            std::unordered_map<CWindowRuleEffectContainer::storageType, UP<SCustomPropContainer>> props;
        } m_otherProps;

#define COMMA ,
#define DEFINE_PROP(type, name, def, eff)                                                                                                                                          \
  private:                                                                                                                                                                         \
    std::pair<Types::COverridableVar<type>, std::underlying_type_t<eRuleProperty>> m_##name = {def, RULE_PROP_NONE};                                                               \
                                                                                                                                                                                   \
  public:                                                                                                                                                                          \
    Types::COverridableVar<type>& name() {                                                                                                                                         \
        return m_##name.first;                                                                                                                                                     \
    }                                                                                                                                                                              \
    void name##Override(const Types::COverridableVar<type>& other) {                                                                                                               \
        m_##name.first = other;                                                                                                                                                    \
    }                                                                                                                                                                              \
    eWindowRuleEffect name##Effect() {                                                                                                                                             \
        return eff;                                                                                                                                                                \
    }

        // dynamic props
        DEFINE_PROP(Types::SAlphaValue, alpha, Types::SAlphaValue{}, WINDOW_RULE_EFFECT_OPACITY)
        DEFINE_PROP(Types::SAlphaValue, alphaInactive, Types::SAlphaValue{}, WINDOW_RULE_EFFECT_OPACITY)
        DEFINE_PROP(Types::SAlphaValue, alphaFullscreen, Types::SAlphaValue{}, WINDOW_RULE_EFFECT_OPACITY)

        DEFINE_PROP(bool, allowsInput, false, WINDOW_RULE_EFFECT_ALLOWS_INPUT)
        DEFINE_PROP(bool, decorate, true, WINDOW_RULE_EFFECT_DECORATE)
        DEFINE_PROP(bool, focusOnActivate, false, WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE)
        DEFINE_PROP(bool, keepAspectRatio, false, WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO)
        DEFINE_PROP(bool, nearestNeighbor, false, WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR)
        DEFINE_PROP(bool, noAnim, false, WINDOW_RULE_EFFECT_NO_ANIM)
        DEFINE_PROP(bool, noBlur, false, WINDOW_RULE_EFFECT_NO_BLUR)
        DEFINE_PROP(bool, noDim, false, WINDOW_RULE_EFFECT_NO_DIM)
        DEFINE_PROP(bool, noFocus, false, WINDOW_RULE_EFFECT_NO_FOCUS)
        DEFINE_PROP(bool, noMaxSize, false, WINDOW_RULE_EFFECT_NO_MAX_SIZE)
        DEFINE_PROP(bool, noShadow, false, WINDOW_RULE_EFFECT_NO_SHADOW)
        DEFINE_PROP(bool, noShortcutsInhibit, false, WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT)
        DEFINE_PROP(bool, opaque, false, WINDOW_RULE_EFFECT_OPAQUE)
        DEFINE_PROP(bool, dimAround, false, WINDOW_RULE_EFFECT_DIM_AROUND)
        DEFINE_PROP(bool, RGBX, false, WINDOW_RULE_EFFECT_FORCE_RGBX)
        DEFINE_PROP(bool, syncFullscreen, true, WINDOW_RULE_EFFECT_SYNC_FULLSCREEN)
        DEFINE_PROP(bool, tearing, false, WINDOW_RULE_EFFECT_IMMEDIATE)
        DEFINE_PROP(bool, xray, false, WINDOW_RULE_EFFECT_XRAY)
        DEFINE_PROP(bool, renderUnfocused, false, WINDOW_RULE_EFFECT_RENDER_UNFOCUSED)
        DEFINE_PROP(bool, noFollowMouse, false, WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE)
        DEFINE_PROP(bool, noScreenShare, false, WINDOW_RULE_EFFECT_NO_SCREEN_SHARE)
        DEFINE_PROP(bool, noVRR, false, WINDOW_RULE_EFFECT_NO_VRR)
        DEFINE_PROP(bool, persistentSize, false, WINDOW_RULE_EFFECT_PERSISTENT_SIZE)
        DEFINE_PROP(bool, stayFocused, false, WINDOW_RULE_EFFECT_STAY_FOCUSED)

        DEFINE_PROP(int, idleInhibitMode, false, WINDOW_RULE_EFFECT_IDLE_INHIBIT)

        DEFINE_PROP(Hyprlang::INT, borderSize, {std::string("general:border_size") COMMA sc<Hyprlang::INT>(0) COMMA std::nullopt}, WINDOW_RULE_EFFECT_BORDER_SIZE)
        DEFINE_PROP(Hyprlang::INT, rounding, {std::string("decoration:rounding") COMMA sc<Hyprlang::INT>(0) COMMA std::nullopt}, WINDOW_RULE_EFFECT_ROUNDING)

        DEFINE_PROP(Hyprlang::FLOAT, roundingPower, {std::string("decoration:rounding_power")}, WINDOW_RULE_EFFECT_ROUNDING_POWER)
        DEFINE_PROP(Hyprlang::FLOAT, scrollMouse, {std::string("input:scroll_factor")}, WINDOW_RULE_EFFECT_SCROLL_MOUSE)
        DEFINE_PROP(Hyprlang::FLOAT, scrollTouchpad, {std::string("input:touchpad:scroll_factor")}, WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD)

        DEFINE_PROP(std::string, animationStyle, std::string(""), WINDOW_RULE_EFFECT_ANIMATION)

        DEFINE_PROP(Vector2D, maxSize, Vector2D{}, WINDOW_RULE_EFFECT_MAX_SIZE)
        DEFINE_PROP(Vector2D, minSize, Vector2D{}, WINDOW_RULE_EFFECT_MIN_SIZE)

        DEFINE_PROP(CGradientValueData, activeBorderColor, {}, WINDOW_RULE_EFFECT_BORDER_COLOR)
        DEFINE_PROP(CGradientValueData, inactiveBorderColor, {}, WINDOW_RULE_EFFECT_BORDER_COLOR)

        std::vector<std::pair<std::string, std::underlying_type_t<eRuleProperty>>> m_dynamicTags;
        CTagKeeper                                                                 m_tagKeeper;

#undef COMMA
#undef DEFINE_PROP

      private:
        PHLWINDOWREF m_window;

        struct SRuleResult {
            bool needsRelayout = false;
            bool tagsChanged   = false;
        };

        SRuleResult applyDynamicRule(const SP<CWindowRule>& rule, Types::eOverridePriority prio);
        SRuleResult applyStaticRule(const SP<CWindowRule>& rule);
    };
};
