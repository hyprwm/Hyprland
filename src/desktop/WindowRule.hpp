#pragma once

#include <string>
#include <cstdint>
#include "Rule.hpp"

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
        RULE_CONTENT,
        RULE_PERSISTENTSIZE
    };

    eRuleType         m_ruleType = RULE_INVALID;

    const std::string m_value;
    const std::string m_rule;
    const bool        m_v2       = false;
    const bool        m_execRule = false;

    std::string       m_title;
    std::string       m_class;
    std::string       m_initialTitle;
    std::string       m_initialClass;
    std::string       m_tag;
    int               m_X11             = -1; // -1 means "ANY"
    int               m_floating        = -1;
    int               m_fullscreen      = -1;
    int               m_pinned          = -1;
    int               m_focus           = -1;
    std::string       m_fullscreenState = ""; // empty means any
    std::string       m_onWorkspace     = ""; // empty means any
    std::string       m_workspace       = ""; // empty means any
    std::string       m_contentType     = ""; // empty means any
    std::string       m_xdgTag          = ""; // empty means any

    // precompiled regexes
    CRuleRegexContainer m_titleRegex;
    CRuleRegexContainer m_classRegex;
    CRuleRegexContainer m_initialTitleRegex;
    CRuleRegexContainer m_initialClassRegex;
    CRuleRegexContainer m_v1Regex;
};