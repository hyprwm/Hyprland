#include "NamedAddressableNameStatement.hpp"

#include "../../AbstractWorkspace.hpp"

#include <utility>

using namespace Workspace;
using namespace Workspace::Filter;

CNamedAddressableNameStatement::CNamedAddressableNameStatement(std::string name) : m_name(std::move(name)) {
    ;
}

bool CNamedAddressableNameStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    const auto ID = workspace.id();
    return workspace.type() == eWorkspaceType::NORMAL && std::holds_alternative<SWorkspaceSpecialID>(ID) && workspace.addressableName() == m_name;
}
