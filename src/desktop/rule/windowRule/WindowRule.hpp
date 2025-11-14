#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"
#include "../../../helpers/math/Math.hpp"

#include <unordered_set>

namespace Desktop::Rule {
    constexpr const char* EXEC_RULE_ENV_NAME = "HL_EXEC_RULE_TOKEN";

    enum eWindowRuleEffect : uint8_t {
        WINDOW_RULE_EFFECT_NONE = 0,

        // static
        WINDOW_RULE_EFFECT_FLOAT,
        WINDOW_RULE_EFFECT_TILE,
        WINDOW_RULE_EFFECT_FULLSCREEN,
        WINDOW_RULE_EFFECT_MAXIMIZE,
        WINDOW_RULE_EFFECT_FULLSCREENSTATE,
        WINDOW_RULE_EFFECT_MOVE,
        WINDOW_RULE_EFFECT_SIZE,
        WINDOW_RULE_EFFECT_CENTER,
        WINDOW_RULE_EFFECT_PSEUDO,
        WINDOW_RULE_EFFECT_MONITOR,
        WINDOW_RULE_EFFECT_WORKSPACE,
        WINDOW_RULE_EFFECT_NOINITIALFOCUS,
        WINDOW_RULE_EFFECT_PIN,
        WINDOW_RULE_EFFECT_GROUP,
        WINDOW_RULE_EFFECT_SUPPRESSEVENT,
        WINDOW_RULE_EFFECT_CONTENT,
        WINDOW_RULE_EFFECT_NOCLOSEFOR,

        // dynamic
        WINDOW_RULE_EFFECT_ROUNDING,
        WINDOW_RULE_EFFECT_ROUNDING_POWER,
        WINDOW_RULE_EFFECT_PERSISTENT_SIZE,
        WINDOW_RULE_EFFECT_ANIMATION,
        WINDOW_RULE_EFFECT_BORDER_COLOR,
        WINDOW_RULE_EFFECT_IDLE_INHIBIT,
        WINDOW_RULE_EFFECT_OPACITY,
        WINDOW_RULE_EFFECT_TAG,
        WINDOW_RULE_EFFECT_MAX_SIZE,
        WINDOW_RULE_EFFECT_MIN_SIZE,
        WINDOW_RULE_EFFECT_BORDER_SIZE,
        WINDOW_RULE_EFFECT_ALLOWS_INPUT,
        WINDOW_RULE_EFFECT_DIM_AROUND,
        WINDOW_RULE_EFFECT_DECORATE,
        WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE,
        WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO,
        WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR,
        WINDOW_RULE_EFFECT_NO_ANIM,
        WINDOW_RULE_EFFECT_NO_BLUR,
        WINDOW_RULE_EFFECT_NO_DIM,
        WINDOW_RULE_EFFECT_NO_FOCUS,
        WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE,
        WINDOW_RULE_EFFECT_NO_MAX_SIZE,
        WINDOW_RULE_EFFECT_NO_SHADOW,
        WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT,
        WINDOW_RULE_EFFECT_OPAQUE,
        WINDOW_RULE_EFFECT_FORCE_RGBX,
        WINDOW_RULE_EFFECT_SYNC_FULLSCREEN,
        WINDOW_RULE_EFFECT_IMMEDIATE,
        WINDOW_RULE_EFFECT_XRAY,
        WINDOW_RULE_EFFECT_RENDER_UNFOCUSED,
        WINDOW_RULE_EFFECT_NO_SCREEN_SHARE,
        WINDOW_RULE_EFFECT_NO_VRR,
        WINDOW_RULE_EFFECT_SCROLL_MOUSE,
        WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD,
        WINDOW_RULE_EFFECT_STAY_FOCUSED,
    };

    std::optional<Vector2D>          parseRelativeVector(PHLWINDOW w, const std::string& s);
    std::optional<eWindowRuleEffect> matchWindowEffectFromString(const std::string& s);
    std::optional<eWindowRuleEffect> matchWindowEffectFromString(const std::string_view& s);
    const std::vector<std::string>&  allWindowEffectStrings();

    class CWindowRule : public IRule {
      public:
        CWindowRule(const std::string& name = "");
        virtual ~CWindowRule() = default;

        static SP<CWindowRule>                                        buildFromExecString(std::string&&);

        virtual eRuleType                                             type();

        void                                                          addEffect(eWindowRuleEffect e, const std::string& result);
        const std::vector<std::pair<eWindowRuleEffect, std::string>>& effects();
        const std::unordered_set<eWindowRuleEffect>&                  effectsSet();

        bool                                                          matches(PHLWINDOW w, bool allowEnvLookup = false);

      private:
        std::vector<std::pair<eWindowRuleEffect, std::string>> m_effects;
        std::unordered_set<eWindowRuleEffect>                  m_effectSet;
    };
};
