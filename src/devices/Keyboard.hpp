#pragma once

#include "IKeyboard.hpp"

class CKeyboard : public IKeyboard {
  public:
    static SP<CKeyboard>  create(wlr_keyboard* keeb);

    virtual bool          isVirtual();
    virtual wlr_keyboard* wlr();

  private:
    CKeyboard(wlr_keyboard* keeb);

    wlr_keyboard* keyboard = nullptr;

    void          disconnectCallbacks();

    DYNLISTENER(destroy);
    DYNLISTENER(key);
    DYNLISTENER(modifiers);
    DYNLISTENER(keymap);
    DYNLISTENER(repeatInfo);
};