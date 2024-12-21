#pragma once

#include <string>
#include <cstdint>
#include <memory>

//NOLINTNEXTLINE
namespace re2 {
    class RE2;
};

class CWindowRule {
  public:
    CWindowRule(const std::string& rule, const std::string& value, bool isV2 = false, bool isExecRule = false);

    enum eRuleType : uint8_t {
        RULE_INVALID = 0,
        RULE_FLOAT,
        RULE_FULLSCREEN,
        RULE_MAXIMIZE,
        RULE_NOINITIALFOCUS,
        RULE_PIN,
        RULE_STAYFOCUSED,
        RULE_TILE,
        RULE_RENDERUNFOCUSED,
        RULE_ANIMATION,
        RULE_BORDERCOLOR,
        RULE_CENTER,
        RULE_FULLSCREENSTATE,
        RULE_GROUP,
        RULE_IDLEINHIBIT,
        RULE_MAXSIZE,
        RULE_MINSIZE,
        RULE_MONITOR,
        RULE_MOVE,
        RULE_OPACITY,
        RULE_PLUGIN,
        RULE_PSEUDO,
        RULE_SIZE,
        RULE_SUPPRESSEVENT,
        RULE_TAG,
        RULE_WORKSPACE,
        RULE_PROP,
    };

    eRuleType         ruleType = RULE_INVALID;

    const std::string szValue;
    const std::string szRule;
    const bool        v2       = false;
    const bool        execRule = false;

    std::string       szTitle;
    std::string       szClass;
    std::string       szInitialTitle;
    std::string       szInitialClass;
    std::string       szTag;
    int               bX11              = -1; // -1 means "ANY"
    int               bFloating         = -1;
    int               bFullscreen       = -1;
    int               bPinned           = -1;
    int               bFocus            = -1;
    std::string       szFullscreenState = ""; // empty means any
    std::string       szOnWorkspace     = ""; // empty means any
    std::string       szWorkspace       = ""; // empty means any

    // precompiled regexes
    std::unique_ptr<re2::RE2> rTitle;
    std::unique_ptr<re2::RE2> rClass;
    std::unique_ptr<re2::RE2> rInitialTitle;
    std::unique_ptr<re2::RE2> rInitialClass;
    std::unique_ptr<re2::RE2> rV1Regex;
};