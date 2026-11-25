#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <desktop/view/View.hpp>
#include <desktop/view/WLSurface.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <protocols/FractionalScale.hpp>
#include <protocols/HyprlandSurface.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/Viewporter.hpp>
#include <protocols/core/Compositor.hpp>
#include <protocols/core/Subcompositor.hpp>
#include <render/ElementRenderer.hpp>
#include <render/Renderer.hpp>
#include <render/SyncFDManager.hpp>
#include <state/MonitorState.hpp>

#include <array>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <sys/socket.h>

using namespace Hyprutils::OS;

class CHyprlandSurfaceTestOutput : public Aquamarine::IOutput {
  public:
    bool commit() override {
        ADD_FAILURE() << "Unexpected output commit";
        return false;
    }
    bool test() override {
        ADD_FAILURE() << "Unexpected output test";
        return false;
    }
    SP<Aquamarine::IBackendImplementation> getBackend() override {
        return nullptr;
    }
    std::vector<SDRMFormat> getRenderFormats() override {
        return {};
    }
    bool pendingPageFlip() override {
        return false;
    }
    bool pendingIdleFrame() override {
        return false;
    }
};

class CHyprlandSurfaceTestRenderer : public Render::IHyprRenderer {
  public:
    eType type() override {
        return RT_GL;
    }
    Render::SRenderResult endRender(const std::function<void()>&) override {
        ADD_FAILURE() << "Unexpected rendering";
        return {};
    }
    UP<Render::ISyncFDManager> createSyncFDManager() override {
        return nullptr;
    }
    WP<Render::IElementRenderer> elementRenderer() override {
        return nullptr;
    }
    SP<Render::ITexture> createStencilTexture(int, int) override {
        return nullptr;
    }
    using Render::IHyprRenderer::createTexture;
    SP<Render::ITexture> createTexture(bool) override {
        return nullptr;
    }
    SP<Render::ITexture> createTexture(uint32_t, uint8_t*, uint32_t, const Vector2D&, bool, bool) override {
        return nullptr;
    }
    SP<Render::ITexture> createTexture(const Aquamarine::SDMABUFAttrs&, bool, const NColorManagement::SColorRepresentation&) override {
        return nullptr;
    }
    SP<Render::ITexture> createTexture(int, int, unsigned char*) override {
        return nullptr;
    }
    SP<Render::ITexture> createTexture(cairo_surface_t*) override {
        return nullptr;
    }
    SP<Render::ITexture> createTexture(std::span<const float>, size_t) override {
        return nullptr;
    }
    bool explicitSyncSupported() override {
        return false;
    }
    bool fp16Supported() override {
        return false;
    }
    std::vector<SDRMFormat> getDRMFormats() override {
        return {};
    }
    std::vector<uint64_t> getDRMFormatModifiers(DRMFormat) override {
        return {};
    }
    SP<Render::IFramebuffer> createFB(const std::string&) override {
        ADD_FAILURE() << "Unexpected framebuffer allocation";
        return nullptr;
    }
    void disableScissor() override {
        ;
    }
    void blend(bool) override {
        ;
    }
    void drawShadow(Render::CRenderContext&, const CBox&, Math::SCornerRadii, float, int, const Config::CGradientValueData&, float,
                    const Render::SWindowRenderPresentation&) override {
        ADD_FAILURE() << "Unexpected shadow draw";
    }
    void drawShadow(Render::CRenderContext&, const CBox&, Math::SCornerRadii, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float,
                    const Render::SWindowRenderPresentation&) override {
        ADD_FAILURE() << "Unexpected shadow draw";
    }
    void drawGlow(Render::CRenderContext&, const CBox&, Math::SCornerRadii, float, int, const Config::CGradientValueData&, float) override {
        ADD_FAILURE() << "Unexpected glow draw";
    }
    void drawGlow(Render::CRenderContext&, const CBox&, Math::SCornerRadii, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float,
                  float) override {
        ADD_FAILURE() << "Unexpected glow draw";
    }
    void setViewport(int, int, int, int) override {
        ;
    }
    SP<Render::IFramebuffer> blurFramebuffer(Render::CRenderContext&, SP<Render::IFramebuffer>, float, const CRegion&, const Render::SBlurContext&) override {
        ADD_FAILURE() << "Unexpected blur";
        return nullptr;
    }
    void refreshBlurProvider() override {
        ;
    }
    void expandBlurDamage(CRegion&, float) const override {
        ;
    }
    bool blurProviderIsAnimated(Render::CRenderContext&) const override {
        return false;
    }
    bool blurProviderRequiresLiveBlur() const override {
        return false;
    }
    bool reloadShaders(const std::string&) override {
        return false;
    }

