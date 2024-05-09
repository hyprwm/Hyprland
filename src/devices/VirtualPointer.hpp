#pragma once

#include "IPointer.hpp"

class CVirtualPointerV1Resource;

class CVirtualPointer : public IPointer {
  public:
    static SP<CVirtualPointer> create(SP<CVirtualPointerV1Resource> resource);

    virtual bool               isVirtual();
    virtual wlr_pointer*       wlr();

  private:
    CVirtualPointer(SP<CVirtualPointerV1Resource>);

    WP<CVirtualPointerV1Resource> pointer;

    void                          disconnectCallbacks();

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