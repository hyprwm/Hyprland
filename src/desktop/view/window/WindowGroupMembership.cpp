#include "WindowGroupMembership.hpp"

#include "Window.hpp"
#include "../Group.hpp"
#include "../../state/WindowState.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../debug/log/Logger.hpp"
#include "../../../layout/LayoutManager.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>

using namespace Desktop::View;
using namespace Hyprutils::String;

uint16_t Desktop::View::parseGroupRules(std::string_view rule, uint16_t currentRules) {
    if ((currentRules & GROUP_OVERRIDE) || trim(rule) == "group")
        return currentRules;

    CVarList2   vars(std::string{rule}, 0, 's');
    std::string previous;

    for (const auto& value : vars) {
        if (value == "group")
            continue;

        if (value == "set")
            currentRules |= GROUP_SET;
        else if (value == "new")
            currentRules |= GROUP_SET | GROUP_BARRED;
        else if (value == "lock")
            currentRules |= GROUP_LOCK;
        else if (value == "invade")
            currentRules |= GROUP_INVADE;
        else if (value == "barred")
            currentRules |= GROUP_BARRED;
        else if (value == "deny")
            currentRules |= GROUP_DENY;
        else if (value == "override")
            currentRules = GROUP_OVERRIDE;
        else if (value == "unset") {
            currentRules = GROUP_OVERRIDE;
            break;
        } else if (value == "always") {
            if (previous == "set" || previous == "group")
                currentRules |= GROUP_SET_ALWAYS;
            else if (previous == "lock")
                currentRules |= GROUP_LOCK_ALWAYS;
            else
                Log::logger->log(Log::ERR, "windowrule `group` does not support `{} always`", previous);
        }

        previous = value;
    }

    return currentRules;
}

CWindowGroupMembership::CWindowGroupMembership(CWindow& window) : m_window(window) {
    ;
}

const SP<CGroup>& CWindowGroupMembership::group() const {
    return m_group;
}

uint16_t CWindowGroupMembership::rules() const {
    return m_rules;
}

void CWindowGroupMembership::applyRule(std::string_view rule) {
    m_rules = parseGroupRules(rule, m_rules);
}

bool CWindowGroupMembership::canBeGroupedInto(const SP<CGroup>& group) const {
    if (!group || m_window.backend().traits().overrideRedirect)
        return false;

    static auto ALLOWGROUPMERGE       = CConfigValue<Config::INTEGER>("group:merge_groups_on_drag");
    const bool  isGroup               = !!m_group;
    const bool  disallowDragIntoGroup = g_layoutManager->dragController()->wasDraggingWindow() && isGroup && !sc<bool>(*ALLOWGROUPMERGE);

    return !Desktop::windowState()->groupsLocked()                                                       // global group lock disengaged
        && ((m_rules & GROUP_INVADE && (m_window.m_state & WINDOW_STATE_FIRST_MAP) != WINDOW_STATE_NONE) // window ignores local group locks, or
            || (!group->locked()                                                                         // target unlocked
                && !(m_group && m_group->locked())))                                                     // source unlocked or isn't group
        && !(m_rules & GROUP_DENY)                                                                       // source is not denied entry
        && !(m_rules & GROUP_BARRED && (m_window.m_state & WINDOW_STATE_FIRST_MAP) != WINDOW_STATE_NONE) // group rule doesn't prevent adding window
        && !disallowDragIntoGroup;                                                                       // config allows groups to be merged
}

void CWindowGroupMembership::attach(const SP<CGroup>& group) {
    m_group = group;
}

void CWindowGroupMembership::detach() {
    m_group.reset();
}
