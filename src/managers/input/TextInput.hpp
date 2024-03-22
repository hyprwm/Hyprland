#pragma once

#include "../../helpers/WLListener.hpp"
#include "../../macros.hpp"
#include "../../helpers/Box.hpp"

struct wlr_text_input_v3;
struct wlr_surface;
struct wl_client;

struct STextInputV1;

class CTextInput {
  public:
    CTextInput(STextInputV1* ti);
    CTextInput(wlr_text_input_v3* ti);
    ~CTextInput();

    bool         isV3();
    void         enter(wlr_surface* pSurface);
    void         leave();
    void         tiV1Destroyed();
    wl_client*   client();
    void         commitStateToIME(wlr_input_method_v2* ime);
    void         updateIMEState(wlr_input_method_v2* ime);

    void         onEnabled(wlr_surface* surfV1 = nullptr);
    void         onDisabled();
    void         onCommit();

    bool         hasCursorRectangle();
    CBox         cursorBox();

    wlr_surface* focusedSurface();

  private:
    void               setFocusedSurface(wlr_surface* pSurface);
    void               initCallbacks();

    wlr_surface*       pFocusedSurface = nullptr;
    int                enterLocks      = 0;
    wlr_text_input_v3* pWlrInput       = nullptr;
    STextInputV1*      pV1Input        = nullptr;

    DYNLISTENER(textInputEnable);
    DYNLISTENER(textInputDisable);
    DYNLISTENER(textInputCommit);
    DYNLISTENER(textInputDestroy);
    DYNLISTENER(surfaceUnmapped);
    DYNLISTENER(surfaceDestroyed);
};