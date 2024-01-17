#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprBorderDecoration : public IHyprWindowDecoration {
  public:
    CHyprBorderDecoration(CWindow*);
    virtual ~CHyprBorderDecoration();

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

    CBox                     m_bAssignedGeometry = {0};

    CBox                     assignedBoxGlobal();
    bool                     doesntWantBorders();
};
