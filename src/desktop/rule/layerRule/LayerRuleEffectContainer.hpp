#pragma once

#include "../effect/EffectContainer.hpp"
#include "../../../helpers/memory/Memory.hpp"

#pragma once

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

        LAYER_RULE_EFFECT_LAST_STATIC,
    };

    class CLayerRuleEffectContainer : public IEffectContainer<eLayerRuleEffect> {
      public:
        CLayerRuleEffectContainer();
        virtual ~CLayerRuleEffectContainer() = default;
    };

    SP<CLayerRuleEffectContainer> layerEffects();
};