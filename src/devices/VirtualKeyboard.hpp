#pragma once

#include "IKeyboard.hpp"

class CVirtualKeyboardV1Resource;

class CVirtualKeyboard : public IKeyboard {
  public:
    static SP<CVirtualKeyboard> create(SP<CVirtualKeyboardV1Resource> keeb);

    virtual bool                isVirtual();
    virtual wlr_keyboard*       wlr();

    wl_client*                  getClient();

  private:
    CVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keeb);

    WP<CVirtualKeyboardV1Resource> keyboard;

    void                           disconnectCallbacks();

    DYNLISTENER(destroy);
    DYNLISTENER(key);
    DYNLISTENER(modifiers);
    DYNLISTENER(keymap);
    DYNLISTENER(repeatInfo);
};
