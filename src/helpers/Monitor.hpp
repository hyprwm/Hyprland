#pragma once

#include "../defines.hpp"
#include <deque>
#include "WLClasses.hpp"
#include <vector>
#include <array>
#include <memory>

struct SMonitor {
    Vector2D    vecPosition         = Vector2D(0,0);
    Vector2D    vecSize             = Vector2D(0,0);
    Vector2D    vecPixelSize        = Vector2D(0,0);
    Vector2D    vecTransformedSize  = Vector2D(0,0);

    bool        primary         = false;

    uint64_t    ID              = -1;
    int         activeWorkspace = -1;
    float       scale = 1;

    std::string szName          = "";

    Vector2D    vecReservedTopLeft = Vector2D(0,0);
    Vector2D    vecReservedBottomRight = Vector2D(0,0);

    // WLR stuff
    wlr_output* output          = nullptr;
    float       refreshRate     = 60;
    wlr_output_damage* damage   = nullptr;
    int         framesToSkip    = 0;
    int         forceFullFrames = 0;
    bool        noFrameSchedule = false;
    wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;

    // for the special workspace
    bool        specialWorkspaceOpen = false;
    
    // Double-linked list because we need to have constant mem addresses for signals
    // We have to store pointers and use raw new/delete because they might be moved between them
    // and I am lazy
    std::array<std::vector<std::unique_ptr<SLayerSurface>>, 4>   m_aLayerSurfaceLists;

    DYNLISTENER(monitorFrame);
    DYNLISTENER(monitorDestroy);
    DYNLISTENER(monitorMode);

    // hack: a group = workspaces on a monitor.
    // I don't really care lol :P
    wlr_ext_workspace_group_handle_v1* pWLRWorkspaceGroupHandle = nullptr;


    // For the list lookup

    bool operator==(const SMonitor& rhs) {
        return vecPosition == rhs.vecPosition && vecSize == rhs.vecSize && szName == rhs.szName;
    }
};