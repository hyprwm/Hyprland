#include "MonitorStatement.hpp"

#include "../WorkspaceFilter.hpp"

#include <utility>

using namespace Workspace;
using namespace Workspace::Filter;

CMonitorStatement::CMonitorStatement(std::string monitor) : m_monitor(std::move(monitor)) {
    ;
}

bool CMonitorStatement::matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const {
    return dataSource && dataSource->monitorMatches(workspace, m_monitor);
}
