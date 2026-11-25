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
#include <protocols/BackgroundEffect.hpp>
#include <protocols/FractionalScale.hpp>
#include <protocols/PresentationTime.hpp>
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

class CBackgroundEffectTestOutput : public Aquamarine::IOutput {
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

class CBackgroundEffectTestRenderer : public Render::IHyprRenderer {
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

class CBackgroundEffectTestTexture : public Render::ITexture {
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

class CBackgroundEffectTestBuffer : public IHLBuffer {
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

class CBackgroundEffectTestView : public Desktop::View::IView {
  public:
    CBackgroundEffectTestView(SP<Desktop::View::CWLSurface> wlSurface, const CBox& box) : IView(wlSurface), m_box(box) {
        ;
    }

    static SP<CBackgroundEffectTestView> create(SP<Desktop::View::CWLSurface> wlSurface, const CBox& box) {
        auto p = makeShared<CBackgroundEffectTestView>(wlSurface, box);
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

class CBackgroundEffectTest : public ::testing::Test {
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
        m_previousBgEffect       = std::move(PROTO::backgroundEffect);
        m_installedGlobals       = true;

        Event::bus()                        = makeUnique<Event::CEventBus>();
        State::monitorState()               = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay          = m_display.get();
        g_pCompositor->m_wlEventLoop        = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_explicitConfigPath = "background-effect-test.lua";
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
        g_pHyprRenderer         = makeUnique<CBackgroundEffectTestRenderer>();
        wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);

        PROTO::presentation     = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "bg-effect-test");
        PROTO::compositor       = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "bg-effect-test-compositor");
        PROTO::subcompositor    = makeUnique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "bg-effect-test-subcompositor");
        PROTO::fractional       = makeUnique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "bg-effect-test-fractional");
        PROTO::backgroundEffect = makeUnique<CBackgroundEffectProtocol>(&ext_background_effect_manager_v1_interface, 1, "bg-effect-test-bg");

        m_monitor                    = makeShared<Monitor::CMonitor>(makeShared<CBackgroundEffectTestOutput>());
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
        m_bgManagerId = m_nextID++;
        PROTO::backgroundEffect->bindManager(m_client, nullptr, 1, m_bgManagerId);
    }

