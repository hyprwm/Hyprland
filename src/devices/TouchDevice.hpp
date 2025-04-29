#pragma once

#include "ITouch.hpp"

class CTouchDevice : public ITouch {
  public:
    static SP<CTouchDevice>        create(SP<Aquamarine::ITouch> touch);

    virtual bool                   isVirtual();
    virtual SP<Aquamarine::ITouch> aq();

  private:
    CTouchDevice(SP<Aquamarine::ITouch> touch);

    WP<Aquamarine::ITouch> m_touch;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener down;
        CHyprSignalListener up;
        CHyprSignalListener motion;
        CHyprSignalListener cancel;
        CHyprSignalListener frame;
    } m_listeners;
};