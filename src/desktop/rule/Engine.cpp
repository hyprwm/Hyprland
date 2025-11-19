#include "Engine.hpp"
#include "Rule.hpp"
#include "../LayerSurface.hpp"
#include "../../Compositor.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

SP<CRuleEngine> Rule::ruleEngine() {
    static SP<CRuleEngine> engine = makeShared<CRuleEngine>();
    return engine;
}

void CRuleEngine::registerRule(SP<IRule>&& rule) {
    m_rules.emplace_back(std::move(rule));
}

void CRuleEngine::unregisterRule(const std::string& name) {
    if (name.empty())
        return;

    std::erase_if(m_rules, [&name](const auto& el) { return el->name() == name; });
}

void CRuleEngine::unregisterRule(const SP<IRule>& rule) {
    std::erase(m_rules, rule);
    cleanExecRules();
}

void CRuleEngine::cleanExecRules() {
    std::erase_if(m_rules, [](const auto& e) { return e->isExecRule() && e->execExpired(); });
}

void CRuleEngine::updateAllRules() {
    cleanExecRules();
    for (const auto& w : g_pCompositor->m_windows) {
        if (!validMapped(w) || w->isHidden())
            continue;

        w->m_ruleApplicator->propertiesChanged(RULE_PROP_ALL);
    }
    for (const auto& ls : g_pCompositor->m_layers) {
        if (!validMapped(ls))
            continue;

        ls->m_ruleApplicator->propertiesChanged(RULE_PROP_ALL);
    }
}

void CRuleEngine::clearAllRules() {
    std::erase_if(m_rules, [](const auto& e) { return !e->isExecRule() || e->execExpired(); });
}

const std::vector<SP<IRule>>& CRuleEngine::rules() {
    return m_rules;
}
