#pragma once

#include "../AbstractWorkspace.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace Workspace {
    std::string selector(const IAbstractWorkspace& workspace);

    class CQuery {
      public:
        CQuery()  = default;
        ~CQuery() = default;

        CQuery&& numbered(SWorkspaceNumberedID id) &&;
        CQuery&& identity(WorkspaceID id, std::string_view address, eWorkspaceType type) &&;
        CQuery&& address(std::string_view address) &&;
        CQuery&& input(std::string_view input) &&;

        bool     matches(const IAbstractWorkspace& workspace) const;

      private:
        std::optional<WorkspaceID>    m_identity;
        std::optional<std::string>    m_address;
        std::optional<std::string>    m_input;
        std::optional<eWorkspaceType> m_type;
    };
}
