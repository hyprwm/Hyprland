#pragma once

#include "IKeyboard.hpp"

class CKeyboard : public IKeyboard {
  public:
    static SP<CKeyboard>              create(SP<Aquamarine::IKeyboard> keeb);

    virtual bool                      isVirtual();
    virtual SP<Aquamarine::IKeyboard> aq();

  private:
    CKeyboard(SP<Aquamarine::IKeyboard> keeb);

    WP<Aquamarine::IKeyboard> keyboard;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener key;
        CHyprSignalListener modifiers;
        m_m_listeners;
    };