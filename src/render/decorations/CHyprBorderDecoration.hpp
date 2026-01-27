#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprBorderDecoration : public IHyprWindowDecoration {
  public:
    CHyprBorderDecoration(PHLWINDOW);
    virtual ~CHyprBorderDecoration() = default;

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    SBoxExtents  m_extents;
    SBoxExtents  m_reportedExtents;

    PHLWINDOWREF m_window;

    CBox         m_assignedGeometry = {0};

    int          m_lastBorderSize = -1;

    CBox         assignedBoxGlobal();
    bool         doesntWantBorders();
};
