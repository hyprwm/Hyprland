#include "WorkspaceMatchEngine.hpp"
#include "../../Workspace.hpp"

using namespace Desktop::Rule;

CWorkspaceMatchEngine::CWorkspaceMatchEngine(const std::string& s) : m_value(s) {
    ;
}

bool CWorkspaceMatchEngine::match(PHLWORKSPACE ws) {
    return ws->matchesStaticSelector(m_value);
}
