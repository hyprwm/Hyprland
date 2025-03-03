#pragma once

#include <list>
#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "TextInput.hpp"
#include "InputMethodPopup.hpp"
#include <any>

class CInputManager;
class CHyprRenderer;
class CTextInputV1;
class CInputMethodV2;

class CInputMethodRelay {
  public:
    CInputMethodRelay();

    void               onNewIME(SP<CInputMethodV2>);
    void               onNewTextInput(WP<CTextInputV3> tiv3);
    void               onNewTextInput(WP<CTextInputV1> pTIV1);

    void               activateIME(CTextInput* pInput, bool shouldCommit = true);
    void               deactivateIME(CTextInput* pInput, bool shouldCommit = true);
    void               commitIMEState(CTextInput* pInput);
    void               removeTextInput(CTextInput* pInput);

    void               onKeyboardFocus(SP<CWLSurfaceResource>);

    CTextInput*        getFocusedTextInput();

    void               removePopup(CInputPopup*);

    CInputPopup*       popupFromCoords(const Vector2D& point);
    CInputPopup*       popupFromSurface(const SP<CWLSurfaceResource> surface);

    void               updateAllPopups();

    WP<CInputMethodV2> m_pIME;

  private:
    std::vector<UP<CTextInput>>  m_vTextInputs;
    std::vector<UP<CInputPopup>> m_vIMEPopups;

    WP<CWLSurfaceResource>       m_pLastKbFocus;

    struct {
        CHyprSignalListener newTIV3;
        CHyprSignalListener newTIV1;
        CHyprSignalListener newIME;
        CHyprSignalListener commitIME;
        CHyprSignalListener destroyIME;
        CHyprSignalListener newPopup;
        m_m_listeners;

        friend class CHyprRenderer;
        friend class CInputManager;
        friend class CTextInputV1ProtocolManager;
        friend class CTextInput;
        friend class CHyprRenderer;
    };
