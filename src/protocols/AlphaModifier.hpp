#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "alpha-modifier-v1.hpp"

class CAlphaModifier {
  public:
    CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource_, wlr_surface* surface);
    ~CAlphaModifier();

    bool         good();
    wlr_surface* getSurface();
    void         onSurfaceDestroy();

  private:
    SP<CWpAlphaModifierSurfaceV1> resource;
    wlr_surface*                  pSurface = nullptr;

    void                          setSurfaceAlpha(float a);

    DYNLISTENER(surfaceDestroy);
};

class CAlphaModifierProtocol : public IWaylandProtocol {
  public:
    CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyModifier(CAlphaModifier* decoration);
    void onGetSurface(CWpAlphaModifierV1* pMgr, uint32_t id, wlr_surface* surface);

    //
    std::vector<UP<CWpAlphaModifierV1>>                  m_vManagers;
    std::unordered_map<wlr_surface*, UP<CAlphaModifier>> m_mAlphaModifiers; // xdg_toplevel -> deco

    friend class CAlphaModifier;
};

namespace PROTO {
    inline UP<CAlphaModifierProtocol> alphaModifier;
};