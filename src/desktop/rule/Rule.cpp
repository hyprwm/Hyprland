#include "Rule.hpp"
#include "../../debug/Log.hpp"
#include <re2/re2.h>

#include "matchEngine/RegexMatchEngine.hpp"
#include "matchEngine/BoolMatchEngine.hpp"
#include "matchEngine/IntMatchEngine.hpp"
#include "matchEngine/WorkspaceMatchEngine.hpp"
#include "matchEngine/TagMatchEngine.hpp"

using namespace Desktop;
using namespace Desktop::Rule;

static const std::unordered_map<eRuleProperty, std::string> MATCH_PROP_STRINGS = {
    {RULE_PROP_CLASS, "class"},                                       //
    {RULE_PROP_TITLE, "title"},                                       //
    {RULE_PROP_INITIAL_CLASS, "initial_class"},                       //
    {RULE_PROP_INITIAL_TITLE, "initial_title"},                       //
    {RULE_PROP_FLOATING, "floating"},                                 //
    {RULE_PROP_TAG, "tag"},                                           //
    {RULE_PROP_XWAYLAND, "xwayland"},                                 //
    {RULE_PROP_FULLSCREEN, "fullscreen"},                             //
    {RULE_PROP_PINNED, "pinned"},                                     //
    {RULE_PROP_FOCUS, "focus"},                                       //
    {RULE_PROP_GROUP, "group"},                                       //
    {RULE_PROP_MODAL, "modal"},                                       //
    {RULE_PROP_FULLSCREENSTATE_INTERNAL, "fullscreenstate_internal"}, //
    {RULE_PROP_FULLSCREENSTATE_CLIENT, "fullscreenstate_client"},     //
    {RULE_PROP_ON_WORKSPACE, "on_workspace"},                         //
    {RULE_PROP_CONTENT, "content"},                                   //
    {RULE_PROP_XDG_TAG, "xdg_tag"},                                   //
    {RULE_PROP_NAMESPACE, "namespace"},                               //
};

static const std::unordered_map<eRuleProperty, eRuleMatchEngine> RULE_ENGINES = {
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
    {RULE_PROP_CONTENT, RULE_MATCH_ENGINE_INT},                  //
    {RULE_PROP_XDG_TAG, RULE_MATCH_ENGINE_REGEX},                //
    {RULE_PROP_NAMESPACE, RULE_MATCH_ENGINE_REGEX},              //
    {RULE_PROP_EXEC_TOKEN, RULE_MATCH_ENGINE_REGEX},             //
};

const std::vector<std::string>& Rule::allMatchPropStrings() {
    static std::vector<std::string> strings;
    static bool                     once = true;
    if (once) {
        for (const auto& [k, v] : MATCH_PROP_STRINGS) {
            strings.emplace_back(v);
        }
        once = false;
    }
    return strings;
}

std::optional<eRuleProperty> Rule::matchPropFromString(const std::string_view& s) {
    const auto IT = std::ranges::find_if(MATCH_PROP_STRINGS, [&s](const auto& el) { return el.second == s; });
    if (IT == MATCH_PROP_STRINGS.end())
        return std::nullopt;

    return IT->first;
}

std::optional<eRuleProperty> Rule::matchPropFromString(const std::string& s) {
    return matchPropFromString(std::string_view{s});
}

IRule::IRule(const std::string& name) : m_name(name) {
    ;
}

void IRule::registerMatch(eRuleProperty p, const std::string& s) {
    if (!RULE_ENGINES.contains(p)) {
        Debug::log(ERR, "BUG THIS: IRule: RULE_ENGINES does not contain rule idx {}", sc<std::underlying_type_t<eRuleProperty>>(p));
        return;
    }

    switch (RULE_ENGINES.at(p)) {
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

bool IRule::has(eRuleProperty p) {
    return m_matchEngines.contains(p);
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

void IRule::markAsExecRule(const std::string& token, bool persistent) {
    m_execData.isExecRule       = true;
    m_execData.isExecPersistent = persistent;
    m_execData.token            = token;
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
