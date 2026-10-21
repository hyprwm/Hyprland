#pragma once

#include "AnimatedDecorationGradient.hpp"
#include "IHyprWindowDecoration.hpp"

class CHyprInnerGlowDecoration : public IHyprWindowDecoration {
  public:
    CHyprInnerGlowDecoration(PHLWINDOW);
    virtual ~CHyprInnerGlowDecoration() = default;

    virtual SDecorationPositioningInfo getPositioningInfo() override;

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply) override;

    virtual void                       draw(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const Render::SWindowRenderPresentation& presentation) override;

    virtual eDecorationType            getDecorationType() override;

    virtual void                       updateWindow(PHLWINDOW) override;

    virtual void                       damageEntire() override;

    virtual eDecorationLayer           getDecorationLayer() override;

    virtual uint64_t                   getDecorationFlags() override;

    virtual std::string                getDisplayName() override;

    virtual void                       initializeAnimations() override;
    virtual void                       updateState() override;
    virtual void                       onWindowMap() override;
    virtual void                       onWindowFocus() override;

    void                               render(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const Render::SWindowRenderPresentation& presentation);

  private:
    bool         visible();
    void         drawGlowInternal(Render::CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a);
    void         drawGlowInternal(Render::CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                  const Config::CGradientValueData& grad2, float lerp, float a);

    PHLWINDOWREF m_window;

    CAnimatedDecorationGradient m_gradient;

    Vector2D                    m_lastWindowPos;
    Vector2D                    m_lastWindowSize;
};
