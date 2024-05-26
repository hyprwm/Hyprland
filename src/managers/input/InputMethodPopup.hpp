#pragma once

#include "../../helpers/WLListener.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../macros.hpp"
#include "../../helpers/Box.hpp"
#include "../../helpers/signal/Listener.hpp"

class CInputMethodPopupV2;

class CInputPopup {
  public:
    CInputPopup(SP<CInputMethodPopupV2> popup);

    void                   damageEntire();
    void                   damageSurface();

    bool                   isVecInPopup(const Vector2D& point);

    CBox                   globalBox();
    SP<CWLSurfaceResource> getSurface();

    void                   onCommit();

  private:
    SP<CWLSurface>          queryOwner();
    void                    updateBox();

    void                    onDestroy();
    void                    onMap();
    void                    onUnmap();

    WP<CInputMethodPopupV2> popup;
    SP<CWLSurface>          surface;
    CBox                    lastBoxLocal;
    uint64_t                lastMonitor = -1;

    struct {
        CHyprSignalListener map;
        CHyprSignalListener unmap;
        CHyprSignalListener destroy;
        CHyprSignalListener commit;
    } listeners;
};
