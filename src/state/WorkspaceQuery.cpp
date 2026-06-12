#include "WorkspaceQuery.hpp"
#include "WorkspaceQueryCore.hpp"
#include "WorkspaceStateTracker.hpp"

#include <utility>

using namespace State;

CWorkspaceQuery::CWorkspaceQuery(const IWorkspaceStateTracker& t) : m_tracker(t) {
    ;
}

CWorkspaceQuery&& CWorkspaceQuery::id(const WORKSPACEID& id) && {
    m_id = id;
    return std::move(*this);
}

CWorkspaceQuery&& CWorkspaceQuery::name(const std::string& name) && {
    m_name = name;
    return std::move(*this);
}

CWorkspaceQuery&& CWorkspaceQuery::string(const std::string& str) && {
    m_string = str;
    return std::move(*this);
}

PHLWORKSPACE CWorkspaceQuery::run() && {
    auto queryables = m_tracker.queryableWorkspaces();
    auto core       = CWorkspaceQueryCore{queryables};

    if (m_id)
        std::move(core).id(*m_id);
    if (m_name)
        std::move(core).name(*m_name);
    if (m_string)
        std::move(core).string(*m_string);

    const auto RESULT = std::move(core).run();
    if (!RESULT)
        return nullptr;

    const auto& refs = m_tracker.workspaceRefs();
    if (*RESULT >= refs.size() || !refs[*RESULT])
        return nullptr;

    return refs[*RESULT].lock();
}
