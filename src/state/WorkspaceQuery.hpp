#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../SharedDefs.hpp"

#include <optional>
#include <string>

namespace State {
    class IWorkspaceStateTracker;

    class CWorkspaceQuery {
      public:
        CWorkspaceQuery(const IWorkspaceStateTracker&);
        ~CWorkspaceQuery() = default;

        CWorkspaceQuery(const CWorkspaceQuery&) = delete;
        CWorkspaceQuery(CWorkspaceQuery&)       = delete;
        CWorkspaceQuery(CWorkspaceQuery&&)      = delete;

        CWorkspaceQuery&& id(const WORKSPACEID& id) &&;
        CWorkspaceQuery&& name(const std::string& name) &&;
        CWorkspaceQuery&& string(const std::string& str) &&;

        PHLWORKSPACE      run() &&;

      private:
        std::optional<WORKSPACEID>    m_id;
        std::optional<std::string>    m_name, m_string;
        const IWorkspaceStateTracker& m_tracker;
    };
}
