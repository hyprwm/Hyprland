#pragma once

#include "../SharedDefs.hpp"
#include "../macros.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace State {
    struct SWorkspaceQueryable {
        WORKSPACEID      id      = WORKSPACE_INVALID;
        std::string_view name    = "";
        bool             inert   = false;
        bool             special = false;
    };

    class CWorkspaceQueryCore {
      public:
        CWorkspaceQueryCore(std::span<const SWorkspaceQueryable> workspaces);
        ~CWorkspaceQueryCore() = default;

        CWorkspaceQueryCore(const CWorkspaceQueryCore&) = delete;
        CWorkspaceQueryCore(CWorkspaceQueryCore&)       = delete;
        CWorkspaceQueryCore(CWorkspaceQueryCore&&)      = delete;

        CWorkspaceQueryCore&& id(const WORKSPACEID& id) &&;
        CWorkspaceQueryCore&& name(std::string_view name) &&;
        CWorkspaceQueryCore&& string(std::string_view str) &&;

        std::optional<size_t> run() &&;

        static bool           isSpecial(const WORKSPACEID& id);
        static WORKSPACEID    newSpecialID(std::span<const SWorkspaceQueryable> workspaces);
        static WORKSPACEID    nextAvailableNamedWorkspace(std::span<const SWorkspaceQueryable> workspaces, std::span<const WORKSPACEID> persistentWorkspaceIDs = {});
        static bool           idOutOfBounds(std::span<const SWorkspaceQueryable> workspaces, const WORKSPACEID& id);

      private:
        std::span<const SWorkspaceQueryable> m_workspaces;
        std::optional<WORKSPACEID>           m_id;
        std::optional<std::string_view>      m_name, m_string;
    };
}
