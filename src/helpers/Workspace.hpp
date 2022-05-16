#pragma once

#include "../defines.hpp"
#include "AnimatedVariable.hpp"

class CWorkspace {
public:
    CWorkspace(int monitorID);
    ~CWorkspace();

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    int             m_iID = -1;
    std::string     m_szName = "";
    uint64_t        m_iMonitorID = -1;
    bool            m_bHasFullscreenWindow = false;

    wlr_ext_workspace_handle_v1* m_pWlrHandle = nullptr;

    wl_array        m_wlrCoordinateArr;

    // for animations
    CAnimatedVariable m_vRenderOffset;
    CAnimatedVariable m_fAlpha;

    void            startAnim(bool in, bool left);
};