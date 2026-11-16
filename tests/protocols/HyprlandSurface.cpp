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

namespace {
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
        SP<Render::ITexture> createTexture(bool) override {
            return nullptr;
        }
        SP<Render::ITexture> createTexture(uint32_t, uint8_t*, uint32_t, const Vector2D&, bool, bool) override {
            return nullptr;
        }
        SP<Render::ITexture> createTexture(const Aquamarine::SDMABUFAttrs&, bool) override {
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
        void drawShadow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, float, const Render::SWindowRenderPresentation&) override {
            ADD_FAILURE() << "Unexpected shadow draw";
        }
        void drawShadow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float,
                        const Render::SWindowRenderPresentation&) override {
            ADD_FAILURE() << "Unexpected shadow draw";
        }
        void drawGlow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, float) override {
            ADD_FAILURE() << "Unexpected glow draw";
        }
        void drawGlow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float) override {
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
        CHyprlandSurfaceTestView(SP<Desktop::View::CWLSurface> wlSurface, const CBox& box) : IView(wlSurface), m_box(box) {}

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
} // namespace

class CHyprlandSurfaceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {-1, -1};
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

    void setVisibleRegion(uint32_t hlId, uint32_t regionId) {
        request(hlId, 2, {regionId}); // hyprland_surface_v1.set_visible_region
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

    // Re-committing with an unchanged visible region must not damage anything.
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Clearing visible region (nullptr = whole surface visible) must damage only
    // the newly visible right half: {250, 100, 200, 300}.
    setVisibleRegion(hlId, 0);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{250, 100, 200, 300}.pixman()));
}

TEST_F(CHyprlandSurfaceTest, SetOpacityDamagesFullSurfaceOnlyWhenChanged) {
    const uint32_t surfId = createSurface({50, 100, 400, 300});
    const uint32_t hlId   = getHyprlandSurface(surfId);
    takeDamage();

    setOpacity(hlId, 0.5);
    commit(surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Unchanged opacity on next commit must not damage anything.
    commit(surfId);
    EXPECT_TRUE(takeDamage().empty());
}
