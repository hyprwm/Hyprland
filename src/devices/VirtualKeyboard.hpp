#pragma once

#include "IKeyboard.hpp"

class CVirtualKeyboardV1Resource;

class CVirtualKeyboard : public IKeyboard {
  public:
    static SP<CVirtualKeyboard>       create(SP<CVirtualKeyboardV1Resource> keeb);

    virtual bool                      isVirtual();
    virtual SP<Aquamarine::IKeyboard> aq();

    wl_client*                        getClient();

  private:
    CVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keeb);

    WP<CVirtualKeyboardV1Resource> m_keyboard;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener key;
        CHyprSignalListener modifiers;
        CHyprSignalListener keymap;
    } m_listeners;
};
