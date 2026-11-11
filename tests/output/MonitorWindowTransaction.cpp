#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <desktop/view/window/Window.hpp>
#include <desktop/view/window/WindowPresentation.hpp>
#include <desktop/view/Group.hpp>
#include <desktop/state/WindowState.hpp>
#include <desktop/state/WindowFadeout.hpp>
#include <desktop/state/FadingOutState.hpp>
#include <layout/algorithm/Algorithm.hpp>
#include <layout/algorithm/FloatingAlgorithm.hpp>
#include <layout/algorithm/TiledAlgorithm.hpp>
#include <layout/LayoutManager.hpp>
#include <layout/space/Space.hpp>
#include <layout/target/WindowGroupTarget.hpp>
#include <layout/target/WindowTarget.hpp>
#include <managers/fullscreen/FullscreenController.hpp>
#include <managers/fullscreen/handler/FullscreenHandler.hpp>
#include <animation/WorkspaceAnimationController.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <output/MonitorWindowTransaction.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/XDGShell.hpp>
#include <protocols/core/Compositor.hpp>
#include <render/ElementRenderer.hpp>
#include <render/Framebuffer.hpp>
#include <render/Renderer.hpp>
#include <render/SyncFDManager.hpp>
#include <render/decorations/DecorationPositioner.hpp>
#include <state/MonitorState.hpp>
#include <ipc/s2/S2.hpp>
#include <render/WindowRenderPresentation.hpp>
#include <workspace/HLWorkspace.hpp>
#include <workspace/RegularWorkspace.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sys/socket.h>
#include <utility>

using namespace Desktop::View;
using namespace std::chrono_literals;

class CTransactionTestWorkspace : public Workspace::CHLWorkspace {
  public:
    explicit CTransactionTestWorkspace(PHLMONITOR monitor) : CHLWorkspace(Workspace::SWorkspaceNumberedID{1}, monitor, "transaction", "1", Workspace::eWorkspaceType::NORMAL) {
        ;
    }
};

class CTransactionTestOutput : public Aquamarine::IOutput {
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

// Geometry application uses the real damage/decorations paths, but never draws.
class CTransactionTestRenderer : public Render::IHyprRenderer {
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
    SP<Render::IFramebuffer> createFB(const std::string&) override;

    bool                     m_observeSnapshotAllocations = false;
    size_t                   m_snapshotAllocationAttempts = 0;

    void                     disableScissor() override {
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

class CTransactionTestTexture : public Render::ITexture {
  public:
    explicit CTransactionTestTexture(const Vector2D& size) {
        m_size = size;
    }
    bool ok() override {
        return true;
    }
    void setTexParameter(GLenum, GLint) override {
        ADD_FAILURE() << "Unexpected texture operation";
    }
    void allocate(const Vector2D&, uint32_t) override {
        ADD_FAILURE() << "Unexpected texture allocation";
    }
    void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
        ADD_FAILURE() << "Unexpected texture upload";
    }
};

class CTransactionTestBuffer : public IHLBuffer {
  public:
    void sendRelease() override {
        ;
    }
    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return Aquamarine::BUFFER_TYPE_SHM;
    }
    void update(const CRegion&) override {
        ADD_FAILURE() << "Unexpected buffer upload";
    }
    bool isSynchronous() override {
        return true;
    }
    bool good() override {
        return true;
    }
};

class CTransactionTestFramebuffer : public Render::IFramebuffer {
  public:
    explicit CTransactionTestFramebuffer(bool allowAllocationFailure = false) : m_allowAllocationFailure(allowAllocationFailure) {
        ;
    }

    void release() override {
        ;
    }
    bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
        ADD_FAILURE() << "Unexpected snapshot readback";
        return false;
    }
    void bind() override {
        ADD_FAILURE() << "Unexpected snapshot bind";
    }
    void addStencil(SP<Render::ITexture>) override {
        ADD_FAILURE() << "Unexpected stencil allocation";
    }

  private:
    bool internalAlloc(int, int, DRMFormat) override {
        if (!m_allowAllocationFailure)
            ADD_FAILURE() << "Unexpected snapshot allocation";
        return false;
    }

    bool m_allowAllocationFailure = false;
};

SP<Render::IFramebuffer> CTransactionTestRenderer::createFB(const std::string&) {
    if (!m_observeSnapshotAllocations) {
        ADD_FAILURE() << "Unexpected framebuffer allocation";
        return nullptr;
    }

    ++m_snapshotAllocationAttempts;
    return makeShared<CTransactionTestFramebuffer>(true);
}

class CMonitorWindowTransactionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_previousCompositor         = std::move(g_pCompositor);
        m_previousEventLoop          = std::move(g_pEventLoopManager);
        m_previousRenderer           = std::move(g_pHyprRenderer);
        m_previousMonitors           = std::move(State::monitorState());
        m_previousWindows            = std::move(Desktop::windowState());
        m_previousFadeouts           = std::move(Desktop::fadingOutState());
        m_previousBus                = std::move(Event::bus());
        m_previousWatcher            = std::move(Config::watcher());
        m_previousConfig             = std::move(Config::mgr());
        m_previousAnimations         = std::move(Animation::mgr());
        m_previousTree               = std::move(Config::animationTree());
        m_previousPositioner         = std::move(g_pDecorationPositioner);
        m_previousCompositorProtocol = std::move(PROTO::compositor);
        m_previousPresentation       = std::move(PROTO::presentation);
        m_previousXDG                = std::move(PROTO::xdgShell);
        m_previousLayout             = std::move(g_layoutManager);

        Event::bus()              = makeUnique<Event::CEventBus>();
        Desktop::windowState()    = makeUnique<Desktop::CWindowState>();
        Desktop::fadingOutState() = makeUnique<Desktop::CFadingOutState>();
        State::monitorState()     = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor             = makeUnique<CCompositor>(true);
        Config::watcher()         = makeUnique<Config::CConfigWatcher>();
        Config::mgr()             = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 50;
        // Create animations before the event loop so timer expiry cannot tick them.
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        g_pDecorationPositioner = makeUnique<CDecorationPositioner>();
        g_layoutManager         = makeUnique<Layout::CLayoutManager>();

        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        g_pCompositor->m_wlDisplay    = m_display.get();
        g_pCompositor->m_wlEventLoop  = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_drm.fd       = -1;
        g_pCompositor->m_instancePath = std::string(128, 'x');
        m_previousSocket2             = std::move(IPC::Socket2::sock());
        IPC::Socket2::sock()          = makeUnique<IPC::Socket2::CSocket2>();
        Aquamarine::SBackendImplementationOptions backend;
        backend.backendType        = Aquamarine::AQ_BACKEND_NULL;
        g_pCompositor->m_aqBackend = Aquamarine::CBackend::create({backend}, Aquamarine::SBackendOptions{});
        ASSERT_TRUE(g_pCompositor->m_aqBackend);
        g_pCompositor->m_sessionActive = false;
        g_pEventLoopManager            = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
        g_pHyprRenderer                = makeUnique<CTransactionTestRenderer>();
        wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);

        std::array<int, 2> sockets{-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        Hyprutils::OS::CFileDescriptor serverSocket{sockets[0]};
        m_socket = Hyprutils::OS::CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), serverSocket.get());
        ASSERT_NE(m_client, nullptr);
        serverSocket.take();
        PROTO::compositor   = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "transaction-compositor-test");
        PROTO::presentation = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "transaction-presentation-test");
        PROTO::xdgShell     = makeUnique<CXDGShellProtocol>(&xdg_wm_base_interface, 6, "transaction-xdg-test");
        m_monitor           = makeMonitor();
    }

    void TearDown() override {
        Desktop::fadingOutState()->clear();
        Desktop::windowState()->clear();
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        m_clients.clear();
        IPC::Socket2::sock()    = std::move(m_previousSocket2);
        PROTO::xdgShell         = std::move(m_previousXDG);
        PROTO::presentation     = std::move(m_previousPresentation);
        PROTO::compositor       = std::move(m_previousCompositorProtocol);
        g_pHyprRenderer         = std::move(m_previousRenderer);
        g_pDecorationPositioner = std::move(m_previousPositioner);
        State::monitorState()   = std::move(m_previousMonitors);
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Config::mgr()           = std::move(m_previousConfig);
        Config::watcher()       = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pEventLoopManager       = std::move(m_previousEventLoop);
        g_pCompositor             = std::move(m_previousCompositor);
        Event::bus()              = std::move(m_previousBus);
        Desktop::windowState()    = std::move(m_previousWindows);
        Desktop::fadingOutState() = std::move(m_previousFadeouts);
        g_layoutManager           = std::move(m_previousLayout);
    }

    PHLMONITOR makeMonitor() {
        auto monitor                 = makeShared<Monitor::CMonitor>(makeShared<CTransactionTestOutput>());
        monitor->m_self              = monitor;
        monitor->m_windowTransaction = makeUnique<Monitor::CMonitorWindowTransaction>(monitor);
        return monitor;
    }

    PHLWINDOW makeWindow(const CBox& box = {10, 20, 100, 80}, bool registered = false) {
        auto surface            = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        surface->m_self         = surface;
        auto xdg                = makeShared<CXDGSurfaceResource>(makeShared<CXdgSurface>(m_client, 6, m_nextID++), nullptr, surface);
        xdg->m_self             = xdg;
        xdg->m_current.geometry = {{}, box.size()};
        auto window             = CWindow::create(makeUnique<CWaylandBackend>(xdg));
        if (!registered)
            Desktop::windowState()->removeSafe(window);
        window->m_monitor = m_monitor;
        window->positionAnimation()->setValueAndWarp(box.pos());
        window->sizeAnimation()->setValueAndWarp(box.size());
        window->m_isMapped = true;
        // Exercise the real backend's committed geometry without full window-map,
        // layout, focus, and input handling. Commit delivery goes to the coordinator.
        window->m_backendListeners.geometryChanged.reset();
        window->m_backendListeners.commit.reset();
        m_clients.push_back({surface, xdg});
        return window;
    }

    PHLWORKSPACE makeWorkspace(bool visible, PHLMONITOR monitor = nullptr) {
        auto workspace    = makeShared<CTransactionTestWorkspace>(monitor ? monitor : m_monitor);
        workspace->m_self = workspace;
        workspace->setVisible(visible);
        Animation::mgr()->createAnimation(Vector2D{}, workspace->m_renderOffset, Config::animationTree()->getAnimationPropertyConfig("workspacesIn"), workspace, AVARDAMAGE_ENTIRE);
        Animation::mgr()->createAnimation(1.F, workspace->m_alpha, Config::animationTree()->getAnimationPropertyConfig("workspacesIn"), workspace, AVARDAMAGE_ENTIRE);
        return workspace;
    }

    void stageWorkspace(PHLWINDOW window, PHLWORKSPACE workspace) {
        window->m_monitor->m_enabled = true;
        window->presentation().stageWorkspace();
        window->m_workspace = workspace;
        window->m_monitor   = workspace->m_monitor;
    }

    void commit(PHLWINDOW window, const Vector2D& geometry, const Vector2D& acknowledged, bool newBuffer = true, bool withBuffer = true, bool withTexture = true,
                std::optional<CBox> windowGeometry = std::nullopt) {
        const auto surface = window->backend().surface();
        ASSERT_TRUE(surface);
        const auto client = std::ranges::find_if(m_clients, [&](const auto& client) { return client.surface == surface; });
        ASSERT_NE(client, m_clients.end());
        auto buffer                            = makeShared<CTransactionTestBuffer>();
        buffer->size                           = geometry;
        buffer->m_texture                      = withTexture ? makeShared<CTransactionTestTexture>(geometry) : nullptr;
        surface->m_current.buffer              = withBuffer ? CHLBufferReference(buffer) : CHLBufferReference{};
        surface->m_current.texture             = buffer->m_texture;
        surface->m_current.size                = geometry;
        surface->m_current.bufferSize          = geometry;
        surface->m_current.ackedSize           = acknowledged;
        surface->m_current.updated.bits.buffer = newBuffer;
        client->xdg->m_current.geometry        = windowGeometry.value_or(CBox{{}, geometry});
        client->xdg->m_events.commit.emit();
        window->m_monitor->m_windowTransaction->onWindowCommit(window);
    }

    CBox appliedGoal(PHLWINDOW window) {
        return window->CGeometricMovableAnimated::geometricBox(IGeometric::GEOMETRIC_GOAL);
    }

    bool pending(PHLWINDOW window) {
        return window->m_pendingGeometry.has_value();
    }

    bool ownedBy(PHLWINDOW window, PHLMONITOR monitor) {
        return window->m_geometryTransaction.get() == monitor->m_windowTransaction.get();
    }

    void stageDrag(SP<Layout::ITarget> target) {
        // Exercise the real release boundary without pointer input, hit testing, or layout setup.
        // The default invalid mode skips the input-specific drop hooks.
        g_layoutManager->dragController()->m_target = target;
    }

    SP<CGroup> makeDragGroup(PHLWINDOW first, PHLWINDOW second) {
        *CConfigValue<Config::BOOL>("group:groupbar:enabled").ptr() = false;
        // A temporary space lets the public group constructor swap its target without a layout.
        const auto workspace = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
        workspace->space()->setAlgorithmProvider(nullptr);
        first->windowTarget()->setSpaceGhost(workspace->space());
        auto group = CGroup::create({first, second});
        for (const auto& window : {first, second}) {
            window->windowTarget()->setSpaceGhost(nullptr);
        }
        group->target()->setSpaceGhost(nullptr);
        return group;
    }

    void unmap(PHLWINDOW window) {
        window->m_isMapped = false;
    }

    SP<Desktop::CWindowFadeout> closeSnapshot(PHLWINDOW window) {
        unmap(window);
        auto fadeout = Desktop::CWindowFadeout::create(window, makeShared<CTransactionTestFramebuffer>(), 1.F);
        Desktop::fadingOutState()->add(fadeout);
        return fadeout;
    }

    void prepareNullBufferUnmap(PHLWINDOW window) {
        // Exercise the actual surface/XDG/backend ordering without desktop focus
        // and layout teardown, which the remaining transaction tests drive directly.
        window->m_backendListeners.unmap.reset();
        const auto client = std::ranges::find_if(m_clients, [&](const auto& client) { return client.surface == window->backend().surface(); });
        ASSERT_NE(client, m_clients.end());
        client->xdg->m_mapped = true;
        client->surface->map();
    }

    bool capturedCloseSnapshot(PHLWINDOW window) {
        return window->m_closeSnapshots.has_value();
    }

    void captureWithGeometryBlocked(PHLWINDOW window) {
        window->m_geometryTransactionsBlocked = true;
        window->captureCloseSnapshots();
        window->m_geometryTransactionsBlocked = false;
    }

    SP<CEventLoopTimer> timer(PHLMONITOR monitor) {
        return monitor->m_windowTransaction->m_timeout;
    }

    void expire(PHLMONITOR monitor) {
        timer(monitor)->updateTimeout(-1ms);
        g_pEventLoopManager->onTimerFire();
    }

    PHLMONITOR m_monitor;

  private:
    struct SClient {
        SP<CWLSurfaceResource>  surface;
        SP<CXDGSurfaceResource> xdg;
    };
    std::vector<SClient>                                       m_clients;
    uint32_t                                                   m_nextID = 2;
    wl_client*                                                 m_client = nullptr;
    Hyprutils::OS::CFileDescriptor                             m_socket;
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};

    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<Render::IHyprRenderer>                                  m_previousRenderer;
    UP<Event::CEventBus>                                       m_previousBus;
    UP<Config::CConfigWatcher>                                 m_previousWatcher;
    UP<Config::IConfigManager>                                 m_previousConfig;
    UP<Animation::CHyprAnimationManager>                       m_previousAnimations;
    UP<Config::CAnimationTreeController>                       m_previousTree;
    UP<CDecorationPositioner>                                  m_previousPositioner;
    UP<State::CMonitorStateTracker>                            m_previousMonitors;
    UP<CWLCompositorProtocol>                                  m_previousCompositorProtocol;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<CXDGShellProtocol>                                      m_previousXDG;
    UP<IPC::Socket2::CSocket2>                                 m_previousSocket2;
    UP<Desktop::CWindowState>                                  m_previousWindows;
    UP<Desktop::CFadingOutState>                               m_previousFadeouts;
    UP<Layout::CLayoutManager>                                 m_previousLayout;
};

