#include "Rule.hpp"
#include "../../debug/log/Logger.hpp"
#include <re2/re2.h>

#include "matchEngine/RegexMatchEngine.hpp"
#include "matchEngine/BoolMatchEngine.hpp"
#include "matchEngine/IntMatchEngine.hpp"
#include "matchEngine/WorkspaceMatchEngine.hpp"
#include "matchEngine/TagMatchEngine.hpp"

#include <algorithm>
#include <array>
#include <utility>

using namespace Desktop;
using namespace Desktop::Rule;

constexpr auto MATCH_PROP_STRINGS =
    [](auto matchPropStrings) {
        std::ranges::sort(matchPropStrings, {}, [](auto pair) { return pair.second; });
        std::array<eRuleProperty, matchPropStrings.size()>    props;
        std::array<std::string_view, matchPropStrings.size()> propStrings;
        for (std::size_t i = 0; i < matchPropStrings.size(); ++i)
            std::tie(props[i], propStrings[i]) = matchPropStrings[i];
        return std::pair{props, propStrings};
    }(std::to_array<std::pair<eRuleProperty, std::string_view>>({
        {RULE_PROP_CLASS, "class"},                                        //
        {RULE_PROP_TITLE, "title"},                                        //
        {RULE_PROP_INITIAL_CLASS, "initial_class"},                        //
        {RULE_PROP_INITIAL_TITLE, "initial_title"},                        //
        {RULE_PROP_FLOATING, "float"},                                     //
        {RULE_PROP_TAG, "tag"},                                            //
        {RULE_PROP_XWAYLAND, "xwayland"},                                  //
        {RULE_PROP_FULLSCREEN, "fullscreen"},                              //
        {RULE_PROP_PINNED, "pin"},                                         //
        {RULE_PROP_FOCUS, "focus"},                                        //
        {RULE_PROP_GROUP, "group"},                                        //
        {RULE_PROP_MODAL, "modal"},                                        //
        {RULE_PROP_FULLSCREENSTATE_INTERNAL, "fullscreen_state_internal"}, //
        {RULE_PROP_FULLSCREENSTATE_CLIENT, "fullscreen_state_client"},     //
        {RULE_PROP_ON_WORKSPACE, "workspace"},                             //
        {RULE_PROP_CONTENT, "content"},                                    //
        {RULE_PROP_XDG_TAG, "xdg_tag"},                                    //
        {RULE_PROP_NAMESPACE, "namespace"},                                //
    }));

constexpr auto RULE_ENGINES =
    [](auto ruleEngines) {
        std::ranges::sort(ruleEngines, {}, [](auto pair) { return pair.first; });
        return ruleEngines;
    }(std::to_array<std::pair<eRuleProperty, eRuleMatchEngine>>({
        {RULE_PROP_CLASS, RULE_MATCH_ENGINE_REGEX},                  //
        {RULE_PROP_TITLE, RULE_MATCH_ENGINE_REGEX},                  //
        {RULE_PROP_INITIAL_CLASS, RULE_MATCH_ENGINE_REGEX},          //
        {RULE_PROP_INITIAL_TITLE, RULE_MATCH_ENGINE_REGEX},          //
        {RULE_PROP_FLOATING, RULE_MATCH_ENGINE_BOOL},                //
        {RULE_PROP_TAG, RULE_MATCH_ENGINE_TAG},                      //
        {RULE_PROP_XWAYLAND, RULE_MATCH_ENGINE_BOOL},                //
        {RULE_PROP_FULLSCREEN, RULE_MATCH_ENGINE_BOOL},              //
        {RULE_PROP_PINNED, RULE_MATCH_ENGINE_BOOL},                  //
        {RULE_PROP_FOCUS, RULE_MATCH_ENGINE_BOOL},                   //
        {RULE_PROP_GROUP, RULE_MATCH_ENGINE_BOOL},                   //
        {RULE_PROP_MODAL, RULE_MATCH_ENGINE_BOOL},                   //
        {RULE_PROP_FULLSCREENSTATE_INTERNAL, RULE_MATCH_ENGINE_INT}, //
        {RULE_PROP_FULLSCREENSTATE_CLIENT, RULE_MATCH_ENGINE_INT},   //
        {RULE_PROP_ON_WORKSPACE, RULE_MATCH_ENGINE_WORKSPACE},       //
        {RULE_PROP_CONTENT, RULE_MATCH_ENGINE_REGEX},                //
        {RULE_PROP_XDG_TAG, RULE_MATCH_ENGINE_REGEX},                //
        {RULE_PROP_NAMESPACE, RULE_MATCH_ENGINE_REGEX},              //
        {RULE_PROP_EXEC_TOKEN, RULE_MATCH_ENGINE_REGEX},             //
        {RULE_PROP_EXEC_PID, RULE_MATCH_ENGINE_INT},                 //
    }));