    void TearDown() override {
        m_view.reset();
        m_hlSurface.reset();
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::backgroundEffect = std::move(m_previousBgEffect);
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

    uint32_t createSurface(const CBox& box = {50, 100, 400, 300}) {
        const uint32_t         surfId = m_nextID++;
        SP<CWLSurfaceResource> surf;
        auto                   newSurfListener = PROTO::compositor->m_events.newSurface.listen([&](SP<CWLSurfaceResource> s) { surf = s; });
        request(m_compositorId, 0, {surfId}); // wl_compositor.create_surface
        EXPECT_TRUE(surf);
        m_hlSurface = Desktop::View::CWLSurface::create();
        m_view      = CBackgroundEffectTestView::create(m_hlSurface, box);
        m_hlSurface->assign(surf, m_view);
        auto buffer                  = makeShared<CBackgroundEffectTestBuffer>();
        auto wire                    = makeShared<CWlBuffer>(m_client, 1, m_nextID++);
        buffer->m_resource           = CWLBufferResource::create(wire);
        buffer->m_resource->m_buffer = buffer;
        buffer->size                 = box.size();
        buffer->m_texture            = makeShared<CBackgroundEffectTestTexture>();
        m_buffers.emplace_back(buffer);

        request(surfId, 1, {wl_resource_get_id(buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
        request(surfId, 6);                                                                // wl_surface.commit
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

    uint32_t getBackgroundEffect(uint32_t surfId) {
        const uint32_t effectId = m_nextID++;
        request(m_bgManagerId, 1, {effectId, surfId}); // ext_background_effect_manager_v1.get_background_effect
        return effectId;
    }

    void setBlurRegion(uint32_t effectId, uint32_t regionId) {
        request(effectId, 1, {regionId}); // ext_background_effect_surface_v1.set_blur_region
    }

    void destroyBackgroundEffect(uint32_t effectId) {
        request(effectId, 0); // ext_background_effect_surface_v1.destroy
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
    uint32_t                      m_bgManagerId  = 0;
    PHLMONITOR                    m_monitor;
    SP<Desktop::View::CWLSurface> m_hlSurface;
    SP<CBackgroundEffectTestView> m_view;
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
    UP<CBackgroundEffectProtocol>                              m_previousBgEffect;
};

TEST_F(CBackgroundEffectTest, UnchangedBlurRegionDoesNotDamageOnCommit) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{0, 0, 100, 40}});
    setBlurRegion(effectId, region1);

    // Double-buffered: state and damage must not apply before commit.
    EXPECT_FALSE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(takeDamage().empty());

    // Initial commit activates the effect and damages the full surface box.
    commit(surfId);
    EXPECT_TRUE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(pixman_region32_equal(m_hlSurface->m_blurRegion.pixman(), CRegion{0, 0, 100, 40}.pixman()));
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Subsequent commit with partial surface damage only (no blur change) must
    // NOT add background-effect damage.
    request(surfId, 2, {10, 20, 30, 15}); // wl_surface.damage
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Re-setting an identical blur region must NOT damage on commit either.
    const uint32_t region2 = createRegion({{0, 0, 100, 40}});
    setBlurRegion(effectId, region2);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());
}

TEST_F(CBackgroundEffectTest, BlurRegionChangeDamagesOnlyChangedAreaClippedToSurface) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{0, 0, 100, 40}});
    setBlurRegion(effectId, region1);
    commit(surfId);
    takeDamage();

    // Grow blur region by {100, 0, 20, 40}: only {50+100, 100, 20, 40} should be
    // damaged.
    const uint32_t region2 = createRegion({{0, 0, 120, 40}});
    setBlurRegion(effectId, region2);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{150, 100, 20, 40}.pixman()));

    // Shrink blur region by {80, 0, 40, 40}: only {50+80, 100, 40, 40} should be
    // damaged.
    const uint32_t region3 = createRegion({{0, 0, 80, 40}});
    setBlurRegion(effectId, region3);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{130, 100, 40, 40}.pixman()));

    // Adding a rect entirely outside the 400x300 surface box must not damage
    // anything.
    const uint32_t region4 = createRegion({{0, 0, 80, 40}, {500, 500, 50, 50}});
    setBlurRegion(effectId, region4);
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Clearing blur region via nullptr damages only the previously visible blur
    // region.
    setBlurRegion(effectId, 0);
    commit(surfId);
    EXPECT_TRUE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 80, 40}.pixman()));
}

TEST_F(CBackgroundEffectTest, BlurRegionDisjointOverlappingAndPartiallyClippedTransitions) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{10, 10, 60, 40}});
    setBlurRegion(effectId, region1);
    commit(surfId);
    takeDamage();

    // Switching to a completely disjoint region must damage both old and new
    // rects without damaging the untouched gap between them.
    const uint32_t region2 = createRegion({{200, 120, 80, 50}});
    setBlurRegion(effectId, region2);
    commit(surfId);
    CRegion expectedDisjoint{60, 110, 60, 40};
    expectedDisjoint.add(CBox{250, 220, 80, 50});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedDisjoint.pixman()));

    // Shifting an overlapping region must damage both the removed and added
    // slices while leaving the shared overlap untouched.
    const uint32_t region3 = createRegion({{240, 120, 80, 50}});
    setBlurRegion(effectId, region3);
    commit(surfId);
    CRegion expectedShift{250, 220, 40, 50};
    expectedShift.add(CBox{330, 220, 40, 50});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedShift.pixman()));

    // Rects extending past the surface edges must be clipped to the 400x300
    // surface box rather than leaking negative or oversized damage.
    const uint32_t region4 = createRegion({{-20, -10, 60, 50}, {360, 270, 100, 100}});
    setBlurRegion(effectId, region4);
    commit(surfId);
    CRegion expectedClipped{290, 220, 80, 50};
    expectedClipped.add(CBox{50, 100, 40, 40});
    expectedClipped.add(CBox{410, 370, 40, 30});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedClipped.pixman()));
}

TEST_F(CBackgroundEffectTest, ActivationWithoutBlurRegionMarksPendingAndDamagesOnCommit) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);

    // Creating the effect without calling set_blur_region must stay pending
    // until commit, and commit must not be skipped as an empty commit.
    EXPECT_FALSE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(takeDamage().empty());

    commit(surfId);
    EXPECT_TRUE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Subsequent removal must also be double-buffered and damage on commit.
    destroyBackgroundEffect(effectId);
    EXPECT_TRUE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(takeDamage().empty());

    commit(surfId);
    EXPECT_FALSE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));
}

