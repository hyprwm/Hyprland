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
        RULE_NOSCREENSHARE
    };

    eRuleType           m_ruleType = RULE_INVALID;

    const std::string   m_targetNamespace;
    const std::string   m_rule;

    CRuleRegexContainer m_targetNamespaceRegex;
};
