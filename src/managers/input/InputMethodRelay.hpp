#pragma once

#include <list>
#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"
#include "TextInput.hpp"

class CInputManager;
struct STextInputV1;

class CInputMethodRelay {
  public:
    CInputMethodRelay();

    void                 onNewIME(wlr_input_method_v2*);
    void                 onNewTextInput(wlr_text_input_v3*);
    void                 onNewTextInput(STextInputV1* pTIV1);

    wlr_input_method_v2* m_pWLRIME = nullptr;

    void                 commitIMEState(CTextInput* pInput);
    void                 removeTextInput(CTextInput* pInput);

    void                 onKeyboardFocus(wlr_surface*);

    CTextInput*          getFocusedTextInput();

    SIMEKbGrab*          getIMEKeyboardGrab(SKeyboard*);

    void                 setIMEPopupFocus(SIMEPopup*, wlr_surface*);
    void                 updateInputPopup(SIMEPopup*);
    void                 damagePopup(SIMEPopup*);
    void                 removePopup(SIMEPopup*);

  private:
    std::unique_ptr<SIMEKbGrab>            m_pKeyboardGrab;

    std::list<std::unique_ptr<CTextInput>> m_vTextInputs;
    std::list<SIMEPopup>                   m_lIMEPopups;

    wlr_surface*                           m_pLastKbFocus = nullptr;

    DYNLISTENER(textInputNew);
    DYNLISTENER(IMECommit);
    DYNLISTENER(IMEDestroy);
    DYNLISTENER(IMEGrab);
    DYNLISTENER(IMENewPopup);

    friend class CHyprRenderer;
    friend class CInputManager;
    friend class CTextInputV1ProtocolManager;
    friend struct CTextInput;
};
