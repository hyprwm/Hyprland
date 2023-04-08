#pragma once

#include "../defines.hpp"
#include <deque>
#include "WLClasses.hpp"
#include <vector>
#include <array>
#include <memory>
#include "Timer.hpp"

struct SMonitorRule;

class CMonitor {
  public:
    CMonitor();
    ~CMonitor();

    Vector2D    vecPosition        = Vector2D(-1, -1); // means unset
    Vector2D    vecSize            = Vector2D(0, 0);
    Vector2D    vecPixelSize       = Vector2D(0, 0);
    Vector2D    vecTransformedSize = Vector2D(0, 0);

    bool        primary = false;

    uint64_t    ID              = -1;
    int         activeWorkspace = -1;
    float       scale           = 1;

    std::string szName = "";

    Vector2D    vecReservedTopLeft     = Vector2D(0, 0);
    Vector2D    vecReservedBottomRight = Vector2D(0, 0);

    // WLR stuff
    wlr_damage_ring     damage;
    wlr_output*         output          = nullptr;
    float               refreshRate     = 60;
    int                 framesToSkip    = 0;
    int                 forceFullFrames = 0;
    bool                noFrameSchedule = false;
    bool                scheduledRecalc = false;
    wl_output_transform transform       = WL_OUTPUT_TRANSFORM_NORMAL;

    bool                dpmsStatus    = true;
    bool                vrrActive     = false; // this can be TRUE even if VRR is not active in the case that this display does not support it.
    bool                enabled10bit  = false; // as above, this can be TRUE even if 10 bit failed.
    bool                createdByUser = false;

    bool                pendingFrame    = false; // if we schedule a frame during rendering, reschedule it after
    bool                renderingActive = false;

    wl_event_source*    renderTimer  = nullptr; // for RAT
    bool                RATScheduled = false;
    CTimer              lastPresentationTimer;

    // mirroring
    CMonitor*              pMirrorOf = nullptr;
    std::vector<CMonitor*> mirrors;

    // for the special workspace. 0 means not open.
    int                                                        specialWorkspaceID = 0;

    std::array<std::vector<std::unique_ptr<SLayerSurface>>, 4> m_aLayerSurfaceLayers;

    DYNLISTENER(monitorFrame);
    DYNLISTENER(monitorDestroy);
    DYNLISTENER(monitorStateRequest);
    DYNLISTENER(monitorDamage);
    DYNLISTENER(monitorNeedsFrame);

    // hack: a group = workspaces on a monitor.
    // I don't really care lol :P
    wlr_ext_workspace_group_handle_v1* pWLRWorkspaceGroupHandle = nullptr;

    // methods
    void                       onConnect(bool noRule);
    void                       onDisconnect();
    void                       addDamage(const pixman_region32_t* rg);
    void                       addDamage(const wlr_box* box);
    void                       setMirror(const std::string&);
    bool                       isMirror();
    float                      getDefaultScale();

    std::shared_ptr<CMonitor>* m_pThisWrap            = nullptr;
    bool                       m_bEnabled             = false;
    bool                       m_bRenderingInitPassed = false;

    // For the list lookup

    bool operator==(const CMonitor& rhs) {
        return vecPosition == rhs.vecPosition && vecSize == rhs.vecSize && szName == rhs.szName;
    }

  private:
    void setupDefaultWS(const SMonitorRule&);
    int  findAvailableDefaultWS();
};
