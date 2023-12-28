#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprDropShadowDecoration : public IHyprWindowDecoration {
  public:
    CHyprDropShadowDecoration(CWindow*);
    virtual ~CHyprDropShadowDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(CMonitor*, float a, const Vector2D& offset);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(CWindow*);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    SWindowDecorationExtents m_seExtents;
    SWindowDecorationExtents m_seReportedExtents;

    CWindow*                 m_pWindow = nullptr;

    Vector2D                 m_vLastWindowPos;
    Vector2D                 m_vLastWindowSize;

    CBox                     m_bLastWindowBox          = {0};
    CBox                     m_bLastWindowBoxWithDecos = {0};
};
