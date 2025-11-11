#include "LayerRuleApplicator.hpp"
#include "LayerRule.hpp"
#include "../Engine.hpp"
#include "../../LayerSurface.hpp"
#include "../../types/OverridableVar.hpp"
#include "../../../helpers/MiscFunctions.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

CLayerRuleApplicator::CLayerRuleApplicator(PHLLS ls) : m_ls(ls) {
    ;
}

void CLayerRuleApplicator::resetProps(std::underlying_type_t<eRuleProperty> props, Types::eOverridePriority prio) {
    // TODO: fucking kill me, is there a better way to do this?

#define UNSET(x)                                                                                                                                                                   \
    if (m_##x.second & props) {                                                                                                                                                    \
        if (prio == Types::PRIORITY_WINDOW_RULE)                                                                                                                                   \
            m_##x.second &= ~props;                                                                                                                                                \
        m_##x.first.unset(prio);                                                                                                                                                   \
    }

    UNSET(noanim)
    UNSET(blur)
    UNSET(blurPopups)
    UNSET(dimAround)
    UNSET(xray)
    UNSET(noScreenShare)
    UNSET(order)
    UNSET(aboveLock)
    UNSET(ignoreAlpha)
    UNSET(animationStyle)
}

void CLayerRuleApplicator::applyDynamicRule(const SP<CLayerRule>& rule) {
    for (const auto& [key, effect] : rule->effects()) {
        switch (key) {
            case LAYER_RULE_EFFECT_NONE: {
                Debug::log(ERR, "CLayerRuleApplicator::applyDynamicRule: BUG THIS: LAYER_RULE_EFFECT_NONE??");
                break;
            }
            case LAYER_RULE_EFFECT_NO_ANIM: {
                m_noanim.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_noanim.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_BLUR: {
                m_blur.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_blur.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_BLUR_POPUPS: {
                m_blurPopups.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_blurPopups.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_DIM_AROUND: {
                m_dimAround.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_dimAround.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_XRAY: {
                m_xray.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_xray.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_NO_SCREEN_SHARE: {
                m_noScreenShare.first.set(truthy(effect), Types::PRIORITY_WINDOW_RULE);
                m_noScreenShare.second |= rule->getPropertiesMask();
                break;
            }
            case LAYER_RULE_EFFECT_ORDER: {
                try {
                    m_noScreenShare.first.set(std::stoi(effect), Types::PRIORITY_WINDOW_RULE);
                    m_noScreenShare.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CLayerRuleApplicator::applyDynamicRule: invalid order {}", effect); }
                break;
            }
            case LAYER_RULE_EFFECT_ABOVE_LOCK: {
                try {
                    m_aboveLock.first.set(std::clamp(std::stoull(effect), 0ULL, 2ULL), Types::PRIORITY_WINDOW_RULE);
                    m_aboveLock.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CLayerRuleApplicator::applyDynamicRule: invalid order {}", effect); }
                break;
            }
            case LAYER_RULE_EFFECT_IGNORE_ALPHA: {
                try {
                    m_ignoreAlpha.first.set(std::clamp(std::stof(effect), 0.F, 1.F), Types::PRIORITY_WINDOW_RULE);
                    m_ignoreAlpha.second |= rule->getPropertiesMask();
                } catch (...) { Debug::log(ERR, "CLayerRuleApplicator::applyDynamicRule: invalid order {}", effect); }
                break;
            }
            case LAYER_RULE_EFFECT_ANIMATION: {
                m_animationStyle.first.set(effect, Types::PRIORITY_WINDOW_RULE);
                m_animationStyle.second |= rule->getPropertiesMask();
                break;
            }
        }
    }
}

void CLayerRuleApplicator::propertiesChanged(std::underlying_type_t<eRuleProperty> props) {
    if (!m_ls)
        return;

    resetProps(props);

    for (const auto& r : ruleEngine()->rules()) {
        if (r->type() != RULE_TYPE_LAYER)
            continue;

        if (!(r->getPropertiesMask() & props))
            continue;

        auto wr = reinterpretPointerCast<CLayerRule>(r);

        if (!wr->matches(m_ls.lock()))
            continue;

        applyDynamicRule(wr);
    }
}
