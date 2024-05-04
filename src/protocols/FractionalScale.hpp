#pragma once

#include <memory>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "fractional-scale-v1.hpp"

class CFractionalScaleProtocol;

class CFractionalScaleAddon {
  public:
    CFractionalScaleAddon(SP<CWpFractionalScaleV1> resource_, wlr_surface* surf_);

    void         setScale(const float& scale);
    void         onSurfaceDestroy();

    bool         good();

    wlr_surface* surf();

    bool         operator==(const wl_resource* other) const {
        return other == resource->resource();
    }

    bool operator==(const CFractionalScaleAddon* other) const {
        return other->resource == resource;
    }

  private:
    SP<CWpFractionalScaleV1> resource;
    wlr_surface*             surface     = nullptr;
    bool                     surfaceGone = false;

    friend class CFractionalScaleProtocol;
};

struct SSurfaceListener {
    DYNLISTENER(surfaceDestroy);
};

class CFractionalScaleProtocol : public IWaylandProtocol {
  public:
    CFractionalScaleProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onSurfaceDestroy(wlr_surface* surf);
    void         sendScale(wlr_surface* surf, const float& scale);

  private:
    void removeAddon(CFractionalScaleAddon*);
    void registerSurface(wlr_surface*);
    void onManagerResourceDestroy(wl_resource* res);
    void onGetFractionalScale(CWpFractionalScaleManagerV1* pMgr, uint32_t id, wlr_surface* surface);

    //
    std::unordered_map<wlr_surface*, SSurfaceListener>          m_mSurfaceDestroyListeners;

    std::unordered_map<wlr_surface*, float>                     m_mSurfaceScales;
    std::unordered_map<wlr_surface*, UP<CFractionalScaleAddon>> m_mAddons;
    std::vector<UP<CWpFractionalScaleManagerV1>>                m_vManagers;

    friend class CFractionalScaleAddon;
};

namespace PROTO {
    inline UP<CFractionalScaleProtocol> fractional;
};