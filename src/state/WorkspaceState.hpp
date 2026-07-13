#pragma once

#include "WorkspaceStateTracker.hpp"

#include "../SharedDefs.hpp"

#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>

namespace State {
    class CWorkspaceStateTracker : public IWorkspaceStateTracker {
      public:
        CWorkspaceStateTracker()                   = default;
        virtual ~CWorkspaceStateTracker() override = default;

        virtual const std::vector<PHLWORKSPACEREF>& workspaceRefs() const override;
        virtual std::vector<SWorkspaceQueryable>    queryableWorkspaces() const override;
        auto                                        workspaces() const {
            return std::views::filter(m_workspaces, [](const auto& e) { return !!e; });
        }
        std::vector<PHLWORKSPACE>  workspacesCopy() const;

        void                       add(PHLWORKSPACE w);
        void                       clear();

        [[nodiscard]] PHLWORKSPACE create(const WORKSPACEID& id, const MONITORID& monid, const std::string& name = "", bool isEmpty = true);
        WORKSPACEID                nextAvailableNamedWorkspace() const;
        WORKSPACEID                newSpecialID() const;
        bool                       isSpecial(const WORKSPACEID& id) const;
        bool                       idOutOfBounds(const WORKSPACEID& id) const;

        void                       rememberWorkspaceForMonitor(const std::string& monitor, WORKSPACEID workspace);
        std::optional<WORKSPACEID> rememberedWorkspaceForMonitor(const std::string& monitor) const;

      private:
        std::vector<PHLWORKSPACEREF>                 m_workspaces;
        std::unordered_map<std::string, WORKSPACEID> m_seenMonitorWorkspaceMap;
    };

    UP<CWorkspaceStateTracker>& workspaceState();
}