TEST_F(CMonitorWindowTransactionTest, ZeroTimeoutAppliesLayoutGeometryAndWorkspaceWithoutWaiting) {
    *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 0;
    const auto workspace                                             = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
    auto       window                                                = makeWindow();
    window->m_workspace                                              = workspace;
    m_monitor->m_enabled                                             = true;
    window->windowTarget()->setSpaceGhost(workspace->space());
    window->windowTarget()->setFloatingInitial(true);
    const CBox goal{30, 40, 200, 160};

    window->windowTarget()->setPositionGlobal(goal);
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(timer(m_monitor)->armed());

    const auto destination = makeWorkspace(true);
    stageWorkspace(window, destination);
    EXPECT_EQ(window->presentation().workspace(), destination);
    EXPECT_FALSE(window->presentation().hasPendingState());
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, ZeroTimeoutPublishesFullscreenAndPeerOpacityWithoutWaiting) {
    using namespace Fullscreen;
    *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 0;
    *CConfigValue<Config::BOOL>("animations:enabled").ptr()          = false;
    const auto workspace                                             = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
    workspace->setVisible(true);
    m_monitor->m_enabled         = true;
    m_monitor->m_activeWorkspace = workspace;
    auto window                  = makeWindow({10, 20, 100, 80}, true);
    auto peer                    = makeWindow({10, 20, 100, 80}, true);
    for (const auto& w : {window, peer}) {
        w->m_workspace = workspace;
        w->windowTarget()->setSpaceGhost(workspace->space());
    }
    const auto handler = workspace->space()->algorithm()->tiledAlgo()->getFSHandler();

    for (const auto mode : {FSMODE_FULLSCREEN, FSMODE_NONE}) {
        SCOPED_TRACE(mode);
        window->presentation().stageFullscreen();
        handler->setTargetFullscreenModeInternal(window->windowTarget(), mode);
        handler->setNoMembersAboveFullscreen();
        Animation::Workspace::setFullscreenFadeAnimation(workspace, mode == FSMODE_FULLSCREEN ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);

        EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, mode);
        EXPECT_EQ(controller()->getPresentedFullscreenModes(workspace).internal, mode);
        EXPECT_FLOAT_EQ(peer->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN), mode == FSMODE_FULLSCREEN ? 0.F : 1.F);
        EXPECT_FALSE(workspace->m_fullscreenPresentation);
        EXPECT_FALSE(window->presentation().hasPendingState());
        EXPECT_FALSE(peer->presentation().hasPendingState());
        EXPECT_FALSE(pending(window));
        EXPECT_FALSE(timer(m_monitor)->armed());
    }
}

TEST_F(CMonitorWindowTransactionTest, SwitchingToZeroReleasesPendingPresentationAtASafeBoundary) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    for (const bool rendering : {false, true}) {
        SCOPED_TRACE(rendering);
        *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 50;
        const auto workspace                                             = makeWorkspace(true);
        auto       window                                                = makeWindow({0, 0, 100, 80}, true);
        auto       peer                                                  = makeWindow({100, 0, 100, 80}, true);
        auto       closed                                                = makeWindow({200, 0, 100, 80});
        window->m_workspace = peer->m_workspace = closed->m_workspace = workspace;
        m_monitor->m_enabled                                          = true;
        window->presentation().stageFullscreen();
        auto&      transaction = *m_monitor->m_windowTransaction;
        const CBox goal{0, 0, 200, 160}, peerGoal{200, 0, 200, 160};
        transaction.queue(window, goal);
        transaction.queue(peer, peerGoal);
        const auto before  = appliedGoal(window);
        const auto fadeout = closeSnapshot(closed);
        ASSERT_TRUE(fadeout);
        ASSERT_TRUE(workspace->m_fullscreenPresentation);
        ASSERT_FALSE(fadeout->done());

        if (rendering)
            transaction.onPreRender();
        else
            transaction.beginLayoutUpdate();
        *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 0;
        // Only one participant commits; zero must release the other without bypassing the active guard.
        commit(window, goal.size(), goal.size());
        EXPECT_EQ(appliedGoal(window), before);
        EXPECT_TRUE(pending(peer));
        EXPECT_TRUE(workspace->m_fullscreenPresentation);
        EXPECT_FALSE(fadeout->done());

        if (rendering)
            transaction.onPostRender();
        else
            transaction.endLayoutUpdate();
        EXPECT_EQ(appliedGoal(window), goal);
        EXPECT_EQ(appliedGoal(peer), peerGoal);
        EXPECT_FALSE(pending(window));
        EXPECT_FALSE(pending(peer));
        EXPECT_FALSE(window->presentation().hasPendingState());
        EXPECT_FALSE(peer->presentation().hasPendingState());
        EXPECT_FALSE(workspace->m_fullscreenPresentation);
        EXPECT_FALSE(transaction.hasPendingPresentation());
        EXPECT_TRUE(fadeout->done());
        EXPECT_FALSE(timer(m_monitor)->armed());
        Desktop::windowState()->clear();
    }
}

