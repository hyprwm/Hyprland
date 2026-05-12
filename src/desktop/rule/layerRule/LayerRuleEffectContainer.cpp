#include "LayerRuleEffectContainer.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

//
SP<CLayerRuleEffectContainer> Rule::layerEffects() {
    static SP<CLayerRuleEffectContainer> container = makeShared<CLayerRuleEffectContainer>();
    return container;
}

static const std::vector<std::string> EFFECT_STRINGS = {
    "__internal_none",        //
    "no_anim",                //
    "blur",                   //
    "blur_popups",            //
    "ignore_alpha",           //
    "dim_around",             //
    "xray",                   //
    "animation",              //
    "order",                  //
    "above_lock",             //
    "no_screen_share",        //
    "__internal_last_static", //
};

// This is here so that if we change the rules, we get reminded to update
// the strings.
static_assert(LAYER_RULE_EFFECT_LAST_STATIC == 11);

CLayerRuleEffectContainer::CLayerRuleEffectContainer() : IEffectContainer<eLayerRuleEffect>(std::vector<std::string>{EFFECT_STRINGS}) {
    ;
}
