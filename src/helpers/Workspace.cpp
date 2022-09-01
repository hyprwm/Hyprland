#include "Workspace.hpp"
#include "../Compositor.hpp"

CWorkspace::CWorkspace(int monitorID, std::string name, bool special) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitorID);

    if (!PMONITOR) {
        Debug::log(ERR, "Attempted a creation of CWorkspace with an invalid monitor?");
        return;
    }

    m_iMonitorID = monitorID;
    m_szName = name;
    m_bIsSpecialWorkspace = special;
    
    if (!special) {
        m_pWlrHandle = wlr_ext_workspace_handle_v1_create(PMONITOR->pWLRWorkspaceGroupHandle);

        // set geometry here cuz we can
        wl_array_init(&m_wlrCoordinateArr);
        *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.x;
        *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.y;
        wlr_ext_workspace_handle_v1_set_coordinates(m_pWlrHandle, &m_wlrCoordinateArr);
        wlr_ext_workspace_handle_v1_set_hidden(m_pWlrHandle, false);
        wlr_ext_workspace_handle_v1_set_urgent(m_pWlrHandle, false);
    }

    m_vRenderOffset.m_pWorkspace = this;
    m_vRenderOffset.create(AVARTYPE_VECTOR, special ? g_pConfigManager->getAnimationPropertyConfig("specialWorkspace") : g_pConfigManager->getAnimationPropertyConfig("workspaces"), nullptr, AVARDAMAGE_ENTIRE);
    m_fAlpha.m_pWorkspace = this;
    m_fAlpha.create(AVARTYPE_FLOAT, special ? g_pConfigManager->getAnimationPropertyConfig("specialWorkspace") : g_pConfigManager->getAnimationPropertyConfig("workspaces"), nullptr, AVARDAMAGE_ENTIRE);
    m_fAlpha.setValueAndWarp(255.f);

    g_pEventManager->postEvent({"createworkspace", m_szName}, true);
}

CWorkspace::~CWorkspace() {
    m_vRenderOffset.unregister();

    Debug::log(LOG, "Destroying workspace ID %d", m_iID);

    if (m_pWlrHandle) {
        wlr_ext_workspace_handle_v1_set_active(m_pWlrHandle, false);
        wlr_ext_workspace_handle_v1_destroy(m_pWlrHandle);
        m_pWlrHandle = nullptr;
    }

    g_pEventManager->postEvent({"destroyworkspace", m_szName}, true);
}

void CWorkspace::startAnim(bool in, bool left, bool instant) {
    const auto ANIMSTYLE = m_fAlpha.m_pConfig->pValues->internalStyle;

    if (ANIMSTYLE == "fade") {
        m_vRenderOffset.setValueAndWarp(Vector2D(0, 0)); // fix a bug, if switching from slide -> fade.

        if (in) {
            m_fAlpha.setValueAndWarp(0.f);
            m_fAlpha = 255.f;
        } else {
            m_fAlpha.setValueAndWarp(255.f);
            m_fAlpha = 0.f;
        }
    } else if (ANIMSTYLE == "slidevert") {
        // fallback is slide
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

        m_fAlpha.setValueAndWarp(255.f);  // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset.setValueAndWarp(Vector2D(0, left ? PMONITOR->vecSize.y : -PMONITOR->vecSize.y));
            m_vRenderOffset = Vector2D(0, 0);
        } else {
            m_vRenderOffset = Vector2D(0, left ? -PMONITOR->vecSize.y : PMONITOR->vecSize.y);
        }
    } else {
        // fallback is slide
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

        m_fAlpha.setValueAndWarp(255.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset.setValueAndWarp(Vector2D(left ? PMONITOR->vecSize.x : -PMONITOR->vecSize.x, 0));
            m_vRenderOffset = Vector2D(0, 0);
        } else {
            m_vRenderOffset = Vector2D(left ? -PMONITOR->vecSize.x : PMONITOR->vecSize.x, 0);
        }
    }

    if (instant) {
        m_vRenderOffset.warp();
        m_fAlpha.warp();
    }
}

void CWorkspace::setActive(bool on) {
    if (m_pWlrHandle) {
        wlr_ext_workspace_handle_v1_set_active(m_pWlrHandle, on);
    }
}

void CWorkspace::moveToMonitor(const int& id) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(id);

    if (!PMONITOR || m_bIsSpecialWorkspace)
        return;

    wlr_ext_workspace_handle_v1_set_active(m_pWlrHandle, false);
    wlr_ext_workspace_handle_v1_destroy(m_pWlrHandle);

    m_pWlrHandle = wlr_ext_workspace_handle_v1_create(PMONITOR->pWLRWorkspaceGroupHandle);

    // set geometry here cuz we can
    wl_array_init(&m_wlrCoordinateArr);
    *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.x;
    *reinterpret_cast<int*>(wl_array_add(&m_wlrCoordinateArr, sizeof(int))) = (int)PMONITOR->vecPosition.y;
    wlr_ext_workspace_handle_v1_set_coordinates(m_pWlrHandle, &m_wlrCoordinateArr);
    wlr_ext_workspace_handle_v1_set_hidden(m_pWlrHandle, false);
    wlr_ext_workspace_handle_v1_set_urgent(m_pWlrHandle, false);

    wlr_ext_workspace_handle_v1_set_name(m_pWlrHandle, m_szName.c_str());
}

CWindow* CWorkspace::getLastFocusedWindow() {
    if (!g_pCompositor->windowValidMapped(m_pLastFocusedWindow) || m_pLastFocusedWindow->m_iWorkspaceID != m_iID)
        return nullptr;

    return m_pLastFocusedWindow;
}
