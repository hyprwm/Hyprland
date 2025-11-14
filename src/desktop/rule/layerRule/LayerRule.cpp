#include "LayerRule.hpp"
#include "../../../debug/Log.hpp"
#include "../../LayerSurface.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

CLayerRule::CLayerRule(const std::string& name) : IRule(name) {
    ;
}

eRuleType CLayerRule::type() {
    return RULE_TYPE_LAYER;
}

void CLayerRule::addEffect(CLayerRule::storageType e, const std::string& result) {
    m_effects.emplace_back(std::make_pair<>(e, result));
}

const std::vector<std::pair<CLayerRule::storageType, std::string>>& CLayerRule::effects() {
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
