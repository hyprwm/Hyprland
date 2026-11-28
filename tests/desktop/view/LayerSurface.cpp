#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <config/supplementary/jeremy/Jeremy.hpp>
#include <desktop/state/LayerState.hpp>
#include <desktop/view/LayerSurface.hpp>
#include <desktop/view/WLSurface.hpp>
#include <event/EventBus.hpp>
#include <layout/LayoutManager.hpp>
#include <managers/SeatManager.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <protocols/FractionalScale.hpp>
#include <protocols/LayerShell.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>
#include <protocols/core/Seat.hpp>
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

class CLayerSurfaceTestOutput : public Aquamarine::IOutput {
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

class CLayerSurfaceTestRenderer : public Render::IHyprRenderer {
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

class CLayerSurfaceTestTexture : public Render::ITexture {
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

class CLayerSurfaceTestBuffer : public IHLBuffer {
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

static bool& layerSurfaceTestMapped(PHLLS layer);

template <bool Desktop::View::CLayerSurface::* Mapped>
struct SLayerSurfaceTestAccess {
    friend bool& layerSurfaceTestMapped(PHLLS layer) {
        return layer.get()->*Mapped;
    }
};

template struct SLayerSurfaceTestAccess<&Desktop::View::CLayerSurface::m_mapped>;

class CLayerSurfacePartialDamageTest : public ::testing::Test {
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
        m_previousSeatManager    = std::move(g_pSeatManager);
        m_previousLayoutManager  = std::move(g_layoutManager);
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
        m_previousSeat           = std::move(PROTO::seat);
        m_installedGlobals       = true;

        Event::bus()                        = makeUnique<Event::CEventBus>();
        State::monitorState()               = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor                       = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay          = m_display.get();
        g_pCompositor->m_wlEventLoop        = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_explicitConfigPath = "layer-surface-test.lua";
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
        g_layoutManager         = makeUnique<Layout::CLayoutManager>();
        g_pEventLoopManager     = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
        g_pHyprRenderer         = makeUnique<CLayerSurfaceTestRenderer>();
        wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);

