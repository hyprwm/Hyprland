#include "RangeStatement.hpp"

#include "../../AbstractWorkspace.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CRangeStatement::CRangeStatement(uint32_t from, uint32_t to) : m_from(from), m_to(to) {
    ;
}

bool CRangeStatement::matches(const IAbstractWorkspace& workspace, const IDataSource*) const {
    const auto ID       = workspace.id();
    const auto NUMBERED = std::get_if<SWorkspaceNumberedID>(&ID);
    return NUMBERED && NUMBERED->value >= m_from && NUMBERED->value <= m_to;
}
