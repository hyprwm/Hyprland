#pragma once

#include "../../helpers/WLListener.hpp"
#include "../../macros.hpp"
#include "../../helpers/Box.hpp"
#include "../../helpers/signal/Listener.hpp"
#include <memory>

struct wlr_surface;
struct wl_client;

struct STextInputV1;
class CTextInputV3;
class CInputMethodV2;

class CTextInput {
  public:
    CTextInput(std::weak_ptr<CTextInputV3> ti);
    CTextInput(STextInputV1* ti);
    ~CTextInput();

    bool         isV3();
    void         enter(wlr_surface* pSurface);
    void         leave();
    void         tiV1Destroyed();
    wl_client*   client();
    void         commitStateToIME(SP<CInputMethodV2> ime);
    void         updateIMEState(SP<CInputMethodV2> ime);

    void         onEnabled(wlr_surface* surfV1 = nullptr);
    void         onDisabled();
    void         onCommit();

    bool         hasCursorRectangle();
    CBox         cursorBox();

    wlr_surface* focusedSurface();

  private:
    void                        setFocusedSurface(wlr_surface* pSurface);
    void                        initCallbacks();

    wlr_surface*                pFocusedSurface = nullptr;
    int                         enterLocks      = 0;
    std::weak_ptr<CTextInputV3> pV3Input;
    STextInputV1*               pV1Input = nullptr;

    DYNLISTENER(textInputEnable);
    DYNLISTENER(textInputDisable);
    DYNLISTENER(textInputCommit);
    DYNLISTENER(textInputDestroy);
    DYNLISTENER(surfaceUnmapped);
    DYNLISTENER(surfaceDestroyed);

    struct {
        CHyprSignalListener enable;
        CHyprSignalListener disable;
        CHyprSignalListener commit;
        CHyprSignalListener destroy;
    } listeners;
};