        PROTO::presentation  = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "layer-surface-test");
        PROTO::compositor    = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "layer-surface-test-compositor");
        PROTO::subcompositor = makeUnique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "layer-surface-test-subcompositor");
        PROTO::fractional    = makeUnique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "layer-surface-test-fractional");
        PROTO::seat          = makeUnique<CWLSeatProtocol>(&wl_seat_interface, 9, "layer-surface-test-seat");
        g_pSeatManager       = makeUnique<CSeatManager>();

        m_monitor                    = makeShared<Monitor::CMonitor>(makeShared<CLayerSurfaceTestOutput>());
        m_monitor->m_self            = m_monitor;
        m_monitor->m_id              = 1;
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
    }

    void TearDown() override {
        if (m_monitor) {
            for (auto& la : m_monitor->m_layerSurfaceLayers)
                la.clear();
        }
        for (const auto& layer : m_layers) {
            layerSurfaceTestMapped(layer) = false;
            layer->m_monitor.reset();
        }
        m_layers.clear();
        m_layerResources.clear();
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        g_pSeatManager          = std::move(m_previousSeatManager);
        PROTO::seat             = std::move(m_previousSeat);
        PROTO::subcompositor    = std::move(m_previousSubcompositor);
        PROTO::compositor       = std::move(m_previousCoreCompositor);
        PROTO::fractional       = std::move(m_previousFractional);
        PROTO::presentation     = std::move(m_previousPresentation);
        g_pHyprRenderer         = std::move(m_previousRenderer);
        g_pEventLoopManager     = std::move(m_previousEventLoop);
        g_layoutManager         = std::move(m_previousLayoutManager);
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

    SP<IHLBuffer> createBuffer(const Vector2D& size) {
        auto buffer                  = makeShared<CLayerSurfaceTestBuffer>();
        auto bufWire                 = makeShared<CWlBuffer>(m_client, 1, m_nextID++);
        buffer->m_resource           = CWLBufferResource::create(bufWire);
        buffer->m_resource->m_buffer = buffer;
        buffer->size                 = size;
        buffer->m_texture            = makeShared<CLayerSurfaceTestTexture>();
        m_buffers.emplace_back(buffer);
        return buffer;
    }

    struct SCreatedLayer {
        uint32_t                surfId = 0;
        SP<CLayerShellResource> resource;
        PHLLS                   layer;
        SP<IHLBuffer>           buffer;
    };

    SCreatedLayer createLayerSurface(const std::string& ns, const CBox& geom, uint32_t anchor = 0, const Vector2D& desiredSize = {}) {
        const uint32_t         surfId = m_nextID++;
        SP<CWLSurfaceResource> surf;
        auto                   newSurfListener = PROTO::compositor->m_events.newSurface.listen([&](SP<CWLSurfaceResource> s) { surf = s; });
        request(m_compositorId, 0, {surfId}); // wl_compositor.create_surface
        EXPECT_TRUE(surf);

        auto wire     = makeShared<CZwlrLayerSurfaceV1>(m_client, 4, m_nextID++);
        auto resource = makeShared<CLayerShellResource>(wire, surf, ns, m_monitor, ZWLR_LAYER_SHELL_V1_LAYER_TOP);
        EXPECT_TRUE(resource->good());
        wire->setDestroy([](CZwlrLayerSurfaceV1*) { ; });
        wire->setOnDestroy([](CZwlrLayerSurfaceV1*) { ; });

        const Vector2D desired          = desiredSize == Vector2D{} ? geom.size() : desiredSize;
        resource->m_pending.desiredSize = desired;
        resource->m_current.desiredSize = desired;
        resource->m_pending.anchor      = anchor;
        resource->m_current.anchor      = anchor;
        resource->m_configured          = true;
        resource->m_mapped              = true;

        auto layer                    = Desktop::View::CLayerSurface::create(resource);
        layerSurfaceTestMapped(layer) = true;
        layer->m_monitor              = m_monitor;
        layer->m_geometry             = geom;
        layer->m_position             = geom.pos();
        layer->positionAnimation()->setValueAndWarp(geom.pos());
        layer->sizeAnimation()->setValueAndWarp(geom.size());
        layer->alpha()[Desktop::View::LS_ALPHA_FADE]->setValueAndWarp(1.F);

        m_layerResources.emplace_back(resource);
        m_layers.emplace_back(layer);
        m_monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP].emplace_back(layer);

        auto buffer = createBuffer(geom.size());
        request(surfId, 1, {wl_resource_get_id(buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
        commit(surfId);
        takeDamage();

        return SCreatedLayer{
            .surfId   = surfId,
            .resource = resource,
            .layer    = layer,
            .buffer   = buffer,
        };
    }

    void commit(uint32_t surfId) {
        request(surfId, 6); // wl_surface.commit
    }

    CRegion takeDamage() {
        CRegion damage = m_monitor->m_damage.getBufferDamage(1);
        m_monitor->m_damage.rotate();
        return damage;
    }

    wl_client*                           m_client       = nullptr;
    uint32_t                             m_nextID       = 2;
    uint32_t                             m_compositorId = 0;
    PHLMONITOR                           m_monitor;
    std::vector<SP<IHLBuffer>>           m_buffers;
    std::vector<SP<CLayerShellResource>> m_layerResources;
    std::vector<PHLLS>                   m_layers;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<Render::IHyprRenderer>                                  m_previousRenderer;
    UP<CSeatManager>                                           m_previousSeatManager;
    UP<Layout::CLayoutManager>                                 m_previousLayoutManager;
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
    UP<CWLSeatProtocol>                                        m_previousSeat;
};

TEST_F(CLayerSurfacePartialDamageTest, NonLayoutCommitPreservesPartialDamageAndShrinkDamagesOldGeometry) {
    auto created = createLayerSurface("layer-partial-test", {50, 100, 400, 300});

    // Subsequent commit with partial surface damage must NOT damage the full layer geometry.
    request(created.surfId, 1, {wl_resource_get_id(created.buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
    request(created.surfId, 2, {10, 20, 30, 15});                                                      // wl_surface.damage
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{60, 120, 30, 15}.pixman()));

    // Keyboard interactivity alone (without buffer attach or surface damage) must not damage anything.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_KEYBOARD_INTERACTIVITY;
    created.resource->m_pending.keyboardInteractivity              = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
    created.resource->m_surface->m_pending.updated.bits.layershell = true;
    commit(created.surfId);
    EXPECT_TRUE(takeDamage().empty());

    // Keyboard interactivity with partial damage must preserve partial damage.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_KEYBOARD_INTERACTIVITY;
    created.resource->m_pending.keyboardInteractivity = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
    request(created.surfId, 1, {wl_resource_get_id(created.buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
    request(created.surfId, 2, {10, 20, 30, 15});                                                      // wl_surface.damage
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{60, 120, 30, 15}.pixman()));

    // Shrinking surface size without layer-shell set_size must still damage the old geometry.
    auto buf200 = createBuffer({200, 150});
    request(created.surfId, 1, {wl_resource_get_id(buf200->m_resource->getResource()), 0, 0}); // wl_surface.attach
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));
}

TEST_F(CLayerSurfacePartialDamageTest, CommittedLayoutStateDamagesChangedGeometryWithoutFullMonitorDamage) {
    auto created = createLayerSurface("layer-committed-test", {0, 0, 400, 300}, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

    // Margin change moves the layer from {0, 0, 400, 300} to {0, 10, 400, 300} without changing usableArea.
    // Only the old and new layer geometries ({0, 0, 400, 310}) should be damaged, not the entire monitor.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_MARGIN;
    created.resource->m_pending.margin.top = 10;
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{0, 0, 400, 310}.pixman()));
    EXPECT_EQ(created.layer->m_position, Vector2D(0, 10));

    // Re-committing an unchanged margin alongside partial surface damage must preserve
    // partial damage at the updated layer position rather than damaging the full box.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_MARGIN;
    created.resource->m_pending.margin.top = 10;
    request(created.surfId, 1, {wl_resource_get_id(created.buffer->m_resource->getResource()), 0, 0}); // wl_surface.attach
    request(created.surfId, 2, {10, 20, 30, 15});                                                      // wl_surface.damage
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{10, 30, 30, 15}.pixman()));

    // Re-arranging layers when nothing changed must not produce any damage.
    g_pHyprRenderer->arrangeLayersForMonitor(m_monitor->m_id);
    EXPECT_TRUE(takeDamage().empty());
}

