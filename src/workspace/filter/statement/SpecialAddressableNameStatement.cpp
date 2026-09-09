#include "SpecialAddressableNameStatement.hpp"

#include "../../AbstractWorkspace.hpp"

#include <utility>

using namespace Workspace;
using namespace Workspace::Filter;

CSpecialAddressableNameStatement::CSpecialAddressableNameStatement(std::string name) : m_name(std::move(name)) {
    ;
}

bool CSpecialAddressableNameStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    return workspace.type() == eWorkspaceType::SPECIAL && workspace.addressableName() == m_name;
}
