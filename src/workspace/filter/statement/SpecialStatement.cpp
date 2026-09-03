#include "SpecialStatement.hpp"

#include "../../AbstractWorkspace.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CSpecialStatement::CSpecialStatement(bool special) : m_special(special) {
    ;
}

bool CSpecialStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    return (workspace.type() == eWorkspaceType::SPECIAL) == m_special;
}
