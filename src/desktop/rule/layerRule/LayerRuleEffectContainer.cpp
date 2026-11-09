#include "LayerRuleEffectContainer.hpp"
#include "../../../config/values/types/BoolValue.hpp"
#include "../../../config/values/types/IntValue.hpp"
#include "../../../config/values/types/FloatValue.hpp"
#include "../../../config/values/types/StringValue.hpp"

using namespace Desktop;
using namespace Desktop::Rule;
using namespace Config::Values;

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

static SP<Config::Values::IValue> boolValue(const char* name) {
    return makeShared<CBoolValue>(name, "", false);
}

static SP<Config::Values::IValue> intValue(const char* name, Config::INTEGER def, std::optional<Config::INTEGER> min = {}, std::optional<Config::INTEGER> max = {}) {
    return makeShared<CIntValue>(name, "", def, SIntValueOptions{.min = min, .max = max});
}

static SP<Config::Values::IValue> floatValue(const char* name, Config::FLOAT def, std::optional<Config::FLOAT> min = {}, std::optional<Config::FLOAT> max = {}) {
    return makeShared<CFloatValue>(name, "", def, SFloatValueOptions{.min = min, .max = max});
}

static SP<Config::Values::IValue> stringValue(const char* name) {
    return makeShared<CStringValue>(name, "", Config::STRING{});
}

CLayerRuleEffectContainer::CLayerRuleEffectContainer() : IEffectContainer<eLayerRuleEffect>(std::vector<std::string>{EFFECT_STRINGS}) {
    registerStaticEffect(LAYER_RULE_EFFECT_NO_ANIM, boolValue("no_anim"));
    registerStaticEffect(LAYER_RULE_EFFECT_BLUR, boolValue("blur"));
    registerStaticEffect(LAYER_RULE_EFFECT_BLUR_POPUPS, boolValue("blur_popups"));
    registerStaticEffect(LAYER_RULE_EFFECT_IGNORE_ALPHA, floatValue("ignore_alpha", 0.F, 0.F, 1.F));
    registerStaticEffect(LAYER_RULE_EFFECT_DIM_AROUND, boolValue("dim_around"));
    registerStaticEffect(LAYER_RULE_EFFECT_XRAY, boolValue("xray"));
    registerStaticEffect(LAYER_RULE_EFFECT_ANIMATION, stringValue("animation"));
    registerStaticEffect(LAYER_RULE_EFFECT_ORDER, intValue("order", 0));
    registerStaticEffect(LAYER_RULE_EFFECT_ABOVE_LOCK, intValue("above_lock", 0, 0, 2));
    registerStaticEffect(LAYER_RULE_EFFECT_NO_SCREEN_SHARE, boolValue("no_screen_share"));
}
