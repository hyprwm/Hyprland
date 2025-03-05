#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/memory/Memory.hpp"

struct wl_client;

class CTextInputV1;
class CTextInputV3;
class CInputMethodV2;
class CWLSurfaceResource;

class CTextInput {
  public:
    CTextInput(WP<CTextInputV3> ti);
    CTextInput(WP<CTextInputV1> ti);

    bool                   isV3();
    void                   enter(SP<CWLSurfaceResource> pSurface);
    void                   leave();
    void                   tiV1Destroyed();
    wl_client*             client();
    void                   commitStateToIME(SP<CInputMethodV2> ime);
    void                   updateIMEState(SP<CInputMethodV2> ime);

    void                   onEnabled(SP<CWLSurfaceResource> surfV1 = nullptr);
    void                   onDisabled();
    void                   onCommit();
    void                   onReset();

    bool                   hasCursorRectangle();
    CBox                   cursorBox();

    SP<CWLSurfaceResource> focusedSurface();

  private:
    void                   setFocusedSurface(SP<CWLSurfaceResource> pSurface);
    void                   initCallbacks();

    WP<CWLSurfaceResource> pFocusedSurface;
    int                    enterLocks = 0;
    WP<CTextInputV3>       pV3Input;
    WP<CTextInputV1>       pV1Input;

    struct {
        CHyprSignalListener enable;
        CHyprSignalListener disable;
        CHyprSignalListener reset;
        CHyprSignalListener commit;
        CHyprSignalListener destroy;
        CHyprSignalListener surfaceUnmap;
        CHyprSignalListener surfaceDestroy;
        m_m_listeners;
    };
