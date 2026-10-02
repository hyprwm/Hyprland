#pragma once

#include "AnimatedDecorationGradient.hpp"
#include "IHyprWindowDecoration.hpp"

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

    virtual void                       draw(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const SP<Workspace::CWorkspacePresentable>& presentation);

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

    bool                               canRender(PHLMONITOR);
    SShadowRenderData                  getRenderData(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const SP<Workspace::CWorkspacePresentable>& presentation);
    void                               reposition(Render::CRenderContext& ctx);

    // TODO remove
    void render(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const SP<Workspace::CWorkspacePresentable>& presentation);

  private:
    SBoxExtents                 m_extents;
    SBoxExtents                 m_reportedExtents;

    PHLWINDOWREF                m_window;

    CAnimatedDecorationGradient m_gradient;

    Vector2D                    m_lastWindowPos;
    Vector2D                    m_lastWindowSize;

    void drawShadowInternal(Render::CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a,
                            const SP<Workspace::CWorkspacePresentable>& presentation);
    void drawShadowInternal(Render::CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                            const Config::CGradientValueData& grad2, float lerp, float a, const SP<Workspace::CWorkspacePresentable>& presentation);

    CBox m_lastWindowBox          = {0};
    CBox m_lastWindowBoxWithDecos = {0};
};
