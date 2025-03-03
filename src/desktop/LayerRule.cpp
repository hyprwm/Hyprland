#include <re2/re2.h>
#include "LayerRule.hpp"
#include <unordered_set>
#include <algorithm>
#include "../debug/Log.hpp"

static const auto RULES        = std::unordered_set<std::string>{"noanim", "blur", "blurpopups", "dimaround"};
static const auto RULES_PREFIX = std::unordered_set<std::string>{"ignorealpha", "ignorezero", "xray", "animation", "order"};

CLayerRule::CLayerRule(const std::string& rule_, const std::string& ns_) : m_TARGETNAMESPACE(ns_), m_RULE(rule_) {
    const bool VALID = RULES.contains(m_RULE) || std::any_of(RULES_PREFIX.begin(), RULES_PREFIX.end(), [&rule_](const auto& prefix) { return rule_.starts_with(prefix); });

    if (!VALID)
        return;

    if (m_RULE == "noanim")
        m_ruleType = RULE_NOANIM;
    else if (m_RULE == "blur")
        m_ruleType = RULE_BLUR;
    else if (m_RULE == "blurpopups")
        m_ruleType = RULE_BLURPOPUPS;
    else if (m_RULE == "dimaround")
        m_ruleType = RULE_DIMAROUND;
    else if (m_RULE.starts_with("ignorealpha"))
        m_ruleType = RULE_IGNOREALPHA;
    else if (m_RULE.starts_with("ignorezero"))
        m_ruleType = RULE_IGNOREZERO;
    else if (m_RULE.starts_with("xray"))
        m_ruleType = RULE_XRAY;
    else if (m_RULE.starts_with("animation"))
        m_ruleType = RULE_ANIMATION;
    else if (m_RULE.starts_with("order"))
        m_ruleType = RULE_ORDER;
    else {
        NDebug::log(ERR, "CLayerRule: didn't match a rule that was found valid?!");
        m_ruleType = RULE_INVALID;
    }
}