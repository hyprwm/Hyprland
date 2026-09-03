#include "Query.hpp"
#include "StateTracker.hpp"
#include "../../workspace/HLWorkspace.hpp"

#include <utility>

using namespace State::Workspace;

CQuery::CQuery(const IStateTracker& tracker) : m_tracker(tracker) {
    ;
}

CQuery&& CQuery::numbered(::Workspace::SWorkspaceNumberedID id) && {
    std::move(m_query).numbered(id);
    return std::move(*this);
}

CQuery&& CQuery::identity(::Workspace::WorkspaceID id, std::string_view address, ::Workspace::eWorkspaceType type) && {
    std::move(m_query).identity(std::move(id), address, type);
    return std::move(*this);
}

CQuery&& CQuery::address(std::string_view address) && {
    std::move(m_query).address(address);
    return std::move(*this);
}

CQuery&& CQuery::input(std::string_view input) && {
    std::move(m_query).input(input);
    return std::move(*this);
}

PHLWORKSPACE CQuery::run() && {
    for (const auto& reference : m_tracker.workspaceRefs()) {
        const auto WORKSPACE = reference.lock();
        if (!WORKSPACE || !m_query.matches(*WORKSPACE))
            continue;

        return WORKSPACE;
    }

    return nullptr;
}