  private:
    void renderOffToMain(Render::CRenderContext&, SP<Render::IFramebuffer>) override {
        ADD_FAILURE() << "Unexpected rendering";
    }
    SP<Render::IRenderbuffer> getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer>, uint32_t) override {
        ADD_FAILURE() << "Unexpected renderbuffer allocation";
        return nullptr;
    }
};

class CHyprlandSurfaceTestTexture : public Render::ITexture {
  public:
    void setTexParameter(GLenum, GLint) override {
        ADD_FAILURE();
    }
    void allocate(const Vector2D&, uint32_t) override {
        ADD_FAILURE();
    }
    void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
        ADD_FAILURE();
    }
};

class CHyprlandSurfaceTestBuffer : public IHLBuffer {
  public:
    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return Aquamarine::BUFFER_TYPE_MISC;
    }
    void update(const CRegion&) override {
        ADD_FAILURE();
    }
    bool isSynchronous() override {
        return false;
    }
    bool good() override {
        return true;
    }
};

class CHyprlandSurfaceTestView : public Desktop::View::IView {
  public:
    CHyprlandSurfaceTestView(SP<Desktop::View::CWLSurface> wlSurface, const CBox& box) : IView(wlSurface), m_box(box) {
        ;
    }

    static SP<CHyprlandSurfaceTestView> create(SP<Desktop::View::CWLSurface> wlSurface, const CBox& box) {
        auto p = makeShared<CHyprlandSurfaceTestView>(wlSurface, box);
        p->initView(p, Desktop::View::VIEW_TYPE_LAYER_SURFACE);
        return p;
    }

    Desktop::View::eViewType type() const override {
        return Desktop::View::VIEW_TYPE_LAYER_SURFACE;
    }
    bool mapped() const override {
        return true;
    }
    bool desktopComponent() const override {
        return true;
    }
    std::optional<CBox> logicalBox() const override {
        return m_box;
    }
    std::optional<CBox> surfaceLogicalBox() const override {
        return m_box;
    }
    bool cantLockCursor() const override {
        return false;
    }
    bool focusAvailable() const override {
        return true;
    }

    CBox m_box;
};

class CHyprlandSurfaceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {
            -1,
            -1,
        };
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(m_client, nullptr);
        server.take();

        m_previousCompositor     = std::move(g_pCompositor);
        m_previousEventLoop      = std::move(g_pEventLoopManager);
        m_previousRenderer       = std::move(g_pHyprRenderer);
        m_previousMonitorState   = std::move(State::monitorState());
        m_previousEventBus       = std::move(Event::bus());
        m_previousConfig         = std::move(Config::mgr());
        m_previousWatcher        = std::move(Config::watcher());
        m_previousAnimations     = std::move(Animation::mgr());
        m_previousTree           = std::move(Config::animationTree());
        m_previousPresentation   = std::move(PROTO::presentation);
        m_previousCoreCompositor = std::move(PROTO::compositor);
        m_previousSubcompositor  = std::move(PROTO::subcompositor);
        m_previousFractional     = std::move(PROTO::fractional);
        m_previousViewport       = std::move(PROTO::viewport);
        m_previousHyprSurface    = std::move(PROTO::hyprlandSurface);
        m_installedGlobals       = true;

        Event::bus()                        = makeUnique<Event::CEventBus>();
        State::monitorState()               = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay          = m_display.get();
        g_pCompositor->m_wlEventLoop        = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_explicitConfigPath = "hyprland-surface-test.lua";
        Aquamarine::SBackendImplementationOptions backend;
        backend.backendType        = Aquamarine::AQ_BACKEND_NULL;
        g_pCompositor->m_aqBackend = Aquamarine::CBackend::create({backend}, Aquamarine::SBackendOptions{});
        ASSERT_TRUE(g_pCompositor->m_aqBackend);
        g_pCompositor->m_sessionActive = false;
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        g_pEventLoopManager     = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
        g_pHyprRenderer         = makeUnique<CHyprlandSurfaceTestRenderer>();
        wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);

        PROTO::presentation    = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "hl-surf-test");
        PROTO::compositor      = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "hl-surf-test-compositor");
        PROTO::subcompositor   = makeUnique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "hl-surf-test-subcompositor");
        PROTO::fractional      = makeUnique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "hl-surf-test-fractional");
        PROTO::viewport        = makeUnique<CViewporterProtocol>(&wp_viewporter_interface, 1, "hl-surf-test-viewporter");
        PROTO::hyprlandSurface = makeUnique<CHyprlandSurfaceProtocol>(&hyprland_surface_manager_v1_interface, 2, "hl-surf-test-hl");

        m_monitor                    = makeShared<Monitor::CMonitor>(makeShared<CHyprlandSurfaceTestOutput>());
        m_monitor->m_self            = m_monitor;
        m_monitor->m_position        = {0, 0};
        m_monitor->m_size            = {1920, 1080};
        m_monitor->m_pixelSize       = {1920, 1080};
        m_monitor->m_transformedSize = {1920, 1080};
        m_monitor->m_scale           = 1.F;
        m_monitor->m_damage.setSize(m_monitor->m_transformedSize);
        m_monitor->m_damage.rotate();
        Event::bus()->m_events.monitor.added.emit(m_monitor);

        m_compositorId = m_nextID++;
        PROTO::compositor->bindManager(m_client, nullptr, 6, m_compositorId);
        m_viewporterId = m_nextID++;
        PROTO::viewport->bindManager(m_client, nullptr, 1, m_viewporterId);
        m_hlManagerId = m_nextID++;
        PROTO::hyprlandSurface->bindManager(m_client, nullptr, 2, m_hlManagerId);
    }

    void TearDown() override {
        m_view.reset();
        m_hlSurface.reset();
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::hyprlandSurface  = std::move(m_previousHyprSurface);
        PROTO::viewport         = std::move(m_previousViewport);
        PROTO::subcompositor    = std::move(m_previousSubcompositor);
        PROTO::compositor       = std::move(m_previousCoreCompositor);
        PROTO::fractional       = std::move(m_previousFractional);
        PROTO::presentation     = std::move(m_previousPresentation);
        g_pHyprRenderer         = std::move(m_previousRenderer);
        g_pEventLoopManager     = std::move(m_previousEventLoop);
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Config::mgr()           = std::move(m_previousConfig);
        Config::watcher()       = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pCompositor         = std::move(m_previousCompositor);
        State::monitorState() = std::move(m_previousMonitorState);
        Event::bus()          = std::move(m_previousEventBus);
        Config::Supplementary::Jeremy::flushCachedCfgPath();
    }

    void request(uint32_t id, uint32_t opcode, std::initializer_list<uint32_t> args = {}) {
        std::vector<uint32_t> message = {
            id,
            sc<uint32_t>((args.size() + 2) * sizeof(uint32_t) << 16) | opcode,
        };
        message.insert(message.end(), args);
        const auto size = message.size() * sizeof(uint32_t);
        ASSERT_EQ(send(m_socket.get(), message.data(), size, MSG_NOSIGNAL), sc<ssize_t>(size));
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    uint32_t createSurface(const CBox& box = {50, 100, 400, 300}, int32_t bufferScale = 1) {
        const uint32_t         surfId = m_nextID++;
        SP<CWLSurfaceResource> surf;
        auto                   newSurfListener = PROTO::compositor->m_events.newSurface.listen([&](SP<CWLSurfaceResource> s) { surf = s; });
        request(m_compositorId, 0, {surfId}); // wl_compositor.create_surface
        EXPECT_TRUE(surf);
        m_hlSurface = Desktop::View::CWLSurface::create();
        m_view      = CHyprlandSurfaceTestView::create(m_hlSurface, box);
        m_hlSurface->assign(surf, m_view);
        auto buffer                  = makeShared<CHyprlandSurfaceTestBuffer>();
        auto wire                    = makeShared<CWlBuffer>(m_client, 1, m_nextID++);
        buffer->m_resource           = CWLBufferResource::create(wire);
        buffer->m_resource->m_buffer = buffer;
        buffer->size                 = box.size() * bufferScale;
        buffer->m_texture            = makeShared<CHyprlandSurfaceTestTexture>();
        m_buffers.emplace_back(buffer);

        request(surfId, 1, {wl_resource_get_id(buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
        if (bufferScale != 1)
            request(surfId, 8, {sc<uint32_t>(bufferScale)}); // wl_surface.set_buffer_scale
        request(surfId, 6);                                  // wl_surface.commit
        surf->map();
        m_monitor->m_damage.rotate();
        return surfId;
    }

    uint32_t createRegion(const std::vector<CBox>& rects) {
        const uint32_t regionId = m_nextID++;
        request(m_compositorId, 1, {regionId}); // wl_compositor.create_region
        for (const auto& r : rects) {
            request(regionId, 1, {sc<uint32_t>(sc<int32_t>(r.x)), sc<uint32_t>(sc<int32_t>(r.y)), sc<uint32_t>(sc<int32_t>(r.w)), sc<uint32_t>(sc<int32_t>(r.h))}); // wl_region.add
        }
        return regionId;
    }

    uint32_t getHyprlandSurface(uint32_t surfId) {
        const uint32_t hlId = m_nextID++;
        request(m_hlManagerId, 0, {hlId, surfId}); // hyprland_surface_manager_v1.get_hyprland_surface
        return hlId;
    }

    void setOpacity(uint32_t hlId, double opacity) {
        request(hlId, 0, {sc<uint32_t>(wl_fixed_from_double(opacity))}); // hyprland_surface_v1.set_opacity
    }

    void destroyHyprlandSurface(uint32_t hlId) {
        request(hlId, 1); // hyprland_surface_v1.destroy
    }

    void setVisibleRegion(uint32_t hlId, uint32_t regionId) {
        request(hlId, 2, {regionId}); // hyprland_surface_v1.set_visible_region
    }

    uint32_t getViewport(uint32_t surfId) {
        const uint32_t viewportId = m_nextID++;
        request(m_viewporterId, 1, {viewportId, surfId}); // wp_viewporter.get_viewport
        return viewportId;
    }

    void setViewportSource(uint32_t viewportId, double x, double y, double width, double height) {
        request(viewportId, 1,
                {sc<uint32_t>(wl_fixed_from_double(x)), sc<uint32_t>(wl_fixed_from_double(y)), sc<uint32_t>(wl_fixed_from_double(width)),
                 sc<uint32_t>(wl_fixed_from_double(height))}); // wp_viewport.set_source
    }

    void setViewportDestination(uint32_t viewportId, int32_t width, int32_t height) {
        request(viewportId, 2, {sc<uint32_t>(width), sc<uint32_t>(height)}); // wp_viewport.set_destination
    }

    void damageSurface(uint32_t surfId, int32_t x, int32_t y, int32_t w, int32_t h) {
        request(surfId, 2, {sc<uint32_t>(x), sc<uint32_t>(y), sc<uint32_t>(w), sc<uint32_t>(h)}); // wl_surface.damage
    }

    void commit(uint32_t surfId) {
        request(surfId, 6); // wl_surface.commit
    }

    CRegion takeDamage() {
        CRegion damage = m_monitor->m_damage.getBufferDamage(1);
        m_monitor->m_damage.rotate();
        return damage;
    }

    wl_client*                    m_client       = nullptr;
    uint32_t                      m_nextID       = 2;
    uint32_t                      m_compositorId = 0;
    uint32_t                      m_viewporterId = 0;
    uint32_t                      m_hlManagerId  = 0;
    PHLMONITOR                    m_monitor;
    SP<Desktop::View::CWLSurface> m_hlSurface;
    SP<CHyprlandSurfaceTestView>  m_view;
    std::vector<SP<IHLBuffer>>    m_buffers;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<Render::IHyprRenderer>                                  m_previousRenderer;
    UP<State::CMonitorStateTracker>                            m_previousMonitorState;
    UP<Event::CEventBus>                                       m_previousEventBus;
    UP<Config::IConfigManager>                                 m_previousConfig;
    UP<Config::CConfigWatcher>                                 m_previousWatcher;
    UP<Animation::CHyprAnimationManager>                       m_previousAnimations;
    UP<Config::CAnimationTreeController>                       m_previousTree;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<CWLCompositorProtocol>                                  m_previousCoreCompositor;
    UP<CWLSubcompositorProtocol>                               m_previousSubcompositor;
    UP<CFractionalScaleProtocol>                               m_previousFractional;
    UP<CViewporterProtocol>                                    m_previousViewport;
    UP<CHyprlandSurfaceProtocol>                               m_previousHyprSurface;
};

TEST_F(CHyprlandSurfaceTest, VisibleRegionPartialDamageAndScaling) {
    const uint32_t surfId = createSurface({50, 100, 400, 300}, 2);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    // Setting a visible region equal to the full buffer (which is equivalent to
    // the default empty visible region) must not damage anything.
    const uint32_t fullRegion = createRegion({{0, 0, 800, 600}});
    setVisibleRegion(hlId, fullRegion);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Restricting visible region to the left half (buffer-local 400x600) must
    // damage only the hidden right half in logical coordinates: {250, 100, 200, 300}.
    const uint32_t leftRegion = createRegion({{0, 0, 400, 600}});
    setVisibleRegion(hlId, leftRegion);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));

    // Committing unrelated surface damage with an unchanged visible region must
    // trigger the commit listener without adding visible-region damage.
    damageSurface(surfId, 10, 20, 30, 15);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Re-setting an identical visible region must also short-circuit with zero damage.
    const uint32_t leftRegionCopy = createRegion({{0, 0, 400, 600}});
    setVisibleRegion(hlId, leftRegionCopy);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Clearing visible region (nullptr = whole surface visible) must damage only
    // the newly visible right half: {250, 100, 200, 300}.
    setVisibleRegion(hlId, 0);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, UnrestrictedMaskTransitionsDamageOnlyChangedArea) {
    const uint32_t surfId = createSurface({50, 100, 400, 300}, 2);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    // Direct transition from default empty (unrestricted) to restricted must
    // expand the old empty region to the full buffer and damage only the hidden
    // right half: {250, 100, 200, 300}.
    const uint32_t leftRegion = createRegion({{0, 0, 400, 600}});
    setVisibleRegion(hlId, leftRegion);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));

    // Setting an explicit empty wl_region resets to unrestricted and must damage
    // only the newly visible right half.
    const uint32_t emptyRegion = createRegion({});
    setVisibleRegion(hlId, emptyRegion);
    commit(surfId);
    EXPECT_TRUE(m_hlSurface->m_visibleRegion.empty());
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, DisjointAndOutOfBoundsVisibleRegionTransitions) {
    const uint32_t surfId = createSurface({50, 100, 400, 300}, 2);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    // Start restricted to a small top-left box {0, 0, 200, 200} (logical {0, 0, 100, 100}).
    const uint32_t region1 = createRegion({{0, 0, 200, 200}});
    setVisibleRegion(hlId, region1);
    commit(surfId);
    takeDamage();

    // Switch to a completely disjoint region {400, 200, 200, 200}: with the
    // renderer's 1px mask expansion before scaling by 0.5, the old mask covers
    // logical {0, 0, 101, 101} and the new mask covers logical {199, 99, 102, 102},
    // while the untouched gap stays undamaged.
    const uint32_t region2 = createRegion({{400, 200, 200, 200}});
    setVisibleRegion(hlId, region2);
    commit(surfId);
    CRegion expectedDisjoint{50, 100, 101, 101};
    expectedDisjoint.add(CBox{249, 199, 102, 102});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedDisjoint.pixman()));

    // Reset to unrestricted, then switch to a region completely disjoint from the
    // 800x600 buffer (including touching the far edge at x=800): clipped to empty
    // before expansion, hiding the entire surface and damaging the full surface.
    setVisibleRegion(hlId, 0);
    commit(surfId);
    takeDamage();

    const uint32_t outside1 = createRegion({{800, 0, 100, 100}});
    setVisibleRegion(hlId, outside1);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Switching between two regions both disjoint from the buffer must clip to
    // empty damage (neither is expanded into the buffer).
    const uint32_t outside2 = createRegion({{1000, 1000, 50, 50}});
    setVisibleRegion(hlId, outside2);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Transitioning from an out-of-bounds mask to the left half must damage only
    // the newly visible left half (expanded by 1 buffer px -> logical width 201).
    const uint32_t leftRegion = createRegion({{0, 0, 400, 600}});
    setVisibleRegion(hlId, leftRegion);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 201, 300}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, MovedVisibleMaskIncludesRenderedOnePixelExpansion) {
    const uint32_t surfId = createSurface({50, 100, 200, 100}, 1);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    // Start with mask [10, 20) x [10, 20) at scale 1.
    const uint32_t mask1 = createRegion({{10, 10, 10, 10}});
    setVisibleRegion(hlId, mask1);
    commit(surfId);
    takeDamage();

    // Moving to [30, 40) x [10, 20) changes rendered coverage at [9, 21) and [29, 41)
    // due to CSurfacePassElement::visibleRegion()'s expand(1).
    const uint32_t mask2 = createRegion({{30, 10, 10, 10}});
    setVisibleRegion(hlId, mask2);
    commit(surfId);
    CRegion expected{59, 109, 12, 12};
    expected.add(CBox{79, 109, 12, 12});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expected.pixman()));

    // At fractional monitor scale 1.5x, moving to [50, 60) x [10, 20) must retain
    // outward pixel bounds: [floor(79*1.5), ceil(91*1.5)) = [118, 137) and
    // [floor(99*1.5), ceil(111*1.5)) = [148, 167), y in [163, 182).
    m_monitor->m_scale   = 1.5F;
    const uint32_t mask3 = createRegion({{50, 10, 10, 10}});
    setVisibleRegion(hlId, mask3);
    commit(surfId);
    CRegion expectedFractional{118, 163, 19, 19};
    expectedFractional.add(CBox{148, 163, 19, 19});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedFractional.pixman()));
}

