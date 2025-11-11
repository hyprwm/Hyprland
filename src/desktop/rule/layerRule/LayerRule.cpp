#include "LayerRule.hpp"
#include "../../../debug/Log.hpp"
#include "../../LayerSurface.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

static const std::unordered_map<eLayerRuleEffect, std::string> EFFECT_STRINGS = {
    {LAYER_RULE_EFFECT_NO_ANIM, "no_anim"},                 //
    {LAYER_RULE_EFFECT_BLUR, "blur"},                       //
    {LAYER_RULE_EFFECT_BLUR_POPUPS, "blur_popups"},         //
    {LAYER_RULE_EFFECT_IGNORE_ALPHA, "ignore_alpha"},       //
    {LAYER_RULE_EFFECT_DIM_AROUND, "dim_around"},           //
    {LAYER_RULE_EFFECT_XRAY, "xray"},                       //
    {LAYER_RULE_EFFECT_ANIMATION, "animation"},             //
    {LAYER_RULE_EFFECT_ORDER, "order"},                     //
    {LAYER_RULE_EFFECT_ABOVE_LOCK, "above_lock"},           //
    {LAYER_RULE_EFFECT_NO_SCREEN_SHARE, "no_screen_share"}, //
};

std::optional<eLayerRuleEffect> Rule::matchLayerEffectFromString(const std::string& s) {
    const auto IT = std::ranges::find_if(EFFECT_STRINGS, [&s](const auto& el) { return el.second == s; });
    if (IT == EFFECT_STRINGS.end())
        return std::nullopt;

    return IT->first;
}

const std::vector<std::string>& Rule::allLayerEffectStrings() {
    static std::vector<std::string> strings;
    static bool                     once = true;
    if (once) {
        for (const auto& [k, v] : EFFECT_STRINGS) {
            strings.emplace_back(v);
        }
        once = false;
    }
    return strings;
}

CLayerRule::CLayerRule(const std::string& name) : IRule(name) {
    ;
}

eRuleType CLayerRule::type() {
    return RULE_TYPE_LAYER;
}

void CLayerRule::addEffect(eLayerRuleEffect e, const std::string& result) {
    m_effects.emplace_back(std::make_pair<>(e, result));
}

const std::vector<std::pair<eLayerRuleEffect, std::string>>& CLayerRule::effects() {
    return m_effects;
}

bool CLayerRule::matches(PHLLS ls) {
    if (m_matchEngines.empty())
        return false;

    for (const auto& [prop, engine] : m_matchEngines) {
        switch (prop) {
            default: {
                Debug::log(TRACE, "CLayerRule::matches: skipping prop entry {}", sc<std::underlying_type_t<eRuleProperty>>(prop));
                break;
            }

            case RULE_PROP_NAMESPACE:
                if (!engine->match(ls->m_namespace))
                    return false;
                break;
        }
    }

    return true;
}
