#pragma once

#include <string>
#include <cstdint>
#include "Rule.hpp"

class CLayerRule {
  public:
    CLayerRule(const std::string& rule, const std::string& targetNS);

    enum eRuleType : uint8_t {
        RULE_INVALID = 0,
        RULE_NOANIM,
        RULE_BLUR,
        RULE_BLURPOPUPS,
        RULE_DIMAROUND,
        RULE_ABOVELOCK,
        RULE_IGNOREALPHA,
        RULE_IGNOREZERO,
        RULE_XRAY,
        RULE_ANIMATION,
        RULE_ORDER,
        RULE_ZUMBA,
    };

    eRuleType           ruleType = RULE_INVALID;

    const std::string   targetNamespace;
    const std::string   rule;

    CRuleRegexContainer targetNamespaceRegex;
};