TEST_F(CMonitorWindowTransactionTest, RelayoutAfterSwitchingToZeroPreservesPendingFullscreenPresentation) {
    using namespace Fullscreen;
    *CConfigValue<Config::FLOAT>("decoration:fullscreen_opacity").ptr() = 0.4F;
    *CConfigValue<Config::BOOL>("animations:enabled").ptr()             = false;
    const auto workspace                                                = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
    workspace->setVisible(true);
    m_monitor->m_enabled         = true;
    m_monitor->m_activeWorkspace = workspace;
    auto window                  = makeWindow({0, 0, 400, 300}, true);
    window->m_workspace          = workspace;
    window->windowTarget()->setSpaceGhost(workspace->space());
    const auto handler = workspace->space()->algorithm()->tiledAlgo()->getFSHandler();
    handler->setTargetFullscreenModeInternal(window->windowTarget(), FSMODE_FULLSCREEN);
    window->presentation().refreshValues();
    window->presentation().alpha(WINDOW_ALPHA_ACTIVE)->warp();
    ASSERT_FLOAT_EQ(window->presentation().alphaValue(WINDOW_ALPHA_ACTIVE), 0.4F);

    window->presentation().stageFullscreen();
    handler->setTargetFullscreenModeInternal(window->windowTarget(), FSMODE_NONE);
    auto& transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, {10, 20, 200, 160});
    *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 0;
    const CBox latest{20, 30, 220, 180};
    window->setBox(latest);
    window->presentation().updateDecorations();
    EXPECT_TRUE(pending(window));
    EXPECT_TRUE(window->presentation().hasPendingState());
    transaction.onPreRender();
    transaction.onPostRender();

    EXPECT_EQ(appliedGoal(window), latest);
    EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, FSMODE_NONE);
    EXPECT_FLOAT_EQ(window->presentation().alphaValue(WINDOW_ALPHA_ACTIVE), 1.F);
    EXPECT_FALSE(window->presentation().hasPendingState());
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, CoalescesMovesAndPublishesGoalsBeforeApplyingTheBatch) {
    auto       first        = makeWindow();
    auto       second       = makeWindow({120, 20, 100, 80});
    const auto firstBefore  = appliedGoal(first);
    const auto secondBefore = appliedGoal(second);
    const CBox latest{40, 50, 100, 80}, other{160, 60, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(first, {30, 40, 100, 80});
    transaction.queue(second, other);
    transaction.queue(first, latest);
    transaction.queue(first, latest);

    EXPECT_EQ(first->geometricBox(IGeometric::GEOMETRIC_GOAL), latest);
    EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_GOAL), other);
    EXPECT_EQ(first->geometricBox(IGeometric::GEOMETRIC_CURRENT), firstBefore);
    EXPECT_EQ(appliedGoal(first), firstBefore);
    EXPECT_EQ(appliedGoal(second), secondBefore);

    transaction.onPreRender();
    EXPECT_EQ(appliedGoal(first), latest);
    EXPECT_EQ(appliedGoal(second), other);
    EXPECT_FALSE(pending(first));
    EXPECT_FALSE(pending(second));
    EXPECT_FALSE(timer(m_monitor)->armed());
    transaction.onPostRender();
}

TEST_F(CMonitorWindowTransactionTest, IdenticalGoalIsANoopUnlessTheWindowIsAlreadyHeld) {
    auto  window      = makeWindow();
    auto& transaction = *m_monitor->m_windowTransaction;
    EXPECT_FALSE(transaction.queue(window, appliedGoal(window)));
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(timer(m_monitor)->armed());

    const CBox goal{10, 20, 200, 160};
    EXPECT_TRUE(transaction.queue(window, goal));
    EXPECT_TRUE(transaction.queue(window, goal));
    EXPECT_TRUE(pending(window));
    expire(m_monitor);
    EXPECT_FALSE(transaction.queue(window, goal));
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, ResizeHoldsMovesUntilTheFiftyMillisecondDeadline) {
    auto       resized     = makeWindow();
    auto       moved       = makeWindow({120, 20, 100, 80});
    const auto before      = appliedGoal(resized);
    const auto otherBefore = appliedGoal(moved);
    const CBox resize{10, 20, 200, 160}, move{240, 20, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    const auto started     = Time::steadyNow();
    transaction.queue(resized, resize);
    transaction.queue(moved, move);
    ASSERT_TRUE(timer(m_monitor)->armed());
    const auto remaining = timer(m_monitor)->leftUs();
    const auto elapsed   = std::chrono::duration_cast<std::chrono::microseconds>(Time::steadyNow() - started).count();
    EXPECT_LE(remaining, 50'000.F);
    EXPECT_GE(remaining, 50'000.F - elapsed - 1.F);

    transaction.onPreRender();
    transaction.onPostRender();
    EXPECT_EQ(appliedGoal(resized), before);
    EXPECT_EQ(appliedGoal(moved), otherBefore);
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(resized), resize);
    EXPECT_EQ(appliedGoal(moved), move);
    EXPECT_FALSE(pending(resized));
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, EveryResizeMustCommitBeforeTheBatchIsReleased) {
    for (const bool reverse : {false, true}) {
        for (const bool rendering : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "reverse=" << reverse << ", rendering=" << rendering);
            auto       first  = makeWindow();
            auto       second = makeWindow();
            const auto before = appliedGoal(first);
            const CBox firstGoal{10, 20, 200, 160}, secondGoal{210, 20, 300, 240};
            auto&      transaction = *m_monitor->m_windowTransaction;
            transaction.queue(first, firstGoal);
            transaction.queue(second, secondGoal);
            if (rendering)
                transaction.onPreRender();

            commit(reverse ? second : first, reverse ? secondGoal.size() : firstGoal.size(), reverse ? secondGoal.size() : firstGoal.size());
            if (rendering)
                transaction.onPostRender();
            EXPECT_EQ(appliedGoal(first), before);
            EXPECT_EQ(appliedGoal(second), before);
            EXPECT_TRUE(timer(m_monitor)->armed());

            transaction.onPreRender();
            EXPECT_EQ(appliedGoal(first), before);
            EXPECT_EQ(appliedGoal(second), before);
            transaction.onPostRender();

            commit(reverse ? first : second, reverse ? firstGoal.size() : secondGoal.size(), reverse ? firstGoal.size() : secondGoal.size());
            EXPECT_EQ(appliedGoal(first), firstGoal);
            EXPECT_EQ(appliedGoal(second), secondGoal);
            EXPECT_FALSE(pending(first));
            EXPECT_FALSE(pending(second));
            EXPECT_FALSE(timer(m_monitor)->armed());
        }
    }
}

TEST_F(CMonitorWindowTransactionTest, PartiallyReadyBatchStillAppliesAtTheOriginalDeadline) {
    auto       first  = makeWindow();
    auto       second = makeWindow();
    const auto before = appliedGoal(first);
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(first, goal);
    transaction.queue(second, goal);
    timer(m_monitor)->updateTimeout(-1ms);
    commit(first, goal.size(), goal.size());
    EXPECT_EQ(appliedGoal(first), before);
    EXPECT_TRUE(timer(m_monitor)->passed());
    g_pEventLoopManager->onTimerFire();
    EXPECT_EQ(appliedGoal(first), goal);
    EXPECT_EQ(appliedGoal(second), goal);
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, MovesDoNotRequireACommitAlongsideAReadyResize) {
    auto       resized = makeWindow();
    auto       moved   = makeWindow();
    const CBox resize{10, 20, 200, 160}, move{210, 20, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(resized, resize);
    transaction.queue(moved, move);
    commit(resized, resize.size(), resize.size());
    EXPECT_EQ(appliedGoal(resized), resize);
    EXPECT_EQ(appliedGoal(moved), move);
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, RemovingOrUnmappingTheLastBlockerReleasesReadyResizes) {
    for (const bool unmapped : {false, true}) {
        SCOPED_TRACE(unmapped);
        auto       ready   = makeWindow();
        auto       waiting = makeWindow();
        const auto before  = appliedGoal(ready);
        const CBox goal{10, 20, 200, 160};
        auto&      transaction = *m_monitor->m_windowTransaction;
        transaction.queue(ready, goal);
        transaction.queue(waiting, goal);
        commit(ready, goal.size(), goal.size());
        EXPECT_EQ(appliedGoal(ready), before);
        if (unmapped)
            unmap(waiting);
        else
            transaction.remove(waiting);

        transaction.onPreRender();
        EXPECT_EQ(appliedGoal(ready), goal);
        EXPECT_EQ(appliedGoal(waiting), before);
        EXPECT_FALSE(timer(m_monitor)->armed());
        transaction.onPostRender();
    }
}

TEST_F(CMonitorWindowTransactionTest, AckOnlyWrongGeometryAndMissingBufferOrTextureDoNotRelease) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    const CBox goal{10, 20, 200, 160};
    m_monitor->m_windowTransaction->queue(window, goal);

    commit(window, goal.size(), goal.size(), false);
    EXPECT_EQ(appliedGoal(window), before);
    commit(window, {199, 160}, goal.size());
    EXPECT_EQ(appliedGoal(window), before);
    commit(window, goal.size(), before.size());
    EXPECT_EQ(appliedGoal(window), before);
    commit(window, goal.size(), goal.size(), true, false);
    EXPECT_EQ(appliedGoal(window), before);
    commit(window, goal.size(), goal.size(), true, true, false);
    EXPECT_EQ(appliedGoal(window), before);
    EXPECT_TRUE(pending(window));

    commit(window, goal.size(), goal.size());
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, SupersededConfigureCannotReleaseTheLatestResize) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    const CBox oldGoal{10, 20, 200, 160}, latest{30, 40, 300, 240};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, oldGoal);
    transaction.queue(window, latest);
    commit(window, oldGoal.size(), oldGoal.size());
    EXPECT_EQ(appliedGoal(window), before);
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), latest);
    commit(window, latest.size(), latest.size());
    EXPECT_EQ(appliedGoal(window), latest);
}

TEST_F(CMonitorWindowTransactionTest, UnsetWindowGeometryUsesTheCommittedSurfaceSize) {
    auto       window = makeWindow();
    const CBox goal{10, 20, 200, 160};
    m_monitor->m_windowTransaction->queue(window, goal);
    commit(window, goal.size(), goal.size(), true, true, true, CBox{});
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, WindowGeometryIsClippedToTheCommittedSurfaceBounds) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    const CBox goal{10, 20, 200, 160};
    m_monitor->m_windowTransaction->queue(window, goal);

    commit(window, before.size(), goal.size(), true, true, true, CBox{{}, goal.size()});
    EXPECT_EQ(appliedGoal(window), before);
    EXPECT_TRUE(pending(window));

    commit(window, {220, 180}, goal.size(), true, true, true, CBox{10, 10, 200, 160});
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, ReplacingAReadyResizeInvalidatesItsBufferButNotAnExpiredDeadline) {
    for (const bool timeout : {false, true}) {
        SCOPED_TRACE(timeout);
        auto       window = makeWindow();
        const auto before = appliedGoal(window);
        const CBox first{10, 20, 200, 160}, latest{20, 30, 300, 240};
        auto&      transaction = *m_monitor->m_windowTransaction;
        transaction.queue(window, first);
        transaction.onPreRender();
        commit(window, first.size(), first.size());
        if (timeout)
            expire(m_monitor);
        transaction.queue(window, latest);
        transaction.onPostRender();

        EXPECT_EQ(appliedGoal(window), timeout ? latest : before);
        if (!timeout) {
            EXPECT_TRUE(pending(window));
            commit(window, latest.size(), latest.size());
            EXPECT_EQ(appliedGoal(window), latest);
        }
    }
}

TEST_F(CMonitorWindowTransactionTest, RemovingTheReadyWindowDoesNotReleaseOtherResizes) {
    auto       ready   = makeWindow();
    auto       waiting = makeWindow();
    const auto before  = appliedGoal(waiting);
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(ready, goal);
    transaction.queue(waiting, goal);
    transaction.onPreRender();
    commit(ready, goal.size(), goal.size());
    transaction.remove(ready);
    transaction.onPostRender();
    EXPECT_EQ(appliedGoal(waiting), before);
    EXPECT_TRUE(pending(waiting));
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(waiting), goal);
}

