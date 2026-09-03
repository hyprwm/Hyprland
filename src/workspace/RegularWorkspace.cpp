#include "RegularWorkspace.hpp"

#include "../config/shared/workspace/WorkspaceRule.hpp"

using namespace Workspace;

CRegularWorkspace::CRegularWorkspace(WorkspaceID id, PHLMONITOR monitor, std::string displayName, std::string address, bool isEmpty) :
    CHLWorkspace(std::move(id), std::move(monitor), std::move(displayName), std::move(address), eWorkspaceType::NORMAL, isEmpty) {
    ;
}

PHLWORKSPACE CRegularWorkspace::create(SWorkspaceNumberedID id, PHLMONITOR monitor, std::string name, bool isEmpty) {
    if (id.value == 0)
        return nullptr;

    if (name.empty())
        name = std::to_string(id.value);

    auto workspace = makeShared<CRegularWorkspace>(id, std::move(monitor), std::move(name), std::to_string(id.value), isEmpty);
    workspace->init(workspace);
    return workspace;
}

PHLWORKSPACE CRegularWorkspace::createNamed(PHLMONITOR monitor, std::string address, std::string displayName, bool isEmpty) {
    if (address.empty())
        return nullptr;

    if (displayName.empty())
        displayName = address;

    auto workspace = makeShared<CRegularWorkspace>(SWorkspaceSpecialID{}, std::move(monitor), std::move(displayName), std::move(address), isEmpty);
    workspace->init(workspace);
    return workspace;
}

void CRegularWorkspace::setPersistent(bool persistent) {
    if (m_persistent == persistent)
        return;

    m_persistent = persistent;

    if (persistent)
        m_selfPersistent = m_self.lock();
    else
        m_selfPersistent.reset();
}

bool CRegularWorkspace::isPersistent() const {
    return m_persistent;
}

void CRegularWorkspace::applyTypeSpecificRules(const Config::CWorkspaceRule& rule) {
    setPersistent(rule.m_isPersistent.value_or(false));
}
