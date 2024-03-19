#pragma once

#include <list>
#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"

class CInputManager;
struct STextInputV1;

class CInputMethodRelay {
  public:
    CInputMethodRelay();

    void                 onNewIME(wlr_input_method_v2*);
    void                 onNewTextInput(wlr_text_input_v3*);

    wlr_input_method_v2* m_pWLRIME = nullptr;

    void                 commitIMEState(STextInput* pInput);
    void                 removeTextInput(STextInput* pInput);

    void                 onKeyboardFocus(wlr_surface*);

    STextInput*          getFocusedTextInput();

    SIMEKbGrab*          getIMEKeyboardGrab(SKeyboard*);

    void                 setIMEPopupFocus(SIMEPopup*, wlr_surface*);
    void                 updateInputPopup(SIMEPopup*);
    void                 damagePopup(SIMEPopup*);
    void                 removePopup(SIMEPopup*);

  private:
    std::unique_ptr<SIMEKbGrab> m_pKeyboardGrab;

    std::list<STextInput>       m_lTextInputs;
    std::list<SIMEPopup>        m_lIMEPopups;

    DYNLISTENER(textInputNew);
    DYNLISTENER(IMECommit);
    DYNLISTENER(IMEDestroy);
    DYNLISTENER(IMEGrab);
    DYNLISTENER(IMENewPopup);

    void                                          createNewTextInput(wlr_text_input_v3*, STextInputV1* tiv1 = nullptr);

    wlr_surface*                                  focusedSurface(STextInput* pInput);
    wlr_surface*                                  m_pFocusedSurface;
    void                                          onTextInputLeave(wlr_surface* pSurface);
    void                                          onTextInputEnter(wlr_surface* pSurface);

    std::unordered_map<wlr_surface*, STextInput*> m_mSurfaceToTextInput;
    void                                          setSurfaceToPTI(wlr_surface* pSurface, STextInput* pInput);
    STextInput*                                   getTextInput(wlr_surface* pSurface);
    void                                          removeSurfaceToPTI(STextInput* pInput);

    std::unordered_map<wl_client*, int>           m_mClientTextInputVersion;
    int                                           setTextInputVersion(wl_client* pClient, int version);
    int                                           getTextInputVersion(wl_client* pClient);
    void                                          removeTextInputVersion(wl_client* pClient);

    friend class CHyprRenderer;
    friend class CInputManager;
    friend class CTextInputV1ProtocolManager;
};
