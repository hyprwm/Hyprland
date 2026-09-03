#pragma once

#include "../../helpers/memory/Memory.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Workspace {
    class CHLWorkspace;
    class IAbstractWorkspace;
}

namespace Workspace::Filter {
    class IStatement;

    struct SWindowCountOptions {
        std::optional<bool> tiled;
        bool                pinned  = false;
        bool                groups  = false;
        bool                visible = false;
    };

    class IDataSource {
      public:
        virtual ~IDataSource() = default;

        virtual bool monitorMatches(const IAbstractWorkspace& workspace, std::string_view selector) const       = 0;
        virtual int  windowCount(const IAbstractWorkspace& workspace, const SWindowCountOptions& options) const = 0;
        virtual int  fullscreenState(const IAbstractWorkspace& workspace) const                                 = 0;
    };

    class CWorkspaceFilter {
      public:
        explicit CWorkspaceFilter(const std::string& filter, const IDataSource* dataSource = nullptr);
        ~CWorkspaceFilter();

        void               transform(std::vector<SP<CHLWorkspace>>& workspaces) const;
        void               transform(std::vector<SP<IAbstractWorkspace>>& workspaces) const;

        bool               matches(const IAbstractWorkspace& workspace) const;
        const std::string& error() const;

      private:
        std::vector<UP<IStatement>> m_statements;
        std::string                 m_error;
        const IDataSource*          m_dataSource = nullptr;
    };

    const IDataSource& hlDataSource();
}
