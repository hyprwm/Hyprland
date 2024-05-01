#pragma once

#include <list>
#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"
#include "../../helpers/signal/Listener.hpp"
#include "TextInput.hpp"
#include "InputMethodPopup.hpp"
#include <any>

class CInputManager;
class CHyprRenderer;
struct STextInputV1;
class CInputMethodV2;

class CInputMethodRelay {
  public:
    CInputMethodRelay();

    void               onNewIME(SP<CInputMethodV2>);
    void               onNewTextInput(std::any tiv3);
    void               onNewTextInput(STextInputV1* pTIV1);

    void               activateIME(CTextInput* pInput);
    void               deactivateIME(CTextInput* pInput);
    void               commitIMEState(CTextInput* pInput);
    void               removeTextInput(CTextInput* pInput);

    void               onKeyboardFocus(wlr_surface*);

    CTextInput*        getFocusedTextInput();

    void               setIMEPopupFocus(CInputPopup*, wlr_surface*);
    void               removePopup(CInputPopup*);

    CInputPopup*       popupFromCoords(const Vector2D& point);
    CInputPopup*       popupFromSurface(const wlr_surface* surface);

    void               updateAllPopups();

    WP<CInputMethodV2> m_pIME;

  private:
    std::vector<std::unique_ptr<CTextInput>>  m_vTextInputs;
    std::vector<std::unique_ptr<CInputPopup>> m_vIMEPopups;

    wlr_surface*                              m_pLastKbFocus = nullptr;

    struct {
        CHyprSignalListener newTIV3;
        CHyprSignalListener newIME;
        CHyprSignalListener commitIME;
        CHyprSignalListener destroyIME;
        CHyprSignalListener newPopup;
    } listeners;

    friend class CHyprRenderer;
    friend class CInputManager;
    friend class CTextInputV1ProtocolManager;
    friend struct CTextInput;
    friend class CHyprRenderer;
};
