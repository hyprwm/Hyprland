#pragma once

#include "Statement.hpp"

namespace Workspace::Filter {
    class CSpecialStatement final : public IStatement {
      public:
        explicit CSpecialStatement(bool special);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        bool m_special = false;
    };
}
