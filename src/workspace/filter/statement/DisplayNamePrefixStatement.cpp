#include "DisplayNamePrefixStatement.hpp"

#include "../../AbstractWorkspace.hpp"

#include <utility>

using namespace Workspace;
using namespace Workspace::Filter;

CDisplayNamePrefixStatement::CDisplayNamePrefixStatement(std::string prefix) : m_prefix(std::move(prefix)) {
    ;
}

bool CDisplayNamePrefixStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    return workspace.displayName().starts_with(m_prefix);
}
