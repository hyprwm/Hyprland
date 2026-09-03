#include "IDStatement.hpp"

#include "../../AbstractWorkspace.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CIDStatement::CIDStatement(uint32_t id) : m_id(id) {
    ;
}

bool CIDStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    const auto ID       = workspace.id();
    const auto NUMBERED = std::get_if<SWorkspaceNumberedID>(&ID);
    return NUMBERED && NUMBERED->value == m_id;
}
