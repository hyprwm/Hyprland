#pragma once

#include <list>
#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"
#include "TextInput.hpp"
#include "InputMethodPopup.hpp"

class CInputManager;
class CHyprRenderer;
struct STextInputV1;

class CInputMethodRelay {
  public:
    CInputMethodRelay();

    void                 onNewIME(wlr_input_method_v2*);
    void                 onNewTextInput(wlr_text_input_v3*);
    void                 onNewTextInput(STextInputV1* pTIV1);

    wlr_input_method_v2* m_pWLRIME = nullptr;

    void                 activateIME(CTextInput* pInput);
    void                 deactivateIME(CTextInput* pInput);
    void                 commitIMEState(CTextInput* pInput);
    void                 removeTextInput(CTextInput* pInput);

    void                 onKeyboardFocus(wlr_surface*);

    CTextInput*          getFocusedTextInput();

    SIMEKbGrab*          getIMEKeyboardGrab(SKeyboard*);

    void                 setIMEPopupFocus(CInputPopup*, wlr_surface*);
    void                 removePopup(CInputPopup*);

    CInputPopup*         popupFromCoords(const Vector2D& point);
    CInputPopup*         popupFromSurface(const wlr_surface* surface);

    void                 updateAllPopups();

  private:
    std::unique_ptr<SIMEKbGrab>               m_pKeyboardGrab;

    std::vector<std::unique_ptr<CTextInput>>  m_vTextInputs;
    std::vector<std::unique_ptr<CInputPopup>> m_vIMEPopups;

    wlr_surface*                              m_pLastKbFocus = nullptr;

    DYNLISTENER(textInputNew);
    DYNLISTENER(IMECommit);
    DYNLISTENER(IMEDestroy);
    DYNLISTENER(IMEGrab);
    DYNLISTENER(IMENewPopup);

    friend class CHyprRenderer;
    friend class CInputManager;
    friend class CTextInputV1ProtocolManager;
    friend struct CTextInput;
    friend class CHyprRenderer;
};