TEST_F(CMonitorWindowTransactionTest, ReleasingASynchronousBufferDoesNotLoseReadiness) {
    auto       window = makeWindow();
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, goal);
    transaction.onPreRender();
    commit(window, goal.size(), goal.size());
    window->backend().surface()->m_current.buffer = {};
    transaction.onPostRender();
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, RequeueAndAdditionalWindowsDoNotRestartTheDeadline) {
    auto       first  = makeWindow();
    auto       second = makeWindow();
    const CBox latest{10, 20, 300, 240}, other{320, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(first, {10, 20, 200, 160});
    // An already-passed deadline makes accidental rearming observable without sleep.
    timer(m_monitor)->updateTimeout(-1ms);
    transaction.queue(first, latest);
    transaction.queue(second, other);
    EXPECT_TRUE(timer(m_monitor)->passed());
    g_pEventLoopManager->onTimerFire();
    EXPECT_EQ(appliedGoal(first), latest);
    EXPECT_EQ(appliedGoal(second), other);
}

TEST_F(CMonitorWindowTransactionTest, CommitAndTimeoutDuringRenderingWaitForPostRender) {
    for (const bool timeout : {false, true}) {
        SCOPED_TRACE(timeout);
        auto       window = makeWindow();
        const auto before = appliedGoal(window);
        const CBox goal{10, 20, 200, 160};
        auto&      transaction = *m_monitor->m_windowTransaction;
        transaction.queue(window, goal);
        transaction.onPreRender();
        if (timeout)
            expire(m_monitor);
        else
            commit(window, goal.size(), goal.size());
        EXPECT_EQ(appliedGoal(window), before);
        EXPECT_TRUE(pending(window));
        transaction.onPostRender();
        EXPECT_EQ(appliedGoal(window), goal);
        EXPECT_FALSE(pending(window));
    }
}

TEST_F(CMonitorWindowTransactionTest, SupersedingResizeWithMoveCanApplyAtPreRender) {
    auto       window = makeWindow();
    const CBox move{30, 40, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, {30, 40, 200, 160});
    transaction.queue(window, move);
    transaction.onPreRender();
    EXPECT_EQ(appliedGoal(window), move);
    EXPECT_FALSE(pending(window));
    transaction.onPostRender();
}

TEST_F(CMonitorWindowTransactionTest, WindowMoveResizeAndSetBoxUpdateTheStagedGeometry) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    m_monitor->m_windowTransaction->queue(window, {30, 40, 200, 160});
    window->move({50, 60});
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), CBox(50, 60, 200, 160));
    window->resize({300, 240});
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), CBox(50, 60, 300, 240));
    const CBox latest{70, 80, 400, 320};
    window->setBox(latest);
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), latest);
    EXPECT_EQ(appliedGoal(window), before);
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(window), latest);
}

TEST_F(CMonitorWindowTransactionTest, FinishAnimationDefersWarpAndPreservesItAcrossSupersession) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    const CBox oldAnimation{20, 30, 110, 90};
    window->setBox(oldAnimation);
    const CBox latest{50, 60, 300, 240};
    m_monitor->m_windowTransaction->queue(window, {30, 40, 200, 160});
    window->finishAnimation();
    m_monitor->m_windowTransaction->queue(window, latest);
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), before);
    EXPECT_EQ(appliedGoal(window), oldAnimation);
    EXPECT_TRUE(window->positionAnimation()->isBeingAnimated());
    EXPECT_TRUE(window->sizeAnimation()->isBeingAnimated());
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(window), latest);
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), latest);
    EXPECT_FALSE(window->positionAnimation()->isBeingAnimated());
    EXPECT_FALSE(window->sizeAnimation()->isBeingAnimated());
}

TEST_F(CMonitorWindowTransactionTest, DragReleaseConsumesMotionWarpBeforeDropGeometry) {
    for (const bool grouped : {false, true}) {
        for (const bool renderBeforeRelease : {false, true}) {
            SCOPED_TRACE(std::format("grouped={}, renderBeforeRelease={}", grouped, renderBeforeRelease));
            const CBox             initial{10, 20, 100, 80};
            const CBox             motion{70, 90, 100, 80};
            const CBox             drop{30, 40, 200, 160};
            std::vector<PHLWINDOW> windows{makeWindow(initial)};
            SP<CGroup>             group;
            SP<Layout::ITarget>    target = windows.front()->windowTarget();

            if (grouped) {
                windows.emplace_back(makeWindow(initial));
                group  = makeDragGroup(windows.front(), windows.back());
                target = group->target();
            }

            auto& transaction = *m_monitor->m_windowTransaction;
            for (const auto& window : windows) {
                window->positionAnimation()->setValueAndWarp(initial.pos());
                window->sizeAnimation()->setValueAndWarp(initial.size());
                transaction.queue(window, motion);
            }
            stageDrag(target);
            target->warpPositionSize();

            if (renderBeforeRelease) {
                transaction.onPreRender();
                transaction.onPostRender();
            }

            const auto peer       = makeWindow();
            const auto peerBefore = appliedGoal(peer);
            transaction.queue(peer, drop);

            ASSERT_TRUE(g_layoutManager->endDragTarget());
            EXPECT_FALSE(g_layoutManager->dragController()->target());
            for (const auto& window : windows) {
                EXPECT_FALSE(pending(window));
                EXPECT_FALSE(ownedBy(window, m_monitor));
                EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), motion);
                EXPECT_EQ(appliedGoal(window), motion);
                transaction.queue(window, drop);
            }
            EXPECT_TRUE(pending(peer));
            EXPECT_EQ(appliedGoal(peer), peerBefore);

            expire(m_monitor);
            for (const auto& window : windows) {
                EXPECT_EQ(appliedGoal(window), drop);
                EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), motion);
                EXPECT_TRUE(window->positionAnimation()->isBeingAnimated());
                EXPECT_TRUE(window->sizeAnimation()->isBeingAnimated());
            }
        }
    }
}

TEST_F(CMonitorWindowTransactionTest, ReentrantDragReleaseDoesNotFlushNewDropGeometry) {
    const auto first  = makeWindow();
    const auto second = makeWindow();
    const auto group  = makeDragGroup(first, second);
    const CBox motion{70, 90, 100, 80};
    const CBox drop{30, 40, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(first, motion);
    transaction.queue(second, motion);
    stageDrag(group->target());
    group->target()->warpPositionSize();

    int  ended    = 0;
    auto listener = g_layoutManager->dragController()->m_events.ended.listen([&] { ++ended; });
    first->positionAnimation()->setCallbackOnEnd([&](auto) {
        EXPECT_TRUE(g_layoutManager->endDragTarget());
        transaction.queue(second, drop);
    });

    EXPECT_TRUE(g_layoutManager->endDragTarget());
    EXPECT_EQ(ended, 1);
    EXPECT_FALSE(g_layoutManager->dragController()->target());
    EXPECT_TRUE(pending(second));
    EXPECT_EQ(appliedGoal(second), motion);
    EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_CURRENT), motion);

    expire(m_monitor);
    EXPECT_EQ(appliedGoal(second), drop);
    EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_CURRENT), motion);
    EXPECT_TRUE(second->positionAnimation()->isBeingAnimated());
    EXPECT_TRUE(second->sizeAnimation()->isBeingAnimated());
}

TEST_F(CMonitorWindowTransactionTest, DisabledAnimationWarpsOnlyItsOwnGeometryVariable) {
    for (const bool disablePosition : {false, true}) {
        SCOPED_TRACE(disablePosition);
        auto       window   = makeWindow();
        const auto before   = appliedGoal(window);
        auto&      disabled = disablePosition ? window->positionAnimation() : window->sizeAnimation();
        disabled->setConfig(Config::animationTree()->getAnimationPropertyConfig("borderangle"));
        ASSERT_FALSE(disabled->enabled());
        const CBox goal{30, 40, 200, 160};
        m_monitor->m_windowTransaction->queue(window, goal);
        expire(m_monitor);
        EXPECT_EQ(appliedGoal(window), goal);
        EXPECT_EQ(window->position(IGeometric::GEOMETRIC_CURRENT), disablePosition ? goal.pos() : before.pos());
        EXPECT_EQ(window->size(IGeometric::GEOMETRIC_CURRENT), disablePosition ? before.size() : goal.size());
        EXPECT_EQ(window->positionAnimation()->isBeingAnimated(), !disablePosition);
        EXPECT_EQ(window->sizeAnimation()->isBeingAnimated(), disablePosition);
    }
}

TEST_F(CMonitorWindowTransactionTest, DisabledMoveIsVisibleInThePreRenderFrame) {
    auto window = makeWindow();
    window->positionAnimation()->setConfig(Config::animationTree()->getAnimationPropertyConfig("borderangle"));
    ASSERT_FALSE(window->positionAnimation()->enabled());

    const CBox goal{70, 90, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, goal);
    transaction.onPreRender();
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), goal);
    EXPECT_FALSE(window->positionAnimation()->isBeingAnimated());
    transaction.onPostRender();
}

TEST_F(CMonitorWindowTransactionTest, GestureHandoffAppliesOnlyItsWindowAndDetachesItFromTheBatch) {
    auto       gesture = makeWindow();
    auto       waiting = makeWindow();
    const auto before  = appliedGoal(waiting);
    const CBox goal{30, 40, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(gesture, goal);
    transaction.queue(waiting, goal);

    gesture->flushPendingGeometry();
    EXPECT_EQ(appliedGoal(gesture), goal);
    EXPECT_FALSE(pending(gesture));
    EXPECT_FALSE(ownedBy(gesture, m_monitor));
    EXPECT_EQ(appliedGoal(waiting), before);
    EXPECT_TRUE(pending(waiting));

    const CBox intermediate{50, 60, 140, 120};
    gesture->positionAnimation()->setValueAndWarp(intermediate.pos());
    gesture->sizeAnimation()->setValueAndWarp(intermediate.size());
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(gesture), intermediate);
    EXPECT_EQ(appliedGoal(waiting), goal);
}

TEST_F(CMonitorWindowTransactionTest, RemovingAWindowDiscardsItsGoalWithoutDroppingOtherWindows) {
    auto       removed = makeWindow();
    auto       kept    = makeWindow();
    const auto before  = appliedGoal(removed);
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(removed, goal);
    transaction.queue(kept, goal);
    transaction.remove(removed);
    EXPECT_FALSE(pending(removed));
    EXPECT_FALSE(ownedBy(removed, m_monitor));
    EXPECT_EQ(removed->geometricBox(IGeometric::GEOMETRIC_GOAL), before);
    EXPECT_TRUE(timer(m_monitor)->armed());
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(removed), before);
    EXPECT_EQ(appliedGoal(kept), goal);
}

