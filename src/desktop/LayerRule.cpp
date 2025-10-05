#include <re2/re2.h>
#include "LayerRule.hpp"
#include <unordered_set>
#include <algorithm>
#include "../debug/Log.hpp"

static const auto RULES        = std::unordered_set<std::string>{"noanim", "blur", "blurpopups", "dimaround", "noscreenshare"};
static const auto RULES_PREFIX = std::unordered_set<std::string>{"ignorealpha", "ignorezero", "xray", "animation", "order", "abovelock"};

CLayerRule::CLayerRule(const std::string& rule_, const std::string& ns_) : m_targetNamespace(ns_), m_rule(rule_) {
    const auto firstSpace = m_rule.find(' ');
    const auto baseRule   = m_rule.substr(0, firstSpace == std::string::npos ? m_rule.size() : firstSpace);

    const bool VALID = RULES.contains(baseRule) || std::ranges::any_of(RULES_PREFIX, [&rule_](const auto& prefix) { return rule_.starts_with(prefix); });

    if (!VALID)
        return;

    if (baseRule == "noanim")
        m_ruleType = RULE_NOANIM;
    else if (baseRule == "blur")
        m_ruleType = RULE_BLUR;
    else if (baseRule == "blurpopups")
        m_ruleType = RULE_BLURPOPUPS;
    else if (baseRule == "dimaround")
        m_ruleType = RULE_DIMAROUND;
    else if (baseRule == "noscreenshare")
        m_ruleType = RULE_NOSCREENSHARE;
    else if (m_rule.starts_with("ignorealpha"))
        m_ruleType = RULE_IGNOREALPHA;
    else if (m_rule.starts_with("ignorezero"))
        m_ruleType = RULE_IGNOREZERO;
    else if (m_rule.starts_with("xray"))
        m_ruleType = RULE_XRAY;
    else if (m_rule.starts_with("animation"))
        m_ruleType = RULE_ANIMATION;
    else if (m_rule.starts_with("order"))
        m_ruleType = RULE_ORDER;
    else if (m_rule.starts_with("abovelock"))
        m_ruleType = RULE_ABOVELOCK;
    else {
        Debug::log(ERR, "CLayerRule: didn't match a rule that was found valid?!");
        m_ruleType = RULE_INVALID;
    }
}
