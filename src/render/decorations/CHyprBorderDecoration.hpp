#pragma once

#include "AnimatedDecorationGradient.hpp"
#include "IHyprWindowDecoration.hpp"

class CHyprBorderDecoration : public IHyprWindowDecoration {
  public:
    CHyprBorderDecoration(PHLWINDOW);
    virtual ~CHyprBorderDecoration() = default;

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(Render::CRenderingContext&, PHLMONITOR, float const& a);

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
    CBox                        assignedBoxGlobalForRender(const Render::CRenderingContext&);
    bool                        doesntWantBorders();
};
