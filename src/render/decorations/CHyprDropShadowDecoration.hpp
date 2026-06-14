#pragma once

#include "IHyprWindowDecoration.hpp"
#include "../../config/shared/complex/ComplexDataTypes.hpp"

struct SShadowRenderData {
    bool  valid = false;
    CBox  fullBox;
    float rounding      = 0;
    float roundingPower = 0;
    int   size          = 0;
};

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

    bool                               canRender(PHLMONITOR);
    SShadowRenderData                  getRenderData(PHLMONITOR, float const& a);
    void                               reposition();

    // TODO remove
    void render(PHLMONITOR, float const& a);

  private:
    SBoxExtents  m_extents;
    SBoxExtents  m_reportedExtents;

    PHLWINDOWREF m_window;

    Vector2D     m_lastWindowPos;
    Vector2D     m_lastWindowSize;

    void         drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a);
    void         drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,
                                    float lerp, float a);

    CBox         m_lastWindowBox          = {0};
    CBox         m_lastWindowBoxWithDecos = {0};
};