TEST_F(CMonitorWindowTransactionTest, ClearDiscardsDeferredApplyAndAllowsAFreshBatch) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(window, goal);
    transaction.onPreRender();
    expire(m_monitor);
    transaction.clear();
    transaction.onPostRender();
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), before);
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(ownedBy(window, m_monitor));
    EXPECT_FALSE(timer(m_monitor)->armed());

    transaction.queue(window, goal);
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, SameBoxMigrationTransfersOwnershipAndWarpAndIgnoresOldCleanup) {
    auto       window = makeWindow();
    auto       next   = makeMonitor();
    const auto before = appliedGoal(window);
    const CBox latest{50, 60, 300, 240};
    m_monitor->m_windowTransaction->queue(window, latest);
    window->finishAnimation();
    window->m_monitor = next;
    next->m_windowTransaction->queue(window, latest);
    EXPECT_FALSE(timer(m_monitor)->armed());
    EXPECT_TRUE(ownedBy(window, next));
    m_monitor->m_windowTransaction->remove(window);
    m_monitor->m_windowTransaction->clear();
    EXPECT_TRUE(pending(window));
    EXPECT_EQ(appliedGoal(window), before);
    expire(next);
    EXPECT_EQ(appliedGoal(window), latest);
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), latest);
    EXPECT_FALSE(pending(window));
}

TEST_F(CMonitorWindowTransactionTest, UnmappedAndDestroyedWindowsAreSkippedWithoutHoldingTheBatch) {
    auto       unmapped  = makeWindow();
    auto       destroyed = makeWindow();
    auto       kept      = makeWindow();
    const auto before    = appliedGoal(unmapped);
    const CBox goal{10, 20, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(unmapped, goal);
    transaction.queue(destroyed, goal);
    transaction.queue(kept, goal);
    unmap(unmapped);
    PHLWINDOWREF weak = destroyed;
    destroyed.reset();
    EXPECT_TRUE(weak.expired());
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(unmapped), before);
    EXPECT_FALSE(pending(unmapped));
    EXPECT_EQ(appliedGoal(kept), goal);
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, DestructionDiscardsGoalsAndCancelsExternallyHeldTimer) {
    auto       window = makeWindow();
    const auto before = appliedGoal(window);
    m_monitor->m_windowTransaction->queue(window, {10, 20, 200, 160});
    auto timeout = timer(m_monitor);
    timeout->updateTimeout(-1ms);
    m_monitor->m_windowTransaction.reset();
    EXPECT_TRUE(timeout->cancelled());
    EXPECT_FALSE(timeout->armed());
    EXPECT_FALSE(pending(window));
    EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_GOAL), before);
    g_pEventLoopManager->onTimerFire();
    EXPECT_EQ(appliedGoal(window), before);
}

TEST_F(CMonitorWindowTransactionTest, ReentrantAnimationCallbackQueuesANewBatchWithoutApplyingItEarly) {
    auto       first     = makeWindow();
    auto       second    = makeWindow();
    const auto workspace = makeWorkspace(true);
    first->m_workspace = second->m_workspace = workspace;
    m_monitor->m_enabled                     = true;
    first->presentation().stageFullscreen();
    second->presentation().stageFullscreen();
    const auto before = appliedGoal(second);
    const CBox firstGoal{30, 40, 100, 80}, latest{70, 80, 100, 80};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(first, firstGoal);
    transaction.queue(second, {50, 60, 100, 80});
    int callbacks = 0;
    first->positionAnimation()->setCallbackOnBegin([&](auto) {
        ++callbacks;
        transaction.queue(second, latest);
    });
    transaction.onPreRender();
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(appliedGoal(first), firstGoal);
    EXPECT_EQ(appliedGoal(second), before);
    EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_GOAL), latest);
    EXPECT_TRUE(ownedBy(second, m_monitor));
    EXPECT_TRUE(workspace->m_fullscreenPresentation);
    transaction.onPostRender();
    transaction.onPreRender();
    EXPECT_EQ(appliedGoal(second), latest);
    EXPECT_FALSE(pending(second));
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
    transaction.onPostRender();
}

TEST_F(CMonitorWindowTransactionTest, DestructionDuringApplyDiscardsTheRestOfTheDetachedBatch) {
    auto       first        = makeWindow();
    auto       second       = makeWindow();
    const auto secondBefore = appliedGoal(second);
    const CBox goal{30, 40, 100, 80};
    m_monitor->m_windowTransaction->queue(first, goal);
    m_monitor->m_windowTransaction->queue(second, goal);
    auto timeout = timer(m_monitor);
    first->positionAnimation()->setCallbackOnBegin([&](auto) { m_monitor->m_windowTransaction.reset(); });
    m_monitor->m_windowTransaction->onPreRender();
    EXPECT_FALSE(m_monitor->m_windowTransaction);
    EXPECT_TRUE(timeout->cancelled());
    EXPECT_FALSE(pending(first));
    EXPECT_FALSE(pending(second));
    EXPECT_EQ(appliedGoal(first), goal);
    EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_GOAL), secondBefore);
}

TEST_F(CMonitorWindowTransactionTest, DestructionDuringRemovalReleasesRemainingGeometryAndCloseHolds) {
    const auto workspace = makeWorkspace(true);
    auto       removed   = makeWindow();
    auto       survivor  = makeWindow();
    auto       closed    = makeWindow();
    removed->m_workspace = survivor->m_workspace = closed->m_workspace = workspace;
    m_monitor->m_enabled                                               = true;
    removed->presentation().stageFullscreen();
    removed->presentation().setFullscreenAlpha(0.5F);
    m_monitor->m_windowTransaction->queue(survivor, {10, 20, 200, 160});
    const auto fadeout = closeSnapshot(closed);
    const auto timeout = timer(m_monitor);
    ASSERT_TRUE(fadeout);
    ASSERT_FALSE(fadeout->done());
    int callbacks = 0;
    removed->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setCallbackOnBegin([&](auto) {
        ++callbacks;
        m_monitor->m_windowTransaction.reset();
    });

    m_monitor->m_windowTransaction->remove(removed);
    EXPECT_EQ(callbacks, 1);
    EXPECT_FALSE(m_monitor->m_windowTransaction);
    EXPECT_TRUE(timeout->cancelled());
    EXPECT_FALSE(pending(removed));
    EXPECT_FALSE(pending(survivor));
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
    EXPECT_TRUE(fadeout->done());
}

TEST_F(CMonitorWindowTransactionTest, RemovalPreservesReplacementBatchQueuedByDeferredAlpha) {
    const auto workspace = makeWorkspace(true);
    auto       removed   = makeWindow();
    auto       next      = makeWindow();
    removed->m_workspace = next->m_workspace = workspace;
    m_monitor->m_enabled                     = true;
    removed->presentation().stageFullscreen();
    removed->presentation().setFullscreenAlpha(0.5F);
    const CBox goal{10, 20, 200, 160};
    int        callbacks = 0;
    removed->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setCallbackOnBegin([&](auto) {
        ++callbacks;
        m_monitor->m_windowTransaction->queue(next, goal);
    });

    m_monitor->m_windowTransaction->remove(removed);
    EXPECT_EQ(callbacks, 1);
    EXPECT_FALSE(pending(removed));
    EXPECT_TRUE(ownedBy(next, m_monitor));
    EXPECT_TRUE(timer(m_monitor)->armed());
    EXPECT_TRUE(workspace->m_fullscreenPresentation);
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(next), goal);
    EXPECT_FALSE(pending(next));
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
}

TEST_F(CMonitorWindowTransactionTest, IncomingWorkspaceAndBothTileSizesPublishTogether) {
    for (const int order : {0, 1, 2}) {
        SCOPED_TRACE(order);
        const auto active   = makeWorkspace(true);
        const auto hidden   = makeWorkspace(false);
        auto       first    = makeWindow({0, 0, 400, 300});
        auto       second   = makeWindow({0, 0, 400, 300});
        first->m_workspace  = active;
        second->m_workspace = hidden;
        m_monitor->m_size   = {400, 300};
        second->presentation().alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setConfig(Config::animationTree()->getAnimationPropertyConfig("borderangle"));

        stageWorkspace(second, active);
        const CBox firstGoal{0, 0, 200, 300}, secondGoal{200, 0, 200, 300};
        auto&      transaction = *m_monitor->m_windowTransaction;
        transaction.queue(first, firstGoal);
        transaction.queue(second, secondGoal);
        first->finishAnimation();
        second->finishAnimation();
        commit(order == 1 ? second : first, {200, 300}, {200, 300});
        transaction.onPreRender();
        EXPECT_EQ(second->m_workspace, active);
        EXPECT_FALSE(g_pHyprRenderer->shouldRenderWindow(second, m_monitor));
        EXPECT_EQ(first->size(IGeometric::GEOMETRIC_CURRENT), Vector2D(400, 300));
        transaction.onPostRender();

        if (order == 2)
            expire(m_monitor);
        else
            commit(order == 1 ? first : second, {200, 300}, {200, 300});
        transaction.onPreRender();
        EXPECT_TRUE(g_pHyprRenderer->shouldRenderWindow(second, m_monitor));
        EXPECT_EQ(first->geometricBox(IGeometric::GEOMETRIC_CURRENT), firstGoal);
        EXPECT_EQ(second->geometricBox(IGeometric::GEOMETRIC_CURRENT), secondGoal);
        EXPECT_FLOAT_EQ(second->presentation().alphaValue(WINDOW_ALPHA_MOVE_FROM_WORKSPACE), 1.F);
        transaction.onPostRender();
    }
}

TEST_F(CMonitorWindowTransactionTest, SameBoxWorkspaceChangeWaitsForTheBatchWithoutAnotherBuffer) {
    const auto active     = makeWorkspace(true);
    const auto hidden     = makeWorkspace(false);
    auto       incoming   = makeWindow();
    auto       resized    = makeWindow();
    incoming->m_workspace = hidden;
    stageWorkspace(incoming, active);
    const auto before = appliedGoal(incoming);
    const CBox goal{0, 0, 200, 160};
    auto&      transaction = *m_monitor->m_windowTransaction;
    transaction.queue(resized, goal);
    transaction.onPreRender();
    EXPECT_EQ(incoming->presentation().workspace(), hidden);
    transaction.onPostRender();
    commit(resized, goal.size(), goal.size());
    EXPECT_EQ(incoming->presentation().workspace(), active);
    EXPECT_EQ(appliedGoal(incoming), before);
    EXPECT_FALSE(pending(incoming));
}

TEST_F(CMonitorWindowTransactionTest, WorkspaceSupersessionAndMonitorTransferRetainThePresentedSource) {
    auto       hidden       = makeWorkspace(false);
    const auto intermediate = makeWorkspace(true);
    const auto nextMonitor  = makeMonitor();
    const auto destination  = makeWorkspace(true, nextMonitor);
    auto       window       = makeWindow();
    window->m_workspace     = hidden;
    PHLWORKSPACEREF source  = hidden;
    stageWorkspace(window, intermediate);
    stageWorkspace(window, destination);
    const CBox goal{20, 30, 300, 240};
    nextMonitor->m_windowTransaction->queue(window, goal);
    hidden.reset();
    EXPECT_TRUE(source);
    EXPECT_EQ(window->presentation().workspace(), source.lock());
    EXPECT_EQ(window->presentation().monitor(), m_monitor);
    m_monitor->m_windowTransaction->clear();
    EXPECT_EQ(window->presentation().workspace(), source.lock());
    expire(nextMonitor);
    EXPECT_EQ(window->presentation().workspace(), destination);
    EXPECT_EQ(window->presentation().monitor(), nextMonitor);
    EXPECT_TRUE(source.expired());
    EXPECT_EQ(appliedGoal(window), goal);
}

