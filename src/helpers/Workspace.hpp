#pragma once

#include "../defines.hpp"

class CWorkspace {
public:
    CWorkspace(int monitorID);
    ~CWorkspace();

    int             m_iID = -1;
    uint64_t        m_iMonitorID = -1;
    bool            m_bHasFullscreenWindow = false;

    wlr_ext_workspace_handle_v1* m_pWlrHandle = nullptr;

    wl_array        m_wlrCoordinateArr;
};