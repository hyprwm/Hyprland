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

    void               onKeyboardFocus(SP<CWLSurfaceResource>);

    CTextInput*        getFocusedTextInput();

    void               setIMEPopupFocus(CInputPopup*, SP<CWLSurfaceResource>);
    void               removePopup(CInputPopup*);

    CInputPopup*       popupFromCoords(const Vector2D& point);
    CInputPopup*       popupFromSurface(const SP<CWLSurfaceResource> surface);

    void               updateAllPopups();

    WP<CInputMethodV2> m_pIME;

  private:
    std::vector<std::unique_ptr<CTextInput>>  m_vTextInputs;
    std::vector<std::unique_ptr<CInputPopup>> m_vIMEPopups;

    WP<CWLSurfaceResource>                    m_pLastKbFocus;

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
    friend class CTextInput;
    friend class CHyprRenderer;
};
