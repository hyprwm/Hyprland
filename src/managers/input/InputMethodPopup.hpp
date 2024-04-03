#pragma once

#include "../../helpers/WLListener.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../macros.hpp"
#include "../../helpers/Box.hpp"

struct wlr_input_popup_surface_v2;

class CInputPopup {
  public:
    CInputPopup(wlr_input_popup_surface_v2* surf);

    void         onDestroy();
    void         onMap();
    void         onUnmap();
    void         onCommit();

    void         damageEntire();
    void         damageSurface();

    bool         isVecInPopup(const Vector2D& point);

    CBox         globalBox();
    wlr_surface* getWlrSurface();

  private:
    void                        initCallbacks();
    CWLSurface*                 queryOwner();
    void                        updateBox();

    wlr_input_popup_surface_v2* pWlr = nullptr;
    CWLSurface                  surface;
    CBox                        lastBoxLocal;
    uint64_t                    lastMonitor = -1;

    DYNLISTENER(mapPopup);
    DYNLISTENER(unmapPopup);
    DYNLISTENER(destroyPopup);
    DYNLISTENER(commitPopup);
};
