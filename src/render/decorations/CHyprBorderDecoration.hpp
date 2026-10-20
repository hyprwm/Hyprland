#pragma once

#include "AnimatedDecorationGradient.hpp"
#include "IHyprWindowDecoration.hpp"

class CHyprBorderDecoration : public IHyprWindowDecoration {
  public:
    CHyprBorderDecoration(PHLWINDOW);
    virtual ~CHyprBorderDecoration() = default;

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

    int                                borderSize() const;
    void                               invalidateBorderSize();

  private:
    SBoxExtents                 m_extents;
    SBoxExtents                 m_reportedExtents;

    PHLWINDOWREF                m_window;

    CBox                        m_assignedGeometry = {0};

    int                         m_lastBorderSize = -1;

    CAnimatedDecorationGradient m_gradient;
    mutable int                 m_cachedBorderSize     = -1;
    mutable bool                m_borderSizeCacheDirty = true;

    CBox                        assignedBoxGlobal();
    CBox                        assignedBoxGlobal(const Render::SWindowRenderPresentation& presentation);
    bool                        doesntWantBorders();
};
