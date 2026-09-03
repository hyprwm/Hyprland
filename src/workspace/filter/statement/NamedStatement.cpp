#include "NamedStatement.hpp"

#include "../../AbstractWorkspace.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CNamedStatement::CNamedStatement(bool named) : m_named(named) {
    ;
}

bool CNamedStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    const auto ID = workspace.id();
    return (workspace.type() == eWorkspaceType::NORMAL && std::holds_alternative<SWorkspaceSpecialID>(ID)) == m_named;
}
