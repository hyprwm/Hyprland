#pragma once

#include "../../defines.hpp"
#include "../../helpers/WLClasses.hpp"

class CInputMethodRelay {
public:
    CInputMethodRelay();

    void        onNewIME(wlr_input_method_v2*);
    void        onNewTextInput(wlr_text_input_v3*);

    wlr_input_method_v2* m_pWLRIME = nullptr;

    void        commitIMEState(wlr_text_input_v3*);
    void        removeTextInput(wlr_text_input_v3*);

    void        onKeyboardFocus(wlr_surface*);

    STextInput* getFocusedTextInput();
    STextInput* getFocusableTextInput();

    void        setPendingSurface(STextInput*, wlr_surface*);

    SIMEKbGrab* getIMEKeyboardGrab(SKeyboard*);

private:

    std::unique_ptr<SIMEKbGrab> m_pKeyboardGrab;

    std::list<STextInput>   m_lTextInputs;

    DYNLISTENER(textInputNew);
    DYNLISTENER(IMECommit);
    DYNLISTENER(IMEDestroy);
    DYNLISTENER(IMEGrab);

    void        createNewTextInput(wlr_text_input_v3*);
};