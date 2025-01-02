#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "alpha-modifier-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CAlphaModifierProtocol;

class CAlphaModifier {
  public:
    CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource_, SP<CWLSurfaceResource> surface);

    bool good();
    void setResource(SP<CWpAlphaModifierSurfaceV1> resource);

  private:
    SP<CWpAlphaModifierSurfaceV1> m_pResource;
    WP<CWLSurfaceResource>        m_pSurface;
    float                         m_fAlpha = 1.0;

    void                          destroy();

    struct {
        CHyprSignalListener surfaceCommitted;
        CHyprSignalListener surfaceDestroyed;
    } listeners;

    friend class CAlphaModifierProtocol;
};

class CAlphaModifierProtocol : public IWaylandProtocol {
  public:
    CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyManager(CWpAlphaModifierV1* res);
    void destroyAlphaModifier(CAlphaModifier* surface);
    void getSurface(CWpAlphaModifierV1* manager, uint32_t id, SP<CWLSurfaceResource> surface);

    //
    std::vector<UP<CWpAlphaModifierV1>>                            m_vManagers;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CAlphaModifier>> m_mAlphaModifiers;

    friend class CAlphaModifier;
};

namespace PROTO {
    inline UP<CAlphaModifierProtocol> alphaModifier;
};
