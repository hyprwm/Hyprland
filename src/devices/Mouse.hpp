#pragma once

#include "IPointer.hpp"

class CMouse : public IPointer {
  public:
    static SP<CMouse>    create(wlr_pointer* mouse);

    virtual bool         isVirtual();
    virtual wlr_pointer* wlr();

  private:
    CMouse(wlr_pointer* mouse);

    wlr_pointer* mouse = nullptr;

    void         disconnectCallbacks();

    DYNLISTENER(destroy);
    DYNLISTENER(motion);
    DYNLISTENER(motionAbsolute);
    DYNLISTENER(button);
    DYNLISTENER(axis);
    DYNLISTENER(frame);

    DYNLISTENER(swipeBegin);
    DYNLISTENER(swipeEnd);
    DYNLISTENER(swipeUpdate);

    DYNLISTENER(pinchBegin);
    DYNLISTENER(pinchEnd);
    DYNLISTENER(pinchUpdate);

    DYNLISTENER(holdBegin);
    DYNLISTENER(holdEnd);
};
