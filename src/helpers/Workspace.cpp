#include "Workspace.hpp"
#include "../Compositor.hpp"

CWorkspace::CWorkspace(int monitorID) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitorID);

    if (!PMONITOR) {
        Debug::log(ERR, "Attempted a creation of CWorkspace with an invalid monitor?");
        return;
    }

    m_iMonitorID = monitorID;
    
    m_pWlrHandle = wlr_ext_workspace_handle_v1_create(PMONITOR->pWLRWorkspaceGroupHandle);

    // set geometry here cuz we can
    wl_array_init(&m_wlrCoordinateArr);
    *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.x;
    *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.y;
    wlr_ext_workspace_handle_v1_set_coordinates(m_pWlrHandle, &m_wlrCoordinateArr);
}

CWorkspace::~CWorkspace() {
    if (m_pWlrHandle) {
        wlr_ext_workspace_handle_v1_set_active(m_pWlrHandle, false);
        wlr_ext_workspace_handle_v1_destroy(m_pWlrHandle);
        m_pWlrHandle = nullptr;
    }
}