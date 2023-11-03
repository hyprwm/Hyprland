#pragma once

#include "../defines.hpp"
#include "fractional-scale-v1-protocol.h"

#include <vector>
#include <algorithm>

struct SFractionalScaleAddon {
    wlr_surface* pSurface       = nullptr;
    double       preferredScale = 1.0;
    wl_resource* pResource      = nullptr;
};

class CFractionalScaleProtocolManager {
  public:
    CFractionalScaleProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);

    void displayDestroy();

    void setPreferredScaleForSurface(wlr_surface*, double);

    void removeAddon(wlr_surface*);

    // handlers

    void getFractionalScale(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface);

  private:
    SFractionalScaleAddon*                              getAddonForSurface(wlr_surface*);

    std::vector<std::unique_ptr<SFractionalScaleAddon>> m_vFractionalScaleAddons;

    wl_global*                                          m_pGlobal = nullptr;
    wl_listener                                         m_liDisplayDestroy;
};