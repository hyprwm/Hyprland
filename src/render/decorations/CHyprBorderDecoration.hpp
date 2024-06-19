#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprBorderDecoration : public IHyprWindowDecoration {
  public:
    CHyprBorderDecoration(PHLWINDOW);
    virtual ~CHyprBorderDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(CMonitor*, float a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    SBoxExtents  m_seExtents;
    SBoxExtents  m_seReportedExtents;

    PHLWINDOWREF m_pWindow;

    Vector2D     m_vLastWindowPos;
    Vector2D     m_vLastWindowSize;

    CBox         m_bAssignedGeometry = {0};

    CBox         assignedBoxGlobal();
    bool         doesntWantBorders();
};
