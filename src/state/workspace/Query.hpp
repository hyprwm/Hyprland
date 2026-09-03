#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../workspace/query/Query.hpp"

#include <string_view>

namespace State::Workspace {
    class IStateTracker;

    class CQuery {
      public:
        explicit CQuery(const IStateTracker& tracker);
        ~CQuery() = default;

        CQuery(const CQuery&) = delete;
        CQuery(CQuery&)       = delete;
        CQuery(CQuery&&)      = delete;

        CQuery&&     numbered(::Workspace::SWorkspaceNumberedID id) &&;
        CQuery&&     identity(::Workspace::WorkspaceID id, std::string_view address, ::Workspace::eWorkspaceType type) &&;
        CQuery&&     address(std::string_view address) &&;
        CQuery&&     input(std::string_view input) &&;

        PHLWORKSPACE run() &&;

      private:
        ::Workspace::CQuery  m_query;
        const IStateTracker& m_tracker;
    };
}