TEST_F(CLayerSurfacePartialDamageTest, LayerPlaneChangeDamagesLayerOrMonitorWhenDimAround) {
    auto created                            = createLayerSurface("layer-plane-test", {50, 100, 400, 300}, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    created.resource->m_pending.margin.left = 50;
    created.resource->m_pending.margin.top  = 100;
    created.resource->m_current.margin.left = 50;
    created.resource->m_current.margin.top  = 100;

    // Moving a regular layer between planes with unchanged geometry damages only its box.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_LAYER;
    created.resource->m_pending.layer                              = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    created.resource->m_surface->m_pending.updated.bits.layershell = true;
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{50, 100, 400, 300}.pixman()));

    // Moving a dimAround layer between planes must damage the full monitor.
    created.layer->m_ruleApplicator->dimAround().set(true, Desktop::Types::PRIORITY_SET_PROP);
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_LAYER;
    created.resource->m_pending.layer                              = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    created.resource->m_surface->m_pending.updated.bits.layershell = true;
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{0, 0, 1920, 1080}.pixman()));
}

TEST_F(CLayerSurfacePartialDamageTest, OrderChangeDamagesReorderedLayersAndUnmappedConfigureDoesNotDamage) {
    auto first                             = createLayerSurface("layer-order-first", {0, 0, 200, 100}, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    auto second                            = createLayerSurface("layer-order-second", {100, 50, 200, 100}, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    second.resource->m_pending.margin.left = 100;
    second.resource->m_pending.margin.top  = 50;
    second.resource->m_current.margin.left = 100;
    second.resource->m_current.margin.top  = 50;
    g_pHyprRenderer->arrangeLayersForMonitor(m_monitor->m_id);
    takeDamage();

    // Swapping layer order must damage both reordered layers without full-monitor damage.
    second.layer->m_ruleApplicator->order().set(1, Desktop::Types::PRIORITY_SET_PROP);
    g_pHyprRenderer->arrangeLayersForMonitor(m_monitor->m_id);
    CRegion expectedOrder{0, 0, 200, 100};
    expectedOrder.add(CBox{100, 50, 200, 100});
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), expectedOrder.pixman()));

    // Subsequent arrange with unchanged order must not damage anything.
    g_pHyprRenderer->arrangeLayersForMonitor(m_monitor->m_id);
    EXPECT_TRUE(takeDamage().empty());

    // Initial layout arrange for an unmapped layer must update its geometry without damaging.
    auto unmapped                          = createLayerSurface("layer-unmapped-test", {0, 0, 300, 80}, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    layerSurfaceTestMapped(unmapped.layer) = false;
    unmapped.resource->m_mapped            = false;
    unmapped.layer->m_geometry             = {};
    g_pHyprRenderer->arrangeLayersForMonitor(m_monitor->m_id);
    EXPECT_EQ(unmapped.layer->m_geometry, CBox(0, 1000, 300, 80));
    EXPECT_TRUE(takeDamage().empty());
}

TEST_F(CLayerSurfacePartialDamageTest, ArrangeLayersDamagesMonitorWhenUsableAreaChanges) {
    auto created = createLayerSurface("layer-exclusive-test", {0, 0, 1920, 40},
                                      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, {0, 40});

    // Changing exclusive zone modifies usableArea, which must damage the full monitor.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_EXCLUSIVE;
    created.resource->m_pending.exclusive = 40;
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{0, 0, 1920, 1080}.pixman()));

    // Resetting exclusive zone back to 0 restores usableArea and must also damage the full monitor.
    created.resource->m_pending.committed |= CLayerShellResource::eCommittedState::STATE_EXCLUSIVE;
    created.resource->m_pending.exclusive                          = 0;
    created.resource->m_surface->m_pending.updated.bits.layershell = true;
    commit(created.surfId);
    EXPECT_TRUE(pixman_region32_equal(takeDamage().pixman(), CRegion{0, 0, 1920, 1080}.pixman()));
}
