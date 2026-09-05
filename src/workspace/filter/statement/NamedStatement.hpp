#pragma once

#include "Statement.hpp"

namespace Workspace::Filter {
    class CNamedStatement final : public IStatement {
      public:
        explicit CNamedStatement(bool named);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        bool m_named = false;
    };
}