TEST_F(CHyprlandSurfaceTest, CroppedAndScaledVisibleRegionMasks) {
    // 1. Oversized and partially out-of-bounds masks clipped to surface bounds.
    const uint32_t surfId = createSurface({50, 100, 400, 300}, 2);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    // Oversized mask covering the entire 800x600 buffer: diff lies entirely
    // outside the surface clip box, so damage must be empty.
    const uint32_t oversized = createRegion({{-100, -100, 1000, 800}});
    setVisibleRegion(hlId, oversized);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Partially out-of-bounds mask covering x in [-40, 400): only the hidden
    // right half {400, 0, 400, 600} inside the surface should be damaged.
    const uint32_t partialEdge = createRegion({{-40, 0, 440, 600}});
    setVisibleRegion(hlId, partialEdge);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));

    // 2. Non-uniform scaling via wp_viewport.set_destination (800x600 buffer ->
    //    200x300 surface: sx = 0.25, sy = 0.5).
    const uint32_t viewportId = getViewport(surfId);
    setViewportDestination(viewportId, 200, 300);
    m_view->m_box = {50, 100, 200, 300};
    request(surfId, 1, {wl_resource_get_id(m_buffers.back()->m_resource->getResource()), 0, 0}); // wl_surface.attach
    setVisibleRegion(hlId, 0);
    commit(surfId);
    ASSERT_EQ(m_hlSurface->resource()->m_current.size, Vector2D(200, 300));
    takeDamage();

    // Restrict to top-left buffer quadrant {0, 0, 400, 300} -> logical {0, 0, 100, 150}.
    // Hidden area is the complement inside {50, 100, 200, 300}.
    const uint32_t topLeftQuad = createRegion({{0, 0, 400, 300}});
    setVisibleRegion(hlId, topLeftQuad);
    commit(surfId);
    CRegion expectedViewportHidden{150, 100, 100, 150};
    expectedViewportHidden.add(CBox{50, 250, 200, 150});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedViewportHidden.pixman()));

    // Switch to disjoint bottom-right quadrant {400, 300, 400, 300} with monitor
    // scale = 2.0: old expanded logical rect {50, 100, 100.25, 150.5} and new
    // expanded logical rect {149.75, 249.5, 100.25, 150.5} project directly into
    // monitor pixel space without double-rounding.
    m_monitor->m_scale             = 2.F;
    const uint32_t bottomRightQuad = createRegion({{400, 300, 400, 300}});
    setVisibleRegion(hlId, bottomRightQuad);
    commit(surfId);
    CRegion expectedMonitorScaled{100, 200, 201, 301};
    expectedMonitorScaled.add(CBox{299, 499, 201, 301});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedMonitorScaled.pixman()));
}

