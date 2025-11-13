#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"

namespace Desktop::Rule {
    enum eLayerRuleEffect : uint8_t {
        LAYER_RULE_EFFECT_NONE = 0,
        LAYER_RULE_EFFECT_NO_ANIM,
        LAYER_RULE_EFFECT_BLUR,
        LAYER_RULE_EFFECT_BLUR_POPUPS,
        LAYER_RULE_EFFECT_IGNORE_ALPHA,
        LAYER_RULE_EFFECT_DIM_AROUND,
        LAYER_RULE_EFFECT_XRAY,
        LAYER_RULE_EFFECT_ANIMATION,
        LAYER_RULE_EFFECT_ORDER,
        LAYER_RULE_EFFECT_ABOVE_LOCK,
        LAYER_RULE_EFFECT_NO_SCREEN_SHARE,
    };

    std::optional<eLayerRuleEffect> matchLayerEffectFromString(const std::string& s);
    std::optional<eLayerRuleEffect> matchLayerEffectFromString(const std::string_view& s);
    const std::vector<std::string>& allLayerEffectStrings();

    class CLayerRule : public IRule {
      public:
        CLayerRule(const std::string& name = "");
        virtual ~CLayerRule() = default;

        virtual eRuleType                                            type();

        void                                                         addEffect(eLayerRuleEffect e, const std::string& result);
        const std::vector<std::pair<eLayerRuleEffect, std::string>>& effects();

        bool                                                         matches(PHLLS w);

      private:
        std::vector<std::pair<eLayerRuleEffect, std::string>> m_effects;
    };
};
