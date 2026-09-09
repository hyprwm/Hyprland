#pragma once

#include "Statement.hpp"

#include <cstdint>

namespace Workspace::Filter {
    class CIDStatement final : public IStatement {
      public:
        explicit CIDStatement(uint32_t id);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        uint32_t m_id = 0;
    };
}