TEST_F(CMonitorWindowTransactionTest, PresentedFullscreenModeFilterSurvivesPublication) {
    using namespace Fullscreen;
    const auto workspace = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
    auto       window    = makeWindow();
    window->m_workspace  = workspace;
    window->windowTarget()->setSpaceGhost(workspace->space());
    m_monitor->m_enabled = true;
    const auto handler   = workspace->space()->algorithm()->tiledAlgo()->getFSHandler();
    const auto check     = [&](eFullscreenMode mode) {
        EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, mode);
        EXPECT_EQ(controller()->isPresentedFullscreen(window), mode != FSMODE_NONE);
        EXPECT_EQ(controller()->getPresentedFullscreenWindow(workspace), mode == FSMODE_NONE ? nullptr : window);
        for (const auto filter : {FSMODE_MAXIMIZED, FSMODE_FULLSCREEN}) {
            EXPECT_EQ(controller()->getPresentedFullscreenWindow(workspace, filter), mode == filter ? window : nullptr);
            EXPECT_EQ(controller()->isPresentedFullscreen(window, filter), mode == filter);
        }
    };

    check(FSMODE_NONE);
    for (const auto mode : {FSMODE_FULLSCREEN, FSMODE_MAXIMIZED, FSMODE_NONE}) {
        SCOPED_TRACE(mode);
        const auto beforeMode = controller()->getFullscreenModes(window).internal;
        window->presentation().stageFullscreen();
        handler->setTargetFullscreenModeInternal(window->windowTarget(), mode);
        check(beforeMode);
        expire(m_monitor);
        check(mode);
    }
}

TEST_F(CMonitorWindowTransactionTest, FullscreenClassificationAndPeerOpacityWaitForGeometry) {
    using namespace Fullscreen;
    *CConfigValue<Config::FLOAT>("decoration:fullscreen_opacity").ptr() = 0.4F;
    for (const bool entering : {false, true}) {
        SCOPED_TRACE(entering);
        const auto workspace = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
        workspace->setVisible(true);
        m_monitor->m_enabled         = true;
        m_monitor->m_size            = {400, 300};
        m_monitor->m_activeWorkspace = workspace;
        // Register these windows so the real fullscreen fade visits the peer.
        auto window = makeWindow({10, 20, 100, 80}, true);
        auto peer   = makeWindow({10, 20, 100, 80}, true);
        for (const auto& w : {window, peer}) {
            w->m_workspace = workspace;
            w->windowTarget()->setSpaceGhost(workspace->space());
            w->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setConfig(Config::animationTree()->getAnimationPropertyConfig("borderangle"));
        }
        const auto handler    = workspace->space()->algorithm()->tiledAlgo()->getFSHandler();
        const auto beforeMode = entering ? FSMODE_NONE : FSMODE_FULLSCREEN;
        const auto afterMode  = entering ? FSMODE_FULLSCREEN : FSMODE_NONE;
        handler->setTargetFullscreenModeInternal(window->windowTarget(), beforeMode);
        handler->setNoMembersAboveFullscreen();
        Animation::Workspace::applyFullscreenFade(workspace);
        window->presentation().alpha(WINDOW_ALPHA_ACTIVE)->setConfig(Config::animationTree()->getAnimationPropertyConfig("borderangle"));
        window->presentation().refreshValues();
        window->presentation().alpha(WINDOW_ALPHA_ACTIVE)->warp();
        const auto before = appliedGoal(window);
        const CBox goal   = entering ? CBox{0, 0, 400, 300} : CBox{20, 30, 200, 160};

        window->presentation().stageFullscreen();
        handler->setTargetFullscreenModeInternal(window->windowTarget(), afterMode);
        handler->setNoMembersAboveFullscreen();
        Animation::Workspace::setFullscreenFadeAnimation(workspace, entering ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
        m_monitor->m_windowTransaction->queue(window, goal);
        window->finishAnimation();
        m_monitor->m_windowTransaction->onPreRender();
        EXPECT_EQ(controller()->getFullscreenModes(window).internal, afterMode);
        EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, beforeMode);
        EXPECT_EQ(controller()->isPresentedFullscreen(window), !entering);
        EXPECT_EQ(controller()->isAllowedOverFullscreen(peer), !entering);
        EXPECT_EQ(controller()->isAllowedOverPresentedFullscreen(peer), entering);
        EXPECT_EQ(peer->isBlockedByFullscreen(), entering);
        EXPECT_EQ(controller()->getPresentedFullscreenModes(workspace).internal, beforeMode);
        EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), before);
        EXPECT_FLOAT_EQ(peer->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN), entering ? 1.F : 0.F);
        EXPECT_FLOAT_EQ(window->presentation().alphaValue(WINDOW_ALPHA_ACTIVE), entering ? 1.F : 0.4F);
        EXPECT_EQ(window->presentation().renderBorderSize() > 0, entering);
        commit(window, goal.size(), goal.size());
        EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, beforeMode);
        m_monitor->m_windowTransaction->onPostRender();
        EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), goal);
        EXPECT_EQ(controller()->getPresentedFullscreenModes(window).internal, afterMode);
        EXPECT_EQ(controller()->isPresentedFullscreen(window), entering);
        EXPECT_EQ(controller()->isAllowedOverPresentedFullscreen(peer), !entering);
        EXPECT_EQ(controller()->getPresentedFullscreenModes(workspace).internal, afterMode);
        EXPECT_FLOAT_EQ(peer->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN), entering ? 0.F : 1.F);
        EXPECT_FLOAT_EQ(window->presentation().alphaValue(WINDOW_ALPHA_ACTIVE), entering ? 0.4F : 1.F);
        EXPECT_EQ(window->presentation().renderBorderSize() > 0, !entering);
        Desktop::windowState()->clear();
    }
}

TEST_F(CMonitorWindowTransactionTest, WorkspaceOffsetUsesFullscreenStatePublishedWithGeometry) {
    using namespace Fullscreen;
    for (const bool entering : {false, true}) {
        SCOPED_TRACE(entering);
        const auto workspace = Workspace::CRegularWorkspace::create(Workspace::SWorkspaceNumberedID{9}, m_monitor, "transaction");
        workspace->setVisible(true);
        workspace->m_renderOffset->setValueAndWarp({200, 0});
        m_monitor->m_enabled         = true;
        m_monitor->m_size            = {400, 300};
        m_monitor->m_activeWorkspace = workspace;
        auto window                  = makeWindow({-20, 20, 100, 80});
        window->m_workspace          = workspace;
        window->windowTarget()->setSpaceGhost(workspace->space());
        window->windowTarget()->setFloatingInitial(true);
        auto& presentation = window->presentation();
        presentation.onWorkspaceAnimUpdate();
        ASSERT_GT(presentation.floatingOffset().x, 0);
        const auto handler = workspace->space()->algorithm()->floatingAlgo()->getFSHandler();
        handler->setTargetFullscreenModeInternal(window->windowTarget(), entering ? FSMODE_NONE : FSMODE_FULLSCREEN);
        presentation.onWorkspaceAnimUpdate();
        const auto before = presentation.floatingOffset();

        presentation.stageFullscreen();
        handler->setTargetFullscreenModeInternal(window->windowTarget(), entering ? FSMODE_FULLSCREEN : FSMODE_NONE);
        auto&      transaction = *m_monitor->m_windowTransaction;
        const CBox goal{-20, 20, 200, 160};
        transaction.queue(window, goal);
        window->finishAnimation();
        transaction.onPreRender();
        presentation.onWorkspaceAnimUpdate();
        EXPECT_EQ(presentation.floatingOffset(), before);
        commit(window, goal.size(), goal.size());
        presentation.onWorkspaceAnimUpdate();
        EXPECT_EQ(presentation.floatingOffset(), before);
        transaction.onPostRender();

        presentation.onWorkspaceAnimUpdate();
        EXPECT_EQ(window->geometricBox(IGeometric::GEOMETRIC_CURRENT), goal);
        if (entering)
            EXPECT_EQ(presentation.floatingOffset(), Vector2D{});
        else
            EXPECT_GT(presentation.floatingOffset().x, 0);
    }
}

TEST_F(CMonitorWindowTransactionTest, WorkspaceFadeCompletesBeforeReturningFromAHiddenWorkspace) {
    m_monitor->m_id = 1;
    for (const bool animated : {false, true}) {
        SCOPED_TRACE(animated);
        const auto active   = makeWorkspace(true);
        const auto hidden   = makeWorkspace(false);
        auto       window   = makeWindow();
        window->m_workspace = active;
        auto& presentation  = window->presentation();
        for (const auto channel : {WINDOW_ALPHA_MOVE_TO_WORKSPACE, WINDOW_ALPHA_MOVE_FROM_WORKSPACE})
            presentation.alpha(channel)->setConfig(Config::animationTree()->getAnimationPropertyConfig(animated ? "fade" : "borderangle"));

        stageWorkspace(window, hidden);
        auto& transaction = *m_monitor->m_windowTransaction;
        transaction.onPreRender();
        transaction.onPostRender();
        if (animated) {
            EXPECT_TRUE(presentation.movingFromMonitor());
            EXPECT_FLOAT_EQ(presentation.alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->goal(), 0.F);
            presentation.alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->warp();
        }
        EXPECT_FALSE(presentation.movingFromMonitor());
        EXPECT_FLOAT_EQ(presentation.alphaValue(WINDOW_ALPHA_MOVE_TO_WORKSPACE), 1.F);

        stageWorkspace(window, active);
        EXPECT_EQ(presentation.workspace(), hidden);
        transaction.onPreRender();
        transaction.onPostRender();
        presentation.alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->warp();
        EXPECT_EQ(presentation.workspace(), active);
        EXPECT_FLOAT_EQ(presentation.alphaValue(WINDOW_ALPHA_MOVE_FROM_WORKSPACE), 1.F);
        EXPECT_FLOAT_EQ(presentation.alphaValue(WINDOW_ALPHA_MOVE_TO_WORKSPACE), 1.F);
    }
}

TEST_F(CMonitorWindowTransactionTest, TransferredFullscreenHoldsWaitForBothBatchesWithoutTakingUnrelatedWorkspaces) {
    for (const bool destinationFirst : {false, true}) {
        SCOPED_TRACE(destinationFirst);
        const auto source      = makeWorkspace(true);
        const auto unrelated   = makeWorkspace(false);
        const auto nextMonitor = makeMonitor();
        const auto destination = makeWorkspace(true, nextMonitor);
        auto       moved       = makeWindow({10, 20, 100, 80}, true);
        auto       peer        = makeWindow({10, 20, 100, 80}, true);
        auto       other       = makeWindow({10, 20, 100, 80}, true);
        moved->m_workspace = peer->m_workspace = source;
        other->m_workspace                     = unrelated;
        m_monitor->m_enabled = nextMonitor->m_enabled = true;
        moved->presentation().stageFullscreen();
        other->presentation().stageFullscreen();
        stageWorkspace(moved, destination);
        const CBox goal{10, 20, 200, 160};
        nextMonitor->m_windowTransaction->queue(moved, goal);
        m_monitor->m_windowTransaction->queue(peer, goal);
        m_monitor->m_windowTransaction->queue(other, goal);

        expire(destinationFirst ? nextMonitor : m_monitor);
        EXPECT_TRUE(source->m_fullscreenPresentation);
        EXPECT_EQ(!!unrelated->m_fullscreenPresentation, destinationFirst);
        expire(destinationFirst ? m_monitor : nextMonitor);
        EXPECT_FALSE(source->m_fullscreenPresentation);
        EXPECT_FALSE(unrelated->m_fullscreenPresentation);
        EXPECT_EQ(moved->presentation().workspace(), destination);
        EXPECT_EQ(appliedGoal(moved), goal);
        EXPECT_EQ(appliedGoal(peer), goal);
        Desktop::windowState()->clear();
    }
}

