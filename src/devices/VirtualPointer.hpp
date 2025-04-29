#pragma once

#include "IPointer.hpp"

class CVirtualPointerV1Resource;

class CVirtualPointer : public IPointer {
  public:
    static SP<CVirtualPointer>       create(SP<CVirtualPointerV1Resource> resource);

    virtual bool                     isVirtual();
    virtual SP<Aquamarine::IPointer> aq();

  private:
    CVirtualPointer(SP<CVirtualPointerV1Resource>);

    WP<CVirtualPointerV1Resource> m_pointer;

    struct {
        CHyprSignalListener destroy;

        CHyprSignalListener motion;
        CHyprSignalListener motionAbsolute;
        CHyprSignalListener button;
        CHyprSignalListener axis;
        CHyprSignalListener frame;

        CHyprSignalListener swipeBegin;
        CHyprSignalListener swipeEnd;
        CHyprSignalListener swipeUpdate;

        CHyprSignalListener pinchBegin;
        CHyprSignalListener pinchEnd;
        CHyprSignalListener pinchUpdate;

        CHyprSignalListener holdBegin;
        CHyprSignalListener holdEnd;
    } m_listeners;
};