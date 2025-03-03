#include "WindowRule.hpp"
#include <unordered_set>
#include <algorithm>
#include <re2/re2.h>
#include "../config/ConfigManager.hpp"

static const auto RULES = std::unordered_set<std::string>{
    "float", "fullscreen", "maximize", "noinitialfocus", "pin", "stayfocused", "tile", "renderunfocused",
};
static const auto RULES_PREFIX = std::unordered_set<std::string>{
    "animation", "bordercolor",   "bordersize", "center",    "content", "fullscreenstate", "group",    "idleinhibit",   "maxsize",     "minsize",
    "monitor",   "move",          "opacity",    "plugin:",   "prop",    "pseudo",          "rounding", "roundingpower", "scrollmouse", "scrolltouchpad",
    "size",      "suppressevent", "tag",        "workspace", "xray",
};

CWindowRule::CWindowRule(const std::string& rule, const std::string& value, bool isV2, bool isExecRule) : szValue(value), szRule(rule), v2(isV2), execRule(isExecRule) {
    const auto VALS  = CVarList(rule, 2, ' ');
    const bool VALID = RULES.contains(rule) || std::any_of(RULES_PREFIX.begin(), RULES_PREFIX.end(), [&rule](auto prefix) { return rule.starts_with(prefix); }) ||
        (NWindowProperties::boolWindowProperties.find(VALS[0]) != NWindowProperties::boolWindowProperties.end()) ||
        (NWindowProperties::intWindowProperties.find(VALS[0]) != NWindowProperties::intWindowProperties.end()) ||
        (NWindowProperties::floatWindowProperties.find(VALS[0]) != NWindowProperties::floatWindowProperties.end());

    if (!VALID)
        return;

    if (rule == "float")
        m_ruleType = RULE_FLOAT;
    else if (rule == "fullscreen")
        m_ruleType = RULE_FULLSCREEN;
    else if (rule == "maximize")
        m_ruleType = RULE_MAXIMIZE;
    else if (rule == "noinitialfocus")
        m_ruleType = RULE_NOINITIALFOCUS;
    else if (rule == "pin")
        m_ruleType = RULE_PIN;
    else if (rule == "stayfocused")
        m_ruleType = RULE_STAYFOCUSED;
    else if (rule == "tile")
        m_ruleType = RULE_TILE;
    else if (rule == "renderunfocused")
        m_ruleType = RULE_RENDERUNFOCUSED;
    else if (rule.starts_with("animation"))
        m_ruleType = RULE_ANIMATION;
    else if (rule.starts_with("bordercolor"))
        m_ruleType = RULE_BORDERCOLOR;
    else if (rule.starts_with("center"))
        m_ruleType = RULE_CENTER;
    else if (rule.starts_with("fullscreenstate"))
        m_ruleType = RULE_FULLSCREENSTATE;
    else if (rule.starts_with("group"))
        m_ruleType = RULE_GROUP;
    else if (rule.starts_with("idleinhibit"))
        m_ruleType = RULE_IDLEINHIBIT;
    else if (rule.starts_with("maxsize"))
        m_ruleType = RULE_MAXSIZE;
    else if (rule.starts_with("minsize"))
        m_ruleType = RULE_MINSIZE;
    else if (rule.starts_with("monitor"))
        m_ruleType = RULE_MONITOR;
    else if (rule.starts_with("move"))
        m_ruleType = RULE_MOVE;
    else if (rule.starts_with("opacity"))
        m_ruleType = RULE_OPACITY;
    else if (rule.starts_with("plugin:"))
        m_ruleType = RULE_PLUGIN;
    else if (rule.starts_with("pseudo"))
        m_ruleType = RULE_PSEUDO;
    else if (rule.starts_with("size"))
        m_ruleType = RULE_SIZE;
    else if (rule.starts_with("suppressevent"))
        m_ruleType = RULE_SUPPRESSEVENT;
    else if (rule.starts_with("tag"))
        m_ruleType = RULE_TAG;
    else if (rule.starts_with("workspace"))
        m_ruleType = RULE_WORKSPACE;
    else if (rule.starts_with("prop"))
        m_ruleType = RULE_PROP;
    else if (rule.starts_with("content"))
        m_ruleType = RULE_CONTENT;
    else {
        // check if this is a prop.
        const CVarList VARS(rule, 0, 's', true);
        if (NWindowProperties::intWindowProperties.find(VARS[0]) != NWindowProperties::intWindowProperties.end() ||
            NWindowProperties::boolWindowProperties.find(VARS[0]) != NWindowProperties::boolWindowProperties.end() ||
            NWindowProperties::floatWindowProperties.find(VARS[0]) != NWindowProperties::floatWindowProperties.end()) {
            *const_cast<std::string*>(&szRule) = "prop " + rule;
            m_ruleType                         = RULE_PROP;
            NDebug::log(LOG, "CWindowRule: direct prop rule found, rewritten {} -> {}", rule, szRule);
        } else {
            NDebug::log(ERR, "CWindowRule: didn't match a rule that was found valid?!");
            m_ruleType = RULE_INVALID;
        }
    }
}
