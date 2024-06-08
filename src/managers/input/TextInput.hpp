#pragma once

#include "../../helpers/WLListener.hpp"
#include "../../macros.hpp"
#include "../../helpers/Box.hpp"
#include "../../helpers/signal/Listener.hpp"
#include <memory>

struct wl_client;

struct STextInputV1;
class CTextInputV3;
class CInputMethodV2;
class CWLSurfaceResource;

class CTextInput {
  public:
    CTextInput(WP<CTextInputV3> ti);
    CTextInput(STextInputV1* ti);
    ~CTextInput();

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

    bool                   hasCursorRectangle();
    CBox                   cursorBox();

    SP<CWLSurfaceResource> focusedSurface();

  private:
    void                   setFocusedSurface(SP<CWLSurfaceResource> pSurface);
    void                   initCallbacks();

    WP<CWLSurfaceResource> pFocusedSurface;
    int                    enterLocks = 0;
    WP<CTextInputV3>       pV3Input;
    STextInputV1*          pV1Input = nullptr;

    DYNLISTENER(textInputEnable);
    DYNLISTENER(textInputDisable);
    DYNLISTENER(textInputCommit);
    DYNLISTENER(textInputDestroy);

    struct {
        CHyprSignalListener enable;
        CHyprSignalListener disable;
        CHyprSignalListener commit;
        CHyprSignalListener destroy;
        CHyprSignalListener surfaceUnmap;
        CHyprSignalListener surfaceDestroy;
    } listeners;
};