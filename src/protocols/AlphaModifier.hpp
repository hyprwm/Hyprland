#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "alpha-modifier-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;

class CAlphaModifier {
  public:
    CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource_, SP<CWLSurfaceResource> surface);
    ~CAlphaModifier();

    bool                   good();
    SP<CWLSurfaceResource> getSurface();
    void                   onSurfaceDestroy();

  private:
    SP<CWpAlphaModifierSurfaceV1> resource;
    WP<CWLSurfaceResource>        pSurface;

    void                          setSurfaceAlpha(float a);

    struct {
        CHyprSignalListener destroySurface;
    } listeners;
};

class CAlphaModifierProtocol : public IWaylandProtocol {
  public:
    CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyModifier(CAlphaModifier* decoration);
    void onGetSurface(CWpAlphaModifierV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface);

    //
    std::vector<UP<CWpAlphaModifierV1>>                            m_vManagers;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CAlphaModifier>> m_mAlphaModifiers; // xdg_toplevel -> deco

    friend class CAlphaModifier;
};

namespace PROTO {
    inline UP<CAlphaModifierProtocol> alphaModifier;
};
