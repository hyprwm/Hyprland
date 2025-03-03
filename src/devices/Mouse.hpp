#pragma once

#include "IPointer.hpp"

class CMouse : public IPointer {
  public:
    static SP<CMouse>                create(SP<Aquamarine::IPointer> mouse);

    virtual bool                     isVirtual();
    virtual SP<Aquamarine::IPointer> aq();

  private:
    CMouse(SP<Aquamarine::IPointer> mouse);

    WP<Aquamarine::IPointer> mouse;

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
        m_m_listeners;
    };
