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
    "size",      "suppressevent", "tag",        "workspace", "xray",    "contentscale",
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
        ruleType = RULE_FLOAT;
    else if (rule == "fullscreen")
        ruleType = RULE_FULLSCREEN;
    else if (rule == "maximize")
        ruleType = RULE_MAXIMIZE;
    else if (rule == "noinitialfocus")
        ruleType = RULE_NOINITIALFOCUS;
    else if (rule == "pin")
        ruleType = RULE_PIN;
    else if (rule == "stayfocused")
        ruleType = RULE_STAYFOCUSED;
    else if (rule == "tile")
        ruleType = RULE_TILE;
    else if (rule == "renderunfocused")
        ruleType = RULE_RENDERUNFOCUSED;
    else if (rule.starts_with("animation"))
        ruleType = RULE_ANIMATION;
    else if (rule.starts_with("bordercolor"))
        ruleType = RULE_BORDERCOLOR;
    else if (rule.starts_with("center"))
        ruleType = RULE_CENTER;
    else if (rule.starts_with("fullscreenstate"))
        ruleType = RULE_FULLSCREENSTATE;
    else if (rule.starts_with("group"))
        ruleType = RULE_GROUP;
    else if (rule.starts_with("idleinhibit"))
        ruleType = RULE_IDLEINHIBIT;
    else if (rule.starts_with("maxsize"))
        ruleType = RULE_MAXSIZE;
    else if (rule.starts_with("minsize"))
        ruleType = RULE_MINSIZE;
    else if (rule.starts_with("contentscale"))
        ruleType = RULE_CONTENTSCALE;
    else if (rule.starts_with("monitor"))
        ruleType = RULE_MONITOR;
    else if (rule.starts_with("move"))
        ruleType = RULE_MOVE;
    else if (rule.starts_with("opacity"))
        ruleType = RULE_OPACITY;
    else if (rule.starts_with("plugin:"))
        ruleType = RULE_PLUGIN;
    else if (rule.starts_with("pseudo"))
        ruleType = RULE_PSEUDO;
    else if (rule.starts_with("size"))
        ruleType = RULE_SIZE;
    else if (rule.starts_with("suppressevent"))
        ruleType = RULE_SUPPRESSEVENT;
    else if (rule.starts_with("tag"))
        ruleType = RULE_TAG;
    else if (rule.starts_with("workspace"))
        ruleType = RULE_WORKSPACE;
    else if (rule.starts_with("prop"))
        ruleType = RULE_PROP;
    else if (rule.starts_with("content"))
        ruleType = RULE_CONTENT;
    else {
        // check if this is a prop.
        const CVarList VARS(rule, 0, 's', true);
        if (NWindowProperties::intWindowProperties.find(VARS[0]) != NWindowProperties::intWindowProperties.end() ||
            NWindowProperties::boolWindowProperties.find(VARS[0]) != NWindowProperties::boolWindowProperties.end() ||
            NWindowProperties::floatWindowProperties.find(VARS[0]) != NWindowProperties::floatWindowProperties.end()) {
            *const_cast<std::string*>(&szRule) = "prop " + rule;
            ruleType                           = RULE_PROP;
            Debug::log(LOG, "CWindowRule: direct prop rule found, rewritten {} -> {}", rule, szRule);
        } else {
            Debug::log(ERR, "CWindowRule: didn't match a rule that was found valid?!");
            ruleType = RULE_INVALID;
        }
    }
}