TEST_F(CBackgroundEffectTest, DestroyIsDoubleBufferedOnNextCommit) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{10, 10, 80, 40}});
    setBlurRegion(effectId, region1);
    commit(surfId);
    takeDamage();

    destroyBackgroundEffect(effectId);
    // Effect remains active until the next wl_surface.commit.
    EXPECT_TRUE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(pixman_region32_equal(m_hlSurface->m_blurRegion.pixman(), CRegion{10, 10, 80, 40}.pixman()));
    EXPECT_TRUE(takeDamage().empty());

    commit(surfId);
    EXPECT_FALSE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));
}

TEST_F(CBackgroundEffectTest, DestroyBeforeFirstCommitDoesNotDamage) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{10, 10, 80, 40}});
    setBlurRegion(effectId, region1);
    destroyBackgroundEffect(effectId);

    commit(surfId);
    EXPECT_FALSE(m_hlSurface->m_hasBackgroundEffect);
    EXPECT_TRUE(m_hlSurface->m_blurRegion.empty());
    EXPECT_TRUE(takeDamage().empty());
}

TEST_F(CBackgroundEffectTest, PartialSurfaceDamageWithActiveBlurRegionRemainsPartial) {
    const uint32_t surfId   = createSurface({50, 100, 400, 300});
    auto           surf     = m_hlSurface->resource();
    auto           listener = surf->m_events.commit.listen([&] { g_pHyprRenderer->damageSurface(surf, 50, 100); });

    const uint32_t effectId = getBackgroundEffect(surfId);
    const uint32_t region1  = createRegion({{0, 0, 200, 40}});
    setBlurRegion(effectId, region1);
    commit(surfId);
    takeDamage();

    // Commit partial surface damage with unchanged blur region: only {50+10,
    // 100+20, 30, 15} should be damaged.
    request(surfId, 1, {wl_resource_get_id(m_buffers.back()->m_resource->getResource()), 0, 0}); // wl_surface.attach
    request(surfId, 2, {10, 20, 30, 15});                                                        // wl_surface.damage
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{60, 120, 30, 15}.pixman()));
}

TEST_F(CBackgroundEffectTest, DamageRegionDiffHandlesEqualDisjointScaledAndClippedRegions) {
    const CBox surfaceBox{50, 100, 200, 150};

    // Equal regions short-circuit with zero damage.
    CRegion same{10, 10, 40, 30};
    g_pHyprRenderer->damageRegionDiff(same, same, surfaceBox);
    EXPECT_TRUE(takeDamage().empty());

    // Disjoint regions damage both old and new rects using the default surfaceBox clip.
    CRegion oldDisjoint{0, 0, 40, 30};
    CRegion newDisjoint{180, 130, 50, 50};
    g_pHyprRenderer->damageRegionDiff(oldDisjoint, newDisjoint, surfaceBox);
    CRegion expectedDisjoint{50, 100, 40, 30};
    expectedDisjoint.add(CBox{230, 230, 20, 20});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedDisjoint.pixman()));

    // Non-uniform scale + custom clipBox clips negative and overflowing coordinates.
    CRegion oldScaled{-40, -20, 120, 80};
    CRegion newScaled{360, 200, 200, 200};
    g_pHyprRenderer->damageRegionDiff(oldScaled, newScaled, surfaceBox, {0.5, 0.25}, CBox{0, 0, 200, 100});
    CRegion expectedScaled{50, 100, 40, 15};
    expectedScaled.add(CBox{230, 150, 20, 50});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedScaled.pixman()));

    // Fractional monitor scale (1.5x) rounds outward in pixel space rather than
    // dropping boundary column 88 = floor(59 * 1.5) and row 163 = floor(109 * 1.5).
    m_monitor->m_scale = 1.5F;
    g_pHyprRenderer->damageRegionDiff(CRegion{9, 9, 12, 12}, CRegion{}, surfaceBox);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{88, 163, 19, 19}.pixman()));

    // Monitor scale > 1 (2x) scales the resulting logical damage into monitor pixel space.
    m_monitor->m_scale = 2.F;
    g_pHyprRenderer->damageRegionDiff(CRegion{10, 20, 30, 40}, CRegion{}, surfaceBox);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{120, 240, 60, 80}.pixman()));
}
