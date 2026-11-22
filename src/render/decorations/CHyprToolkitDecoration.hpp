#pragma once

#include "IHyprWindowDecoration.hpp"
#include "../../helpers/signal/Signal.hpp"

#include <cstdint>

namespace Hyprtoolkit {
    class IEmbeddedSurface;
    enum ePointerShape : uint8_t;
}

namespace Render {
    class IFramebuffer;
}

class IToolkitDecorationProvider;
class IToolkitDecorationClient;

// A decoration drawn by an embedded hyprtoolkit surface. What it looks like is up to its provider.
class CHyprToolkitDecoration : public IHyprWindowDecoration {
  public:
    CHyprToolkitDecoration(PHLWINDOW window, SP<IToolkitDecorationProvider> provider);
    virtual ~CHyprToolkitDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo() override;
    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply) override;
    virtual void                       draw(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const Render::SWindowRenderPresentation& presentation) override;
    virtual eDecorationType            getDecorationType() override;
    virtual void                       updateWindow(PHLWINDOW) override;
    virtual void                       damageEntire() override;
    virtual bool                       onInputOnDeco(const eInputType, const Vector2D&, std::any = {}) override;
    virtual eDecorationLayer           getDecorationLayer() override;
    virtual uint64_t                   getDecorationFlags() override;
    virtual std::string                getDisplayName() override;

    // lets the provider build the widgets. Call once the decoration is on its window.
    void createClient();
    // drops the provider's state and all widgets
    void releaseClient();

    // renders the widgets into the decoration's buffer if anything changed. Requires the GL context.
    void renderIfNeeded();

    // in global coordinates
    void                              pointerMotion(const Vector2D& coords);
    void                              pointerLeave();

    PHLWINDOW                         window() const;
    SP<IToolkitDecorationProvider>    provider() const;
    SP<Hyprtoolkit::IEmbeddedSurface> surface() const;

  private:
    CBox                              renderBox(const Render::SWindowRenderPresentation& presentation) const;
    Vector2D                          toLocal(const Vector2D& coords);
    bool                              updateSize();
    void                              onFrameRequested();
    void                              onDamage(const CRegion& damage);
    void                              onCursorChanged(Hyprtoolkit::ePointerShape shape);
    bool                              onButton(const Vector2D& coords, const std::any& data);
    bool                              onAxis(const Vector2D& coords, const std::any& data);

    SP<IToolkitDecorationProvider>    m_provider;
    UP<IToolkitDecorationClient>      m_client;
    SP<Hyprtoolkit::IEmbeddedSurface> m_surface;
    SP<Render::IFramebuffer>          m_framebuffer;

    CBox                              m_assignedBox;
    Vector2D                          m_pixelSize;
    float                             m_scale       = 0.F;
    bool                              m_visible     = true; // the decorate window rule
    bool                              m_needsRender = true;
    bool                              m_rendered    = false;
    bool                              m_hovered     = false;
    bool                              m_cursorSet   = false;

    struct {
        CHyprSignalListener frameRequested;
        CHyprSignalListener damaged;
        CHyprSignalListener cursorChanged;
    } m_listeners;
};
