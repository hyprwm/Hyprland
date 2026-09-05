#pragma once

#include "Statement.hpp"

#include <string>

namespace Workspace::Filter {
    class CDisplayNameSuffixStatement final : public IStatement {
      public:
        explicit CDisplayNameSuffixStatement(std::string suffix);

        bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const override;

      private:
        std::string m_suffix;
    };
}
