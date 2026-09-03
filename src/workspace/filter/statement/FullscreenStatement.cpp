#include "FullscreenStatement.hpp"

#include "../WorkspaceFilter.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CFullscreenStatement::CFullscreenStatement(int state) : m_state(state) {
    ;
}

bool CFullscreenStatement::matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const {
    return dataSource && dataSource->fullscreenState(workspace) == m_state;
}
