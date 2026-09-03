#pragma once

#include "Statement.hpp"

namespace Workspace::Filter {
    class CFullscreenStatement final : public IStatement {
      public:
        explicit CFullscreenStatement(int state);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        int m_state = -1;
    };
}
