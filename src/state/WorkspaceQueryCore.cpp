#include "WorkspaceQueryCore.hpp"

#include <algorithm>
#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <utility>

using namespace State;
using namespace Hyprutils::String;

CWorkspaceQueryCore::CWorkspaceQueryCore(std::span<const SWorkspaceQueryable> workspaces) : m_workspaces(workspaces) {
    ;
}

CWorkspaceQueryCore&& CWorkspaceQueryCore::id(const WORKSPACEID& id) && {
    m_id = id;
    return std::move(*this);
}

CWorkspaceQueryCore&& CWorkspaceQueryCore::name(std::string_view name) && {
    m_name = name;
    return std::move(*this);
}

CWorkspaceQueryCore&& CWorkspaceQueryCore::string(std::string_view str) && {
    m_string = str;
    return std::move(*this);
}

std::optional<size_t> CWorkspaceQueryCore::run() && {
    if (m_string) {
        if (m_string->starts_with("name:"))
            std::move(*this).name(m_string->substr(m_string->find_first_of(':') + 1));
        else if (*m_string == "special")
            std::move(*this).id(SPECIAL_WORKSPACE_START);
        else if (m_string->starts_with("special:"))
            std::move(*this).name(*m_string);
        else if (isNumber(std::string{*m_string})) {
            const auto ID = strToNumber<WORKSPACEID>(*m_string);
            if (!ID)
                return std::nullopt;

            std::move(*this).id(*ID);
        } else
            std::move(*this).name(*m_string);
    }

    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        const auto& w = m_workspaces[i];

        if (w.inert)
            continue;

        if (m_id && w.id != *m_id)
            continue;

        if (m_name && w.name != *m_name)
            continue;

        return i;
    }

    return std::nullopt;
}

bool CWorkspaceQueryCore::isSpecial(const WORKSPACEID& id) {
    return id >= SPECIAL_WORKSPACE_START && id <= -2;
}

WORKSPACEID CWorkspaceQueryCore::newSpecialID(std::span<const SWorkspaceQueryable> workspaces) {
    WORKSPACEID highest = SPECIAL_WORKSPACE_START;
    for (const auto& ws : workspaces) {
        if (ws.inert)
            continue;

        if (ws.special && ws.id > highest)
            highest = ws.id;
    }

    return highest + 1;
}

WORKSPACEID CWorkspaceQueryCore::nextAvailableNamedWorkspace(std::span<const SWorkspaceQueryable> workspaces, std::span<const WORKSPACEID> persistentWorkspaceIDs) {
    WORKSPACEID lowest = -1337 + 1;
    for (const auto& w : workspaces) {
        if (w.inert)
            continue;

        if (w.id < -1 && w.id < lowest)
            lowest = w.id;
    }

    for (const auto& id : persistentWorkspaceIDs) {
        if (id < -1 && id < lowest)
            lowest = id;
    }

    return lowest - 1;
}

bool CWorkspaceQueryCore::idOutOfBounds(std::span<const SWorkspaceQueryable> workspaces, const WORKSPACEID& id) {
    WORKSPACEID lowestID  = INT64_MAX;
    WORKSPACEID highestID = INT64_MIN;

    for (const auto& w : workspaces) {
        if (w.inert || w.special)
            continue;

        lowestID  = std::min(w.id, lowestID);
        highestID = std::max(w.id, highestID);
    }

    return std::clamp(id, lowestID, highestID) != id;
}
