#pragma once

#include "ITouch.hpp"

class CTouchDevice : public ITouch {
  public:
    static SP<CTouchDevice> create(wlr_touch* touch);

    virtual bool            isVirtual();
    virtual wlr_touch*      wlr();

  private:
    CTouchDevice(wlr_touch* touch);

    wlr_touch* touch = nullptr;

    void       disconnectCallbacks();

    DYNLISTENER(destroy);
    DYNLISTENER(down);
    DYNLISTENER(up);
    DYNLISTENER(motion);
    DYNLISTENER(cancel);
    DYNLISTENER(frame);
};