TEST_F(CMonitorWindowTransactionTest, FullscreenGestureFlushPublishesPeerPresentationToo) {
    const auto workspace = makeWorkspace(true);
    auto       window    = makeWindow({10, 20, 100, 80}, true);
    auto       peer      = makeWindow({10, 20, 100, 80}, true);
    window->m_workspace = peer->m_workspace = workspace;
    m_monitor->m_enabled                    = true;
    window->presentation().stageFullscreen();
    const CBox goal{0, 0, 200, 160};
    m_monitor->m_windowTransaction->queue(window, goal);
    window->flushPendingGeometry();
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
    EXPECT_FALSE(window->presentation().hasPendingState());
    EXPECT_FALSE(peer->presentation().hasPendingState());
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, CancellationReleasesHeldWorkspaceAndFullscreenState) {
    for (const bool clear : {false, true}) {
        SCOPED_TRACE(clear);
        auto       source      = makeWorkspace(false);
        const auto destination = makeWorkspace(true);
        auto       window      = makeWindow();
        window->m_workspace    = source;
        m_monitor->m_enabled   = true;
        window->presentation().stageFullscreen();
        stageWorkspace(window, destination);
        const PHLWORKSPACEREF previous = source;
        source.reset();
        if (clear)
            m_monitor->m_windowTransaction->clear();
        else {
            unmap(window);
            m_monitor->m_windowTransaction->remove(window);
        }
        EXPECT_TRUE(previous.expired());
        EXPECT_FALSE(window->presentation().hasPendingState());
        EXPECT_FALSE(pending(window));
        EXPECT_EQ(window->presentation().workspace(), destination);
        EXPECT_FALSE(timer(m_monitor)->armed());
    }
}

TEST_F(CMonitorWindowTransactionTest, NestedReleasePublishesTheLastSharedFullscreenHold) {
    const auto source      = makeWorkspace(true);
    const auto unrelated   = makeWorkspace(false);
    const auto nextMonitor = makeMonitor();
    const auto destination = makeWorkspace(true, nextMonitor);
    auto       moved       = makeWindow({10, 20, 100, 80}, true);
    auto       peer        = makeWindow({10, 20, 100, 80}, true);
    auto       other       = makeWindow({10, 20, 100, 80}, true);
    moved->m_workspace = peer->m_workspace = source;
    other->m_workspace                     = unrelated;
    m_monitor->m_enabled = nextMonitor->m_enabled = true;
    for (const auto& window : {peer, other})
        window->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setValueAndWarp(0.F);
    moved->presentation().stageFullscreen();
    other->presentation().stageFullscreen();
    stageWorkspace(moved, destination);
    const CBox goal{10, 20, 200, 160};
    nextMonitor->m_windowTransaction->queue(moved, goal);
    int callbacks = 0;
    other->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setCallbackOnBegin([&](auto) {
        ++callbacks;
        nextMonitor->m_windowTransaction->clear();
    });
    expire(m_monitor);
    EXPECT_EQ(callbacks, 1);
    EXPECT_FALSE(source->m_fullscreenPresentation);
    EXPECT_FALSE(unrelated->m_fullscreenPresentation);
    EXPECT_FLOAT_EQ(peer->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->goal(), 1.F);
}

TEST_F(CMonitorWindowTransactionTest, ClearingPreservesADeadlineQueuedByDeferredAlpha) {
    const auto workspace = makeWorkspace(true);
    auto       window    = makeWindow({10, 20, 100, 80}, true);
    window->m_workspace  = workspace;
    m_monitor->m_enabled = true;
    window->presentation().stageFullscreen();
    window->presentation().setFullscreenAlpha(0.5F);
    const CBox goal{10, 20, 200, 160};
    int        callbacks = 0;
    window->presentation().alpha(WINDOW_ALPHA_FULLSCREEN)->setCallbackOnBegin([&](auto) {
        if (++callbacks == 1)
            m_monitor->m_windowTransaction->queue(window, goal);
    });
    m_monitor->m_windowTransaction->clear();
    EXPECT_EQ(callbacks, 1);
    EXPECT_TRUE(pending(window));
    EXPECT_TRUE(timer(m_monitor)->armed());
    EXPECT_TRUE(workspace->m_fullscreenPresentation);
    expire(m_monitor);
    EXPECT_EQ(appliedGoal(window), goal);
    EXPECT_FALSE(pending(window));
    EXPECT_FALSE(workspace->m_fullscreenPresentation);
}

TEST_F(CMonitorWindowTransactionTest, DisabledCloseWaitsForReplacementGeometryWithoutAnExtraAnimationTick) {
    for (const int disabledBy : {0, 1, 2}) {
        for (const bool timeout : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "disabledBy=" << disabledBy << ", timeout=" << timeout);
            *CConfigValue<Config::BOOL>("animations:enabled").ptr() = disabledBy != 0;
            Config::animationTree()->setConfigForNode("windows", disabledBy != 2, 8.F, "default");
            Config::animationTree()->setConfigForNode("fadeOut", disabledBy != 2, 8.F, "default");
            const auto workspace = makeWorkspace(true);
            auto       closed    = makeWindow({200, 0, 200, 300});
            auto       survivor  = makeWindow({0, 0, 200, 300});
            closed->m_workspace = survivor->m_workspace = workspace;
            if (disabledBy == 1) {
                closed->m_ruleApplicator->noAnim().set(true, Desktop::Types::PRIORITY_SET_PROP);
                survivor->m_ruleApplicator->noAnim().set(true, Desktop::Types::PRIORITY_SET_PROP);
            }
            const auto oldBox = appliedGoal(survivor);
            const CBox goal{0, 0, 400, 300};
            auto&      transaction = *m_monitor->m_windowTransaction;
            transaction.queue(survivor, goal);
            if (timeout)
                timer(m_monitor)->updateTimeout(-1ms);
            auto fadeout = closeSnapshot(closed);
            ASSERT_TRUE(fadeout);
            const auto                     snapshotBox = fadeout->geometricBox(IGeometric::GEOMETRIC_CURRENT);
            const WP<Render::IFramebuffer> snapshot    = fadeout->framebuffer();
            const PHLWINDOWREF             closedRef   = closed;
            closed.reset();
            EXPECT_TRUE(closedRef.expired());

            Animation::mgr()->tick();
            Desktop::fadingOutState()->cleanupForMonitor(m_monitor);
            transaction.onPreRender();
            EXPECT_EQ(survivor->geometricBox(IGeometric::GEOMETRIC_CURRENT), oldBox);
            EXPECT_EQ(fadeout->geometricBox(IGeometric::GEOMETRIC_CURRENT), snapshotBox);
            EXPECT_FLOAT_EQ(fadeout->alpha(), 1.F);
            EXPECT_FALSE(fadeout->done());
            EXPECT_TRUE(transaction.hasPendingPresentation());
            EXPECT_EQ(Desktop::fadingOutState()->fadeouts().size(), 1U);
            if (timeout) {
                EXPECT_TRUE(timer(m_monitor)->passed());
                g_pEventLoopManager->onTimerFire();
            } else
                commit(survivor, goal.size(), goal.size());
            EXPECT_FLOAT_EQ(fadeout->alpha(), 1.F);
            transaction.onPostRender();
            EXPECT_EQ(survivor->geometricBox(IGeometric::GEOMETRIC_CURRENT), goal);
            EXPECT_TRUE(fadeout->done());
            EXPECT_FLOAT_EQ(fadeout->alpha(), 0.F);
            EXPECT_FALSE(transaction.hasPendingPresentation());
            EXPECT_FALSE(timer(m_monitor)->armed());
            Desktop::fadingOutState()->cleanupForMonitor(m_monitor);
            fadeout.reset();
            EXPECT_TRUE(snapshot.expired());
        }
    }
}

TEST_F(CMonitorWindowTransactionTest, CloseWithoutAReplacementResizeDoesNotWaitOnOtherWorkspacesOrMoves) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    const auto workspace                                    = makeWorkspace(true);
    const auto unrelated                                    = makeWorkspace(false);
    auto       closed                                       = makeWindow();
    auto       moved                                        = makeWindow();
    auto       resized                                      = makeWindow();
    closed->m_workspace = moved->m_workspace = workspace;
    resized->m_workspace                     = unrelated;
    auto& transaction                        = *m_monitor->m_windowTransaction;
    transaction.queue(moved, {30, 40, 100, 80});
    transaction.queue(resized, {0, 0, 200, 160});
    const auto fadeout = closeSnapshot(closed);
    ASSERT_TRUE(fadeout);
    EXPECT_TRUE(fadeout->done());
    EXPECT_TRUE(Desktop::fadingOutState()->fadeouts().empty());
    EXPECT_FALSE(transaction.hasPendingPresentation());
    EXPECT_TRUE(pending(resized));
}

TEST_F(CMonitorWindowTransactionTest, ConsecutiveClosesKeepTheirSnapshotsAndTheOriginalDeadline) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    const auto workspace                                    = makeWorkspace(true);
    auto       first                                        = makeWindow();
    auto       second                                       = makeWindow();
    auto       survivor                                     = makeWindow();
    first->m_workspace = second->m_workspace = survivor->m_workspace = workspace;
    auto& transaction                                                = *m_monitor->m_windowTransaction;
    transaction.queue(second, {0, 0, 200, 160});
    const auto firstFadeout = closeSnapshot(first);
    timer(m_monitor)->updateTimeout(-1ms);
    transaction.beginLayoutUpdate();
    transaction.remove(second);
    EXPECT_FLOAT_EQ(firstFadeout->alpha(), 1.F);
    EXPECT_TRUE(timer(m_monitor)->passed());
    const CBox goal{0, 0, 400, 160};
    transaction.queue(survivor, goal);
    const auto secondFadeout = closeSnapshot(second);
    transaction.endLayoutUpdate();
    EXPECT_TRUE(timer(m_monitor)->passed());
    EXPECT_FLOAT_EQ(firstFadeout->alpha(), 1.F);
    EXPECT_FLOAT_EQ(secondFadeout->alpha(), 1.F);
    g_pEventLoopManager->onTimerFire();
    EXPECT_EQ(appliedGoal(survivor), goal);
    EXPECT_TRUE(firstFadeout->done());
    EXPECT_TRUE(secondFadeout->done());
}

