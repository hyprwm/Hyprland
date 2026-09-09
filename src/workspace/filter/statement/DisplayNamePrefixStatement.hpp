#pragma once

#include "Statement.hpp"

#include <string>

namespace Workspace::Filter {
    class CDisplayNamePrefixStatement final : public IStatement {
      public:
        explicit CDisplayNamePrefixStatement(std::string prefix);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        std::string m_prefix;
    };
}
