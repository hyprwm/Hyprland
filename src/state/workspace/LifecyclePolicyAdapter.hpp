#pragma once

#include "LifecyclePolicy.hpp"
#include "../../desktop/DesktopTypes.hpp"

namespace State::Workspace {
    void monitorConnected(PHLMONITOR monitor);
    void monitorDisconnected(PHLMONITOR monitor);
    void workspaceDestroyed(const SWorkspaceIdentity& identity);
    void workspaceIdentityChanged(const SWorkspaceIdentity& oldIdentity, const SWorkspaceIdentity& newIdentity);
    void clearLifecyclePolicy();
}
