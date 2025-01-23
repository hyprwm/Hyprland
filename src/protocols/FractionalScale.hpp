#pragma once

#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "fractional-scale-v1.hpp"

class CFractionalScaleProtocol;
class CWLSurfaceResource;

class CFractionalScaleAddon {
  public:
    CFractionalScaleAddon(SP<CWpFractionalScaleV1> resource_, SP<CWLSurfaceResource> surf_);

    void                   setScale(const float& scale);

    bool                   good();

    SP<CWLSurfaceResource> surf();

    bool                   operator==(const wl_resource* other) const {
        return other == m_resource->resource();
    }

    bool operator==(const CFractionalScaleAddon* other) const {
        return other->m_resource == m_resource;
    }

  private:
    SP<CWpFractionalScaleV1> m_resource;
    float                    m_scale = -1.F; // unset
    WP<CWLSurfaceResource>   m_surface;

    friend class CFractionalScaleProtocol;
};

class CFractionalScaleProtocol : public IWaylandProtocol {
  public:
    CFractionalScaleProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onSurfaceDestroy(SP<CWLSurfaceResource> surf);
    void         sendScale(SP<CWLSurfaceResource> surf, const float& scale);

  private:
    void removeAddon(CFractionalScaleAddon*);
    void onManagerResourceDestroy(wl_resource* res);
    void onGetFractionalScale(CWpFractionalScaleManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface);

    //

    std::unordered_map<WP<CWLSurfaceResource>, float>                     m_mSurfaceScales;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CFractionalScaleAddon>> m_mAddons;
    std::vector<UP<CWpFractionalScaleManagerV1>>                          m_vManagers;

    friend class CFractionalScaleAddon;
};

namespace PROTO {
    inline UP<CFractionalScaleProtocol> fractional;
};
