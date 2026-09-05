#include "DisplayNameSuffixStatement.hpp"

#include "../../AbstractWorkspace.hpp"

#include <utility>

using namespace Workspace;
using namespace Workspace::Filter;

CDisplayNameSuffixStatement::CDisplayNameSuffixStatement(std::string suffix) : m_suffix(std::move(suffix)) {
    ;
}

bool CDisplayNameSuffixStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    return workspace.displayName().ends_with(m_suffix);
}
