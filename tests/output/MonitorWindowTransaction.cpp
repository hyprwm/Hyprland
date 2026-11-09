#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <desktop/view/window/Window.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/Monitor.hpp>
#include <output/MonitorWindowTransaction.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/XDGShell.hpp>
#include <protocols/core/Compositor.hpp>
#include <render/ElementRenderer.hpp>
#include <render/Renderer.hpp>
#include <render/SyncFDManager.hpp>
#include <render/decorations/DecorationPositioner.hpp>
#include <state/MonitorState.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sys/socket.h>
#include <utility>

using namespace Desktop::View;
using namespace std::chrono_literals;

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

class CMonitorWindowTransactionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_previousCompositor         = std::move(g_pCompositor);
        m_previousEventLoop          = std::move(g_pEventLoopManager);
        m_previousRenderer           = std::move(g_pHyprRenderer);
        m_previousMonitors           = std::move(State::monitorState());
        m_previousBus                = std::move(Event::bus());
        m_previousWatcher            = std::move(Config::watcher());
        m_previousConfig             = std::move(Config::mgr());
        m_previousAnimations         = std::move(Animation::mgr());
        m_previousTree               = std::move(Config::animationTree());
        m_previousPositioner         = std::move(g_pDecorationPositioner);
        m_previousCompositorProtocol = std::move(PROTO::compositor);
        m_previousPresentation       = std::move(PROTO::presentation);
        m_previousXDG                = std::move(PROTO::xdgShell);

        Event::bus()          = makeUnique<Event::CEventBus>();
        State::monitorState() = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor         = makeUnique<CCompositor>(true);
        Config::watcher()     = makeUnique<Config::CConfigWatcher>();
        Config::mgr()         = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        // Create animations before the event loop so timer expiry cannot tick them.
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        g_pDecorationPositioner = makeUnique<CDecorationPositioner>();

        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        g_pCompositor->m_wlDisplay   = m_display.get();
        g_pCompositor->m_wlEventLoop = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_drm.fd      = -1;
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
        m_monitor.reset();
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        m_clients.clear();
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
        g_pEventLoopManager = std::move(m_previousEventLoop);
        g_pCompositor       = std::move(m_previousCompositor);
        Event::bus()        = std::move(m_previousBus);
    }

    PHLMONITOR makeMonitor() {
        auto monitor                 = makeShared<Monitor::CMonitor>(makeShared<CTransactionTestOutput>());
        monitor->m_self              = monitor;
        monitor->m_windowTransaction = makeUnique<Monitor::CMonitorWindowTransaction>(monitor);
        return monitor;
    }

    PHLWINDOW makeWindow(const CBox& box = {10, 20, 100, 80}) {
        auto surface            = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        surface->m_self         = surface;
        auto xdg                = makeShared<CXDGSurfaceResource>(makeShared<CXdgSurface>(m_client, 6, m_nextID++), nullptr, surface);
        xdg->m_self             = xdg;
        xdg->m_current.geometry = {{}, box.size()};
        auto window             = CWindow::create(makeUnique<CWaylandBackend>(xdg));
        window->m_monitor       = m_monitor;
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

    void unmap(PHLWINDOW window) {
        window->m_isMapped = false;
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
};

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

TEST_F(CMonitorWindowTransactionTest, AMatchingNewBufferReleasesAllWindowsInTheBatch) {
    auto       first  = makeWindow();
    auto       second = makeWindow();
    const CBox firstGoal{10, 20, 200, 160}, secondGoal{210, 20, 300, 240};
    m_monitor->m_windowTransaction->queue(first, firstGoal);
    m_monitor->m_windowTransaction->queue(second, secondGoal);
    commit(first, firstGoal.size(), firstGoal.size());
    EXPECT_EQ(appliedGoal(first), firstGoal);
    EXPECT_EQ(appliedGoal(second), secondGoal);
    EXPECT_FALSE(pending(second));
    EXPECT_FALSE(timer(m_monitor)->armed());
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
    auto       first  = makeWindow();
    auto       second = makeWindow();
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
    transaction.onPostRender();
    transaction.onPreRender();
    EXPECT_EQ(appliedGoal(second), latest);
    EXPECT_FALSE(pending(second));
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