std::span<const std::string_view> Rule::allMatchPropStrings() {
    return MATCH_PROP_STRINGS.second;
}

std::optional<eRuleProperty> Rule::matchPropFromString(std::string_view s) {
    const auto IT = std::ranges::lower_bound(MATCH_PROP_STRINGS.second, s);
    if (IT == MATCH_PROP_STRINGS.second.end() || *IT != s)
        return std::nullopt;

    return MATCH_PROP_STRINGS.first[std::distance(MATCH_PROP_STRINGS.second.begin(), IT)];
}

IRule::IRule(const std::string& name) : m_name(name) {
    ;
}

void IRule::registerMatch(eRuleProperty p, const std::string& s) {
    const auto IT = std::ranges::lower_bound(RULE_ENGINES, p, {}, [](auto pair) { return pair.first; });
    if (IT == RULE_ENGINES.end() || IT->first != p) {
        Log::logger->log(Log::ERR, "BUG THIS: IRule: RULE_ENGINES does not contain rule idx {}", sc<std::underlying_type_t<eRuleProperty>>(p));
        return;
    }

    switch (IT->second) {
        case RULE_MATCH_ENGINE_REGEX: m_matchEngines[p] = makeUnique<CRegexMatchEngine>(s); break;
        case RULE_MATCH_ENGINE_BOOL: m_matchEngines[p] = makeUnique<CBoolMatchEngine>(s); break;
        case RULE_MATCH_ENGINE_INT: m_matchEngines[p] = makeUnique<CIntMatchEngine>(s); break;
        case RULE_MATCH_ENGINE_WORKSPACE: m_matchEngines[p] = makeUnique<CWorkspaceMatchEngine>(s); break;
        case RULE_MATCH_ENGINE_TAG: m_matchEngines[p] = makeUnique<CTagMatchEngine>(s); break;
    }

    m_mask |= p;
}

std::underlying_type_t<eRuleProperty> IRule::getPropertiesMask() {
    return m_mask;
}

void IRule::setEnabled(bool enable) {
    m_enabled = enable;
}

bool IRule::isEnabled() const {
    return m_enabled;
}

bool IRule::has(eRuleProperty p) {
    return m_matchEngines.contains(p);
}

bool IRule::canMatch() const {
    return !m_matchEngines.empty() && m_enabled;
}

bool IRule::matches(eRuleProperty p, const std::string& s) {
    if (!has(p))
        return false;

    return m_matchEngines[p]->match(s);
}

bool IRule::matches(eRuleProperty p, bool b) {
    if (!has(p))
        return false;

    return m_matchEngines[p]->match(b);
}

const std::string& IRule::name() {
    return m_name;
}

void IRule::markAsExecRule(const std::string& token, uint64_t pid, bool persistent) {
    m_execData.isExecRule       = true;
    m_execData.isExecPersistent = persistent;
    m_execData.token            = token;
    m_execData.pid              = pid;
    m_execData.expiresAt        = Time::steadyNow() + std::chrono::minutes(1);
}

bool IRule::isExecRule() {
    return m_execData.isExecRule;
}

bool IRule::isExecPersistent() {
    return m_execData.isExecPersistent;
}

bool IRule::execExpired() {
    return Time::steadyNow() > m_execData.expiresAt;
}

const std::string& IRule::execToken() {
    return m_execData.token;
}