TEST_F(CMonitorWindowTransactionTest, RemovingTheLastMatchingResizeReleasesCloseEvenWithOtherPendingWork) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    const auto workspace                                    = makeWorkspace(true);
    auto       closed                                       = makeWindow();
    auto       survivor                                     = makeWindow();
    auto       unrelated                                    = makeWindow();
    closed->m_workspace = survivor->m_workspace = workspace;
    unrelated->m_workspace                      = makeWorkspace(false);
    auto&      transaction                      = *m_monitor->m_windowTransaction;
    const CBox goal{0, 0, 200, 160};
    transaction.queue(survivor, goal);
    transaction.queue(unrelated, goal);
    const auto fadeout = closeSnapshot(closed);
    transaction.remove(survivor);
    EXPECT_TRUE(fadeout->done());
    EXPECT_TRUE(pending(unrelated));
    EXPECT_TRUE(timer(m_monitor)->armed());
}

TEST_F(CMonitorWindowTransactionTest, CancelledOrDestroyedTransactionsDiscardTheirCloseSnapshots) {
    for (const int cancellation : {0, 1, 2}) {
        SCOPED_TRACE(cancellation);
        const auto monitor   = makeMonitor();
        const auto workspace = makeWorkspace(true, monitor);
        auto       closed    = makeWindow();
        auto       survivor  = makeWindow();
        closed->m_monitor = survivor->m_monitor = monitor;
        closed->m_workspace = survivor->m_workspace = workspace;
        const auto before                           = appliedGoal(survivor);
        const CBox goal{0, 0, 200, 160};
        monitor->m_windowTransaction->queue(survivor, goal);
        const auto fadeout = closeSnapshot(closed);
        ASSERT_FALSE(fadeout->done());
        if (cancellation == 2) {
            survivor->positionAnimation()->setCallbackOnBegin([&](auto) { monitor->m_windowTransaction.reset(); });
            expire(monitor);
        } else if (cancellation == 1)
            monitor->m_windowTransaction.reset();
        else
            monitor->m_windowTransaction->clear();
        EXPECT_TRUE(fadeout->done());
        EXPECT_FLOAT_EQ(fadeout->alpha(), 0.F);
        EXPECT_EQ(appliedGoal(survivor), cancellation == 2 ? goal : before);
        Desktop::fadingOutState()->cleanupForMonitor(monitor);
        EXPECT_TRUE(Desktop::fadingOutState()->fadeouts().empty());
    }
}

TEST_F(CMonitorWindowTransactionTest, RequeuedReplacementRetainsTheCloseSnapshotUntilItsNextBatch) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    const auto workspace                                    = makeWorkspace(true);
    auto       closed                                       = makeWindow();
    auto       first                                        = makeWindow();
    auto       second                                       = makeWindow();
    closed->m_workspace = first->m_workspace = second->m_workspace = workspace;
    auto&      transaction                                         = *m_monitor->m_windowTransaction;
    const CBox goal{30, 40, 200, 160}, latest{40, 50, 300, 240};
    transaction.queue(first, goal);
    transaction.queue(second, goal);
    const auto fadeout = closeSnapshot(closed);
    first->positionAnimation()->setCallbackOnBegin([&](auto) { transaction.queue(second, latest); });
    expire(m_monitor);
    EXPECT_TRUE(pending(second));
    EXPECT_FLOAT_EQ(fadeout->alpha(), 1.F);
    EXPECT_FALSE(fadeout->done());
    commit(second, latest.size(), latest.size());
    EXPECT_EQ(appliedGoal(second), latest);
    EXPECT_TRUE(fadeout->done());
}

TEST_F(CMonitorWindowTransactionTest, AnimatedCloseHoldsCapturedGeometryBeforeStartingItsExitStyle) {
    const auto workspace = makeWorkspace(true);
    auto       closed    = makeWindow();
    auto       survivor  = makeWindow();
    closed->m_workspace = survivor->m_workspace = workspace;
    closed->m_ruleApplicator->animationStyle().set("slide right", Desktop::Types::PRIORITY_SET_PROP);
    *closed->sizeAnimation() = Vector2D{200, 160};
    const auto capturedBox   = closed->geometricBox(IGeometric::GEOMETRIC_CURRENT);
    const CBox goal{0, 0, 400, 300};
    m_monitor->m_windowTransaction->queue(survivor, goal);
    const auto fadeout = closeSnapshot(closed);
    closed.reset();
    Animation::mgr()->tick();
    EXPECT_EQ(fadeout->geometricBox(IGeometric::GEOMETRIC_CURRENT), capturedBox);
    EXPECT_EQ(fadeout->geometricBox(IGeometric::GEOMETRIC_GOAL), capturedBox);
    EXPECT_FLOAT_EQ(fadeout->alpha(), 1.F);
    commit(survivor, goal.size(), goal.size());
    EXPECT_EQ(fadeout->geometricBox(IGeometric::GEOMETRIC_CURRENT), capturedBox);
    EXPECT_NE(fadeout->geometricBox(IGeometric::GEOMETRIC_GOAL), capturedBox);
    EXPECT_FALSE(fadeout->done());
}

TEST_F(CMonitorWindowTransactionTest, CloseSnapshotAllocationRespectsTransactionEligibility) {
    auto& renderer                        = sc<CTransactionTestRenderer&>(*g_pHyprRenderer);
    renderer.m_observeSnapshotAllocations = true;
    m_monitor->m_size = m_monitor->m_pixelSize = m_monitor->m_transformedSize = {400, 300};
    const auto workspace                                                      = makeWorkspace(true);
    m_monitor->m_activeWorkspace                                              = workspace;

    struct SCase {
        int    timeout     = 0;
        bool   noAnim      = true;
        bool   pending     = false;
        bool   blocked     = false;
        size_t allocations = 0;
    };
    for (const auto& scenario : {
             SCase{},
             SCase{.timeout = 50, .allocations = 1},
             SCase{.noAnim = false, .allocations = 1},
             SCase{.pending = true, .allocations = 1},
             SCase{.timeout = 50, .blocked = true, .allocations = 1},
         }) {
        SCOPED_TRACE(::testing::Message() << "timeout=" << scenario.timeout << ", noAnim=" << scenario.noAnim << ", pending=" << scenario.pending
                                          << ", blocked=" << scenario.blocked);
        auto closed         = makeWindow();
        auto peer           = makeWindow();
        closed->m_workspace = peer->m_workspace = workspace;
        closed->m_ruleApplicator->noAnim().set(scenario.noAnim, Desktop::Types::PRIORITY_SET_PROP);
        commit(closed, {100, 80}, {100, 80});
        prepareNullBufferUnmap(closed);
        auto& transaction = *m_monitor->m_windowTransaction;
        transaction.beginLayoutUpdate();
        const CBox goal{10, 20, 200, 160};
        if (scenario.pending) {
            *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 50;
            transaction.queue(peer, goal);
            ASSERT_TRUE(pending(peer));
            ASSERT_FALSE(transaction.hasPendingPresentation());
        }

        *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = scenario.timeout;
        renderer.m_snapshotAllocationAttempts                            = 0;
        if (scenario.blocked)
            captureWithGeometryBlocked(closed);
        else {
            SSurfaceState detach;
            detach.updated.bits.buffer = true;
            closed->backend().surface()->commitState(detach);
        }
        EXPECT_TRUE(capturedCloseSnapshot(closed));
        EXPECT_EQ(renderer.m_snapshotAllocationAttempts, scenario.allocations);
        EXPECT_EQ(pending(peer), scenario.pending);
        transaction.endLayoutUpdate();
        if (scenario.pending) {
            EXPECT_FALSE(pending(peer));
            EXPECT_EQ(appliedGoal(peer), goal);
        }
    }
}

TEST_F(CMonitorWindowTransactionTest, CloseSnapshotStillCapturesDuringDetachedBatchPublication) {
    auto& renderer                        = sc<CTransactionTestRenderer&>(*g_pHyprRenderer);
    renderer.m_observeSnapshotAllocations = true;
    m_monitor->m_size = m_monitor->m_pixelSize = m_monitor->m_transformedSize = {400, 300};
    const auto workspace                                                      = makeWorkspace(true);
    m_monitor->m_activeWorkspace                                              = workspace;
    auto closed                                                               = makeWindow();
    auto peer                                                                 = makeWindow();
    closed->m_workspace = peer->m_workspace = workspace;
    closed->m_ruleApplicator->noAnim().set(true, Desktop::Types::PRIORITY_SET_PROP);
    m_monitor->m_windowTransaction->queue(peer, {30, 40, 200, 160});
    *CConfigValue<Config::INTEGER>("misc:transaction_timeout").ptr() = 0;
    peer->positionAnimation()->setCallbackOnBegin([&](auto) { captureWithGeometryBlocked(closed); });
    m_monitor->m_windowTransaction->onPreRender();
    EXPECT_TRUE(capturedCloseSnapshot(closed));
    EXPECT_EQ(renderer.m_snapshotAllocationAttempts, 1U);
    EXPECT_FALSE(pending(peer));
    m_monitor->m_windowTransaction->onPostRender();
}

TEST_F(CMonitorWindowTransactionTest, NullBufferUnmapCapturesBeforeLosingTheLastSurfaceImage) {
    auto window         = makeWindow();
    window->m_workspace = makeWorkspace(true);
    window->m_ruleApplicator->noAnim().set(true, Desktop::Types::PRIORITY_SET_PROP);
    commit(window, {100, 80}, {100, 80});
    prepareNullBufferUnmap(window);
    const auto surface = window->backend().surface();
    const auto texture = surface->m_current.texture;
    ASSERT_TRUE(texture);
    EXPECT_FALSE(capturedCloseSnapshot(window));
    int           captures = 0, unmaps = 0;
    auto          before = surface->m_events.preUnmap.listen([&] {
        ++captures;
        EXPECT_EQ(surface->m_current.texture, texture);
        EXPECT_TRUE(capturedCloseSnapshot(window));
    });
    auto          after  = window->backend().m_events.unmap.listen([&] {
        ++unmaps;
        EXPECT_EQ(captures, 1);
        EXPECT_FALSE(surface->m_current.texture);
        EXPECT_TRUE(capturedCloseSnapshot(window));
    });
    SSurfaceState detach;
    detach.updated.bits.buffer = true;
    surface->commitState(detach);
    EXPECT_EQ(captures, 1);
    EXPECT_EQ(unmaps, 1);
    surface->commitState(detach);
    EXPECT_EQ(captures, 1);
    EXPECT_EQ(unmaps, 1);
}

TEST_F(CMonitorWindowTransactionTest, ClosingTheLastReplacementRetiresSnapshotsWhenLayoutUpdateFinishes) {
    *CConfigValue<Config::BOOL>("animations:enabled").ptr() = false;
    const auto workspace                                    = makeWorkspace(true);
    auto       closed                                       = makeWindow();
    auto       survivor                                     = makeWindow();
    closed->m_workspace = survivor->m_workspace = workspace;
    auto& transaction                           = *m_monitor->m_windowTransaction;
    transaction.queue(survivor, {0, 0, 200, 160});
    const auto fadeout = closeSnapshot(closed);
    transaction.beginLayoutUpdate();
    transaction.beginLayoutUpdate();
    transaction.remove(survivor);
    expire(m_monitor);
    transaction.endLayoutUpdate();
    EXPECT_FALSE(fadeout->done());
    transaction.endLayoutUpdate();
    EXPECT_TRUE(fadeout->done());
    EXPECT_FALSE(timer(m_monitor)->armed());
    EXPECT_FALSE(transaction.hasPendingPresentation());
}
