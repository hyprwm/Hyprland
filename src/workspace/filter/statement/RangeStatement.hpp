#pragma once

#include "Statement.hpp"

#include <cstdint>

namespace Workspace::Filter {
    class CRangeStatement final : public IStatement {
      public:
        CRangeStatement(uint32_t from, uint32_t to);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        uint32_t m_from = 0;
        uint32_t m_to   = 0;
    };
}
