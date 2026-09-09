#pragma once

#include "Statement.hpp"

#include <string>

namespace Workspace::Filter {
    class CSpecialAddressableNameStatement final : public IStatement {
      public:
        explicit CSpecialAddressableNameStatement(std::string name);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        std::string m_name;
    };
}
