#pragma once

#include <string>
#include <cstdint>
#include <memory>

//NOLINTNEXTLINE
namespace re2 {
    class RE2;
};

class CLayerRule {
  public:
    CLayerRule(const std::string& rule, const std::string& targetNS);

    enum eRuleType : uint8_t {
        RULE_INVALID = 0,
        RULE_NOANIM,
        RULE_BLUR,
        RULE_BLURPOPUPS,
        RULE_DIMAROUND,
        RULE_IGNOREALPHA,
        RULE_IGNOREZERO,
        RULE_XRAY,
        RULE_ANIMATION,
        RULE_ORDER,
        RULE_ZUMBA,
    };

    eRuleType                 ruleType = RULE_INVALID;

    const std::string         targetNamespace;
    const std::string         rule;

    std::unique_ptr<re2::RE2> rTargetNamespaceRegex;
};