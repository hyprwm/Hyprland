#include "WindowCountStatement.hpp"

using namespace Workspace;
using namespace Workspace::Filter;

CWindowCountStatement::CWindowCountStatement(SWindowCountOptions options, uint32_t from, uint32_t to) : m_options(options), m_from(from), m_to(to) {
    ;
}

bool CWindowCountStatement::matches(const IAbstractWorkspace& workspace, const IDataSource* dataSource) const {
    if (!dataSource)
        return false;

    const auto COUNT = dataSource->windowCount(workspace, m_options);
    return COUNT >= 0 && sc<uint32_t>(COUNT) >= m_from && sc<uint32_t>(COUNT) <= m_to;
}