TEST_F(CHyprlandSurfaceTest, ViewportSourceCropMapsVisibleMaskDamageToCroppedCoordinates) {
    // 200x100 buffer cropped via wp_viewport.set_source to its rightmost 100px: [100, 200) x [0, 100).
    const uint32_t surfId     = createSurface({50, 100, 200, 100}, 1);
    const uint32_t viewportId = getViewport(surfId);
    const uint32_t hlId       = getHyprlandSurface(surfId);

    setViewportSource(viewportId, 100.0, 0.0, 100.0, 100.0);
    m_view->m_box = {50, 100, 100, 100};
    request(surfId, 1, {wl_resource_get_id(m_buffers.back()->m_resource->getResource()), 0, 0}); // wl_surface.attach
    commit(surfId);
    ASSERT_EQ(m_hlSurface->resource()->m_current.size, Vector2D(100, 100));
    takeDamage();

    // Set initial mask at buffer x=110..120, y=10..30 -> surface x=10..20 (expanded: 9..21, y=9..31).
    const uint32_t mask1 = createRegion({{110, 10, 10, 20}});
    setVisibleRegion(hlId, mask1);
    commit(surfId);
    takeDamage();

    // Move mask to buffer x=150..160, y=10..30 -> surface x=50..60 (expanded: 49..61, y=9..31).
    // Must damage surface x=9..21 and x=49..61 (global x=59..71 and x=99..111), NOT uncropped x=55..60.
    const uint32_t mask2 = createRegion({{150, 10, 10, 20}});
    setVisibleRegion(hlId, mask2);
    commit(surfId);
    CRegion expectedCropped{59, 109, 12, 22};
    expectedCropped.add(CBox{99, 109, 12, 22});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedCropped.pixman()));

    // Changes strictly outside the cropped source region [100, 200) must not damage anything.
    const uint32_t outsideCrop1 = createRegion({{10, 10, 20, 20}});
    setVisibleRegion(hlId, outsideCrop1);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{99, 109, 12, 22}.pixman()));

    const uint32_t outsideCrop2 = createRegion({{20, 10, 20, 20}});
    setVisibleRegion(hlId, outsideCrop2);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Combine wp_viewport.set_source(100, 0, 100, 100) with set_destination(200, 50)
    // (scale x=2.0, y=0.5 relative to the cropped source).
    setViewportDestination(viewportId, 200, 50);
    m_view->m_box = {50, 100, 200, 50};
    request(surfId, 1, {wl_resource_get_id(m_buffers.back()->m_resource->getResource()), 0, 0}); // wl_surface.attach
    commit(surfId);
    ASSERT_EQ(m_hlSurface->resource()->m_current.size, Vector2D(200, 50));
    takeDamage();

    // Reveal buffer x=110..120, y=10..30 -> expanded buffer [109,121) x [9,31) ->
    // source-relative [9,21) x [9,31) -> scaled [18,42) x [4.5,15.5) -> outward [18,42) x [4,16).
    const uint32_t scaledCropMask = createRegion({{110, 10, 10, 20}});
    setVisibleRegion(hlId, scaledCropMask);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{68, 104, 24, 12}.pixman()));

    // Panning wp_viewport.set_source while an active visible mask is present
    // (without changing the mask or sending wl_surface.damage) moves the rendered
    // mask on screen and must damage the full surface box.
    setViewportSource(viewportId, 50.0, 0.0, 100.0, 100.0);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 200, 50}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, SetOpacityDamagesFullSurfaceOnlyWhenChanged) {
    const uint32_t surfId = createSurface({50, 100, 400, 300});
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    setOpacity(hlId, 0.5);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Unchanged opacity on a real commit (with pending protocol or surface update)
    // must not damage anything.
    setOpacity(hlId, 0.5);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    damageSurface(surfId, 10, 20, 30, 15);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Changing opacity and visible region together must damage the full surface.
    const uint32_t leftRegion = createRegion({{0, 0, 200, 300}});
    setOpacity(hlId, 0.75);
    setVisibleRegion(hlId, leftRegion);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, DestroyDamagesOnlyChangedVisibleRegionOnCommit) {
    const uint32_t surfId = createSurface({50, 100, 400, 300}, 2);
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    const uint32_t leftRegion = createRegion({{0, 0, 400, 600}});
    setVisibleRegion(hlId, leftRegion);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));

    destroyHyprlandSurface(hlId);
    EXPECT_FALSE(m_hlSurface->m_visibleRegion.empty());
    EXPECT_TRUE(takeDamage().empty());

    // Destroying resets visible region to empty (whole surface visible), damaging
    // only the previously hidden right half: {250, 100, 200, 300}.
    commit(surfId);
    EXPECT_TRUE(m_hlSurface->m_visibleRegion.empty());
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));

    // Re-creating and destroying with default state (opacity 1.0, empty region)
    // must not damage anything on commit.
    const uint32_t hlId2 = getHyprlandSurface(surfId);
    destroyHyprlandSurface(hlId2);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());
}
