#pragma once

#include "../../../helpers/memory/Memory.hpp"

namespace Workspace {
    class IAbstractWorkspace;
}

namespace Workspace::Filter {
    class IDataSource;

    class IStatement {
      public:
        virtual ~IStatement() = default;

        virtual bool matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const = 0;

      protected:
        IStatement() = default;
    };
};
