#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprDropShadowDecoration : public IHyprWindowDecoration {
  public:
    CHyprDropShadowDecoration(PHLWINDOW);
    virtual ~CHyprDropShadowDecoration() = default;

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

    void                               render(PHLMONITOR, float const& a);

  private:
    SBoxExtents  m_extents;
    SBoxExtents  m_reportedExtents;

    PHLWINDOWREF m_window;

    Vector2D     m_lastWindowPos;
    Vector2D     m_lastWindowSize;

    void         drawShadowInternal(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a);

    CBox         m_lastWindowBox          = {0};
    CBox         m_lastWindowBoxWithDecos = {0};
};
