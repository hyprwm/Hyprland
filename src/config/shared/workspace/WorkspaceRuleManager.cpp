#include "WorkspaceRuleManager.hpp"

#include "../../../Compositor.hpp"
#include "../../../helpers/Monitor.hpp"

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

void CWorkspaceRuleManager::add(CWorkspaceRule&& x) {
    m_rules.emplace_back(std::move(x));
}

void CWorkspaceRuleManager::replaceOrAdd(CWorkspaceRule&& x) {
    auto it = std::ranges::find_if(m_rules, [&x](const auto& r) { return r.m_enabled && r.m_workspaceString == x.m_workspaceString; });
    if (it == m_rules.end())
        m_rules.emplace_back(std::move(x));
    else
        (*it).mergeLeft(x);
}

std::optional<CWorkspaceRule> CWorkspaceRuleManager::getWorkspaceRuleFor(PHLWORKSPACE workspace) {
    bool           any = false;

    CWorkspaceRule mergedRule;
    for (auto const& rule : m_rules) {
        if (!rule.m_enabled)
            continue;

        if (!workspace->matchesStaticSelector(rule.m_workspaceString))
            continue;

        mergedRule.mergeLeft(rule);
        any = true;
    }

    if (!any)
        return std::nullopt;

    return mergedRule;
}

std::string CWorkspaceRuleManager::getDefaultWorkspaceFor(const std::string& name) {
    for (auto other = m_rules.begin(); other != m_rules.end(); ++other) {
        if (!other->m_enabled)
            continue;

        if (other->m_isDefault) {
            if (other->m_monitor == name)
                return other->m_workspaceString;
            if (other->m_monitor.starts_with("desc:")) {
                auto const monitor = g_pCompositor->getMonitorFromDesc(trim(other->m_monitor.substr(5)));
                if (monitor && monitor->m_name == name)
                    return other->m_workspaceString;
            }
        }
    }
    return "";
}

PHLMONITOR CWorkspaceRuleManager::getBoundMonitorForWS(const std::string& wsname) {
    auto monitor = getBoundMonitorStringForWS(wsname);
    if (monitor.starts_with("desc:"))
        return g_pCompositor->getMonitorFromDesc(trim(monitor.substr(5)));
    else
        return g_pCompositor->getMonitorFromName(monitor);
}

std::string CWorkspaceRuleManager::getBoundMonitorStringForWS(const std::string& wsname) {
    for (auto const& wr : m_rules) {
        if (!wr.m_enabled)
            continue;
        const auto WSNAME = wr.m_workspaceName.starts_with("name:") ? wr.m_workspaceName.substr(5) : wr.m_workspaceName;
        if (WSNAME == wsname)
            return wr.m_monitor;
    }

    return "";
}

const std::vector<CWorkspaceRule>& CWorkspaceRuleManager::getAllWorkspaceRules() {
    return m_rules;
}