#pragma once

#include "Statement.hpp"

#include <string>

namespace Workspace::Filter {
    class CMonitorStatement final : public IStatement {
      public:
        explicit CMonitorStatement(std::string monitor);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        std::string m_monitor;
    };
}
