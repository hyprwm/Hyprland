#pragma once

#include "../WorkspaceFilter.hpp"
#include "Statement.hpp"

#include <cstdint>

namespace Workspace::Filter {
    class CWindowCountStatement final : public IStatement {
      public:
        CWindowCountStatement(SWindowCountOptions options, uint32_t from, uint32_t to);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        SWindowCountOptions m_options;
        uint32_t            m_from = 0;
        uint32_t            m_to   = 0;
    };
}
