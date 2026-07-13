#include "WorkspaceRuleManager.hpp"

#include "../../../Compositor.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../state/MonitorState.hpp"

#include <hyprutils/string/String.hpp>

using namespace Config;
using namespace Hyprutils::String;

UP<CWorkspaceRuleManager>& Config::workspaceRuleMgr() {
    static UP<CWorkspaceRuleManager> p = makeUnique<CWorkspaceRuleManager>();
    return p;
}

void CWorkspaceRuleManager::clear() {
    m_rules.clear();
}

SP<CWorkspaceRule> CWorkspaceRuleManager::add(CWorkspaceRule&& x) {
    return m_rules.emplace_back(makeShared<CWorkspaceRule>(std::move(x)));
}

SP<CWorkspaceRule> CWorkspaceRuleManager::replaceOrAdd(CWorkspaceRule&& x) {
    auto it = std::ranges::find_if(m_rules, [&x](const auto& r) { return r->isEnabled() && r->m_workspaceString == x.m_workspaceString; });
    if (it == m_rules.end())
        return add(std::move(x));

    (*it)->mergeLeft(x);
    return *it;
}

std::optional<CWorkspaceRule> CWorkspaceRuleManager::getWorkspaceRuleFor(PHLWORKSPACE workspace) {
    bool           any = false;

    CWorkspaceRule mergedRule;
    for (auto const& rule : m_rules) {
        if (!rule->isEnabled())
            continue;

        if (!workspace->matchesStaticSelector(rule->m_workspaceString))
            continue;

        mergedRule.mergeLeft(*rule);
        any = true;
    }

    if (!any)
        return std::nullopt;

    return mergedRule;
}

std::string CWorkspaceRuleManager::getDefaultWorkspaceFor(const Monitor::IMonitorIdentifiable& monitor) {
    for (auto const& rule : m_rules) {
        if (!rule->isEnabled())
            continue;

        if (!rule->m_isDefault.value_or(false))
            continue;

        if (monitor.matchesStaticSelector(rule->m_monitor))
            return rule->m_workspaceString;
    }
    return "";
}

PHLMONITOR CWorkspaceRuleManager::getBoundMonitorForWS(const std::string& wsname) {
    auto monitor = getBoundMonitorStringForWS(wsname);
    if (monitor.starts_with("desc:"))
        return State::monitorState()->query().description(trim(monitor.substr(5))).run();
    else
        return State::monitorState()->query().name(monitor).run();
}

std::string CWorkspaceRuleManager::getBoundMonitorStringForWS(const std::string& wsname) {
    for (auto const& wr : m_rules) {
        if (!wr->isEnabled())
            continue;
        const auto WSNAME = wr->m_workspaceName.starts_with("name:") ? wr->m_workspaceName.substr(5) : wr->m_workspaceName;
        if (WSNAME == wsname)
            return wr->m_monitor;
    }

    return "";
}

const std::vector<SP<CWorkspaceRule>>& CWorkspaceRuleManager::getAllWorkspaceRules() {
    return m_rules;
}
