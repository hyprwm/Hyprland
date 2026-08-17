#pragma once

#include "AnimatedDecorationGradient.hpp"
#include "IHyprWindowDecoration.hpp"

class CHyprInnerGlowDecoration : public IHyprWindowDecoration {
  public:
    CHyprInnerGlowDecoration(PHLWINDOW);
    virtual ~CHyprInnerGlowDecoration() = default;

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

    virtual void                       initializeAnimations() override;
    virtual void                       updateState() override;
    virtual void                       onWindowMap() override;
    virtual void                       onWindowFocus() override;

    void                               render(PHLMONITOR, float const& a);

  private:
    bool visible();
    void drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a);
    void drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp,
                          float a);

    PHLWINDOWREF                m_window;

    CAnimatedDecorationGradient m_gradient;

    Vector2D                    m_lastWindowPos;
    Vector2D                    m_lastWindowSize;
};
