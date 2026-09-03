#pragma once

#include "Statement.hpp"

#include <string>

namespace Workspace::Filter {
    class CNamedAddressableNameStatement final : public IStatement {
      public:
        explicit CNamedAddressableNameStatement(std::string name);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        std::string m_name;
    };
}
