#pragma once

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

        void propertiesChanged(std::underlying_type_t<eRuleProperty> props);
        void resetProps(std::underlying_type_t<eRuleProperty> props, Types::eOverridePriority prio = Types::PRIORITY_WINDOW_RULE);
        void readStaticRules();
        void applyStaticRules();

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

#define COMMA ,
#define DEFINE_PROP(type, name, def)                                                                                                                                               \
  private:                                                                                                                                                                         \
    std::pair<Types::COverridableVar<type>, std::underlying_type_t<eRuleProperty>> m_##name = {def, RULE_PROP_NONE};                                                               \
                                                                                                                                                                                   \
  public:                                                                                                                                                                          \
    Types::COverridableVar<type>& name() {                                                                                                                                         \
        return m_##name.first;                                                                                                                                                     \
    }                                                                                                                                                                              \
    void name##Override(const Types::COverridableVar<type>& other) {                                                                                                               \
        m_##name.first = other;                                                                                                                                                    \
    }

        // dynamic props
        DEFINE_PROP(Types::SAlphaValue, alpha, Types::SAlphaValue{})
        DEFINE_PROP(Types::SAlphaValue, alphaInactive, Types::SAlphaValue{})
        DEFINE_PROP(Types::SAlphaValue, alphaFullscreen, Types::SAlphaValue{})

        DEFINE_PROP(bool, allowsInput, false)
        DEFINE_PROP(bool, decorate, true)
        DEFINE_PROP(bool, focusOnActivate, false)
        DEFINE_PROP(bool, keepAspectRatio, false)
        DEFINE_PROP(bool, nearestNeighbor, false)
        DEFINE_PROP(bool, noAnim, false)
        DEFINE_PROP(bool, noBlur, false)
        DEFINE_PROP(bool, noDim, false)
        DEFINE_PROP(bool, noFocus, false)
        DEFINE_PROP(bool, noMaxSize, false)
        DEFINE_PROP(bool, noShadow, false)
        DEFINE_PROP(bool, noShortcutsInhibit, false)
        DEFINE_PROP(bool, opaque, false)
        DEFINE_PROP(bool, dimAround, false)
        DEFINE_PROP(bool, RGBX, false)
        DEFINE_PROP(bool, syncFullscreen, false)
        DEFINE_PROP(bool, tearing, false)
        DEFINE_PROP(bool, xray, false)
        DEFINE_PROP(bool, renderUnfocused, false)
        DEFINE_PROP(bool, noFollowMouse, false)
        DEFINE_PROP(bool, noScreenShare, false)
        DEFINE_PROP(bool, noVRR, false)
        DEFINE_PROP(bool, persistentSize, false)
        DEFINE_PROP(bool, stayFocused, false)

        DEFINE_PROP(int, idleInhibitMode, false)

        DEFINE_PROP(Hyprlang::INT, borderSize, {std::string("general:border_size") COMMA sc<Hyprlang::INT>(0) COMMA std::nullopt})
        DEFINE_PROP(Hyprlang::INT, rounding, {std::string("decoration:rounding") COMMA sc<Hyprlang::INT>(0) COMMA std::nullopt})

        DEFINE_PROP(Hyprlang::FLOAT, roundingPower, {std::string("decoration:rounding_power")})
        DEFINE_PROP(Hyprlang::FLOAT, scrollMouse, {std::string("input:scroll_factor")})
        DEFINE_PROP(Hyprlang::FLOAT, scrollTouchpad, {std::string("input:touchpad:scroll_factor")})

        DEFINE_PROP(std::string, animationStyle, std::string(""))

        DEFINE_PROP(Vector2D, maxSize, Vector2D{})
        DEFINE_PROP(Vector2D, minSize, Vector2D{})

        DEFINE_PROP(CGradientValueData, activeBorderColor, {})
        DEFINE_PROP(CGradientValueData, inactiveBorderColor, {})

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

        SRuleResult applyDynamicRule(const SP<CWindowRule>& rule);
        SRuleResult applyStaticRule(const SP<CWindowRule>& rule);
    };
};
