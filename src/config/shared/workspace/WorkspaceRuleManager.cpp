#include "WorkspaceRuleManager.hpp"

#include "../../../Compositor.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../state/MonitorState.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

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

std::string CWorkspaceRuleManager::getDefaultWorkspaceFor(const Monitor::IMonitorAddressable& monitor) {
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

std::optional<PHLMONITOR> CWorkspaceRuleManager::getBoundMonitorForWS(const ::Workspace::WorkspaceID& id, ::Workspace::eWorkspaceType type, std::string_view address) {
    const auto MONITOR = getBoundMonitorStringForWS(id, type, address);
    if (MONITOR.empty())
        return std::nullopt;
    const auto RESULT =
        MONITOR.starts_with("desc:") ? State::monitorState()->query().description(trim(MONITOR.substr(5))).run() : State::monitorState()->query().name(MONITOR).run();
    return RESULT ? std::optional{RESULT} : std::nullopt;
}

std::string CWorkspaceRuleManager::getBoundMonitorStringForWS(const std::string& wsname) {
    if (wsname == "special" || wsname.starts_with("special:"))
        return getBoundMonitorStringForWS(::Workspace::SWorkspaceSpecialID{}, ::Workspace::eWorkspaceType::SPECIAL, wsname == "special" ? "special:special" : wsname);
    if (isNumber(wsname)) {
        const auto ID = strToNumber<::Workspace::WorkspaceIDContainer>(wsname);
        if (!ID || *ID == 0)
            return "";
        return getBoundMonitorStringForWS(::Workspace::SWorkspaceNumberedID{*ID}, ::Workspace::eWorkspaceType::NORMAL, wsname);
    }
    return getBoundMonitorStringForWS(::Workspace::SWorkspaceSpecialID{}, ::Workspace::eWorkspaceType::NORMAL,
                                      wsname.starts_with("name:") ? std::string_view{wsname}.substr(5) : std::string_view{wsname});
}

std::string CWorkspaceRuleManager::getBoundMonitorStringForWS(const ::Workspace::WorkspaceID& id, ::Workspace::eWorkspaceType type, std::string_view wsname) {
    for (auto const& wr : m_rules) {
        if (!wr->isEnabled())
            continue;

        const auto& SELECTOR = wr->m_workspaceString;
        if (SELECTOR.contains('[') && !SELECTOR.starts_with("name:") && !SELECTOR.starts_with("special:"))
            continue;

        ::Workspace::WorkspaceID ruleID   = ::Workspace::SWorkspaceSpecialID{};
        auto                     ruleType = ::Workspace::eWorkspaceType::NORMAL;
        std::string_view         address  = SELECTOR;
        if (SELECTOR.starts_with("name:"))
            address.remove_prefix(5);
        else if (SELECTOR == "special") {
            ruleType = ::Workspace::eWorkspaceType::SPECIAL;
            address  = "special:special";
        } else if (SELECTOR.starts_with("special:"))
            ruleType = ::Workspace::eWorkspaceType::SPECIAL;
        else if (isNumber(SELECTOR)) {
            const auto NUMBER = strToNumber<::Workspace::WorkspaceIDContainer>(SELECTOR);
            if (!NUMBER || *NUMBER == 0)
                continue;
            ruleID = ::Workspace::SWorkspaceNumberedID{*NUMBER};
        }

        const bool NUMBERED = std::holds_alternative<::Workspace::SWorkspaceNumberedID>(ruleID);
        if (ruleID == id && ruleType == type && (NUMBERED || address == wsname))
            return wr->m_monitor;
    }

    return "";
}

const std::vector<SP<CWorkspaceRule>>& CWorkspaceRuleManager::getAllWorkspaceRules() {
    return m_rules;
}
