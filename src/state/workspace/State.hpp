#pragma once

#include "StateTracker.hpp"
#include "Target.hpp"

#include <ranges>
#include <string>

namespace State::Workspace {
    class CState final : public IStateTracker {
      public:
        const std::vector<PHLWORKSPACEREF>& workspaceRefs() const override;

        auto                                workspaces() const {
            return std::views::filter(m_workspaces, [](const auto& workspace) { return !!workspace; });
        }

        std::vector<PHLWORKSPACE>  workspacesCopy() const;
        std::vector<PHLWORKSPACE>  filter(const std::string& selector, std::string* error = nullptr) const;

        bool                       add(PHLWORKSPACE workspace);
        void                       clear();

        PHLWORKSPACE               find(const STarget& target) const;
        [[nodiscard]] PHLWORKSPACE create(const STarget& target, PHLMONITOR monitor, bool isEmpty = true);
        [[nodiscard]] PHLWORKSPACE createNumbered(::Workspace::SWorkspaceNumberedID id, PHLMONITOR monitor, std::string displayName = {}, bool isEmpty = true);
        [[nodiscard]] PHLWORKSPACE createNamed(std::string address, PHLMONITOR monitor, std::string displayName = {}, bool isEmpty = true);
        [[nodiscard]] PHLWORKSPACE createSpecial(std::string address, PHLMONITOR monitor, bool isEmpty = true);

      private:
        std::vector<PHLWORKSPACEREF> m_workspaces;
    };

    UP<CState>& state();
}
