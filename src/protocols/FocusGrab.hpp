#pragma once

#include "WaylandProtocol.hpp"
#include "hyprland-focus-grab-v1.hpp"
#include "macros.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

class CFocusGrab;
class CSeatGrab;

class CFocusGrabSurfaceState {
  public:
    CFocusGrabSurfaceState(CFocusGrab* grab, wlr_surface* surface);
    ~CFocusGrabSurfaceState();

    enum State {
        PendingAddition,
        PendingRemoval,
        Comitted,
    } state = PendingAddition;

  private:
    DYNLISTENER(surfaceDestroy);
};

class CFocusGrab {
  public:
    CFocusGrab(SP<CHyprlandFocusGrabV1> resource_);
    ~CFocusGrab();

    bool good();
    bool isSurfaceComitted(wlr_surface* surface);

    void start();
    void finish(bool sendCleared);

  private:
    void                                                         addSurface(wlr_surface* surface);
    void                                                         removeSurface(wlr_surface* surface);
    void                                                         eraseSurface(wlr_surface* surface);
    void                                                         refocusKeyboard();
    void                                                         commit(bool removeOnly = false);

    SP<CHyprlandFocusGrabV1>                                     resource;
    std::unordered_map<wlr_surface*, UP<CFocusGrabSurfaceState>> m_mSurfaces;
    SP<CSeatGrab>                                                grab;

    bool                                                         m_bGrabActive = false;

    DYNLISTENER(pointerGrabStarted);
    DYNLISTENER(keyboardGrabStarted);
    DYNLISTENER(touchGrabStarted);
    friend class CFocusGrabSurfaceState;
};

class CFocusGrabProtocol : public IWaylandProtocol {
  public:
    CFocusGrabProtocol(const wl_interface* iface, const int& var, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                         onManagerResourceDestroy(wl_resource* res);
    void                                         destroyGrab(CFocusGrab* grab);
    void                                         onCreateGrab(CHyprlandFocusGrabManagerV1* pMgr, uint32_t id);

    std::vector<UP<CHyprlandFocusGrabManagerV1>> m_vManagers;
    std::vector<UP<CFocusGrab>>                  m_vGrabs;

    friend class CFocusGrab;
};

namespace PROTO {
    inline UP<CFocusGrabProtocol> focusGrab;
}
