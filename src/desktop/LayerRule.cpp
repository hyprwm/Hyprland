#include <re2/re2.h>
#include "LayerRule.hpp"
#include <unordered_set>
#include <algorithm>
#include "../debug/Log.hpp"

static const auto RULES        = std::unordered_set<std::string>{"noanim", "blur", "blurpopups", "dimaround"};
static const auto RULES_PREFIX = std::unordered_set<std::string>{"ignorealpha", "ignorezero", "xray", "animation", "order", "abovelock"};

CLayerRule::CLayerRule(const std::string& rule_, const std::string& ns_) : targetNamespace(ns_), rule(rule_) {
    const bool VALID = RULES.contains(rule) || std::any_of(RULES_PREFIX.begin(), RULES_PREFIX.end(), [&rule_](const auto& prefix) { return rule_.starts_with(prefix); });

    if (!VALID)
        return;

    if (rule == "noanim")
        ruleType = RULE_NOANIM;
    else if (rule == "blur")
        ruleType = RULE_BLUR;
    else if (rule == "blurpopups")
        ruleType = RULE_BLURPOPUPS;
    else if (rule == "dimaround")
        ruleType = RULE_DIMAROUND;
    else if (rule.starts_with("ignorealpha"))
        ruleType = RULE_IGNOREALPHA;
    else if (rule.starts_with("ignorezero"))
        ruleType = RULE_IGNOREZERO;
    else if (rule.starts_with("xray"))
        ruleType = RULE_XRAY;
    else if (rule.starts_with("animation"))
        ruleType = RULE_ANIMATION;
    else if (rule.starts_with("order"))
        ruleType = RULE_ORDER;
    else if (rule.starts_with("abovelock"))
        ruleType = RULE_ABOVELOCK;
    else {
        Debug::log(ERR, "CLayerRule: didn't match a rule that was found valid?!");
        ruleType = RULE_INVALID;
    }
}
