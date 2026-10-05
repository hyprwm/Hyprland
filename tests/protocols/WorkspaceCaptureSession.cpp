#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <desktop/state/WindowState.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <desktop/view/window/Window.hpp>
#include <event/EventBus.hpp>
#include <ipc/s2/Impl.hpp>
#include <ipc/s2/S2.hpp>
#include <layout/target/WindowTarget.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <managers/permissions/DynamicPermissionManager.hpp>
#include <managers/screenshare/ScreenshareManager.hpp>
#include <managers/screenshare/WorkspaceCaptureSource.hpp>
#include <output/Monitor.hpp>
#include <output/MonitorResources.hpp>
#include <render/ElementRenderer.hpp>
#include <render/Renderbuffer.hpp>
#include <render/SyncFDManager.hpp>
#include <state/MonitorState.hpp>
#include <workspace/HLWorkspace.hpp>
#include <hyprland-workspace-image-capture-source-v1.hpp>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <sys/socket.h>
#include <thread>
#include <utility>

// Like the source fixture, omit CHLWorkspace::init and drive public lifecycle
// signals directly, without layouts, workspace rules, or a compositor session.
class CSessionTestWorkspace : public Workspace::CHLWorkspace {
  public:
    explicit CSessionTestWorkspace(PHLMONITOR monitor) : CHLWorkspace(Workspace::SWorkspaceNumberedID{73}, monitor, "Capture workspace", "73", Workspace::eWorkspaceType::NORMAL) {
        ;
    }
};

class CSessionTestOutput : public Aquamarine::IOutput {
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

class CSessionTestBuffer : public IHLBuffer {
  public:
    uint32_t                      m_format = DRM_FORMAT_ARGB8888;
    std::vector<CBox>             m_readbacks;

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
    Aquamarine::SSHMAttrs shm() override {
        return {.success = true, .fd = -1, .format = m_format, .size = size, .stride = sc<int>(size.x) * 4};
    }
};

// Record readback requests rather than allocating GPU storage or mapping SHM.
class CSessionTestFramebuffer : public Render::IFramebuffer {
  public:
    void release() override {
        m_fbAllocated = false;
    }
    bool readPixels(CHLBufferReference buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {
        EXPECT_EQ(buffer->size, m_size);
        EXPECT_EQ(buffer->shm().format, m_drmFormat);
        sc<CSessionTestBuffer&>(*buffer.m_buffer).m_readbacks.emplace_back(x, y, width, height);
        return true;
    }
    void bind() override {
        ;
    }
    void addStencil(SP<Render::ITexture>) override {
        ;
    }

  private:
    bool internalAlloc(int, int, DRMFormat) override {
        return true;
    }
};

class CSessionTestElements : public Render::IElementRenderer {
  public:
    struct SClear {
        CHyprColor color;
        CRegion    damage;
    };
    std::vector<SClear> m_clears;

  protected:
    void draw(Render::CRenderContext&, WP<CClearPassElement> element, const CRegion& damage) override {
        m_clears.push_back({element->m_data.color, damage});
    }
    void draw(Render::CRenderContext&, WP<CBorderPassElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected border draw";
    }
    void draw(Render::CRenderContext&, WP<CFramebufferElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected framebuffer draw";
    }
    void draw(Render::CRenderContext&, WP<CPreBlurElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected blur draw";
    }
    void draw(Render::CRenderContext&, WP<CRectPassElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected rectangle draw";
    }
    void draw(Render::CRenderContext&, WP<CShadowPassElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected shadow draw";
    }
    void draw(Render::CRenderContext&, WP<CInnerGlowPassElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected glow draw";
    }
    void draw(Render::CRenderContext&, WP<CTexPassElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected texture draw";
    }
    void draw(Render::CRenderContext&, WP<CTextureMatteElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected texture matte draw";
    }
};

// Exercise the real render lifecycle and element dispatch without a GL backend.
class CSessionTestRenderer : public Render::IHyprRenderer {
  public:
    using IHyprRenderer::createTexture;

    UP<CSessionTestElements> m_elements = makeUnique<CSessionTestElements>();
    int                      m_begins = 0, m_ends = 0;

    eType                    type() override {
        return RT_GL;
    }
    Render::SRenderResult endRender(const std::function<void()>& callback) override {
        EXPECT_TRUE(context().active());
        EXPECT_TRUE(context().m_data.blockScreenShader);
        ++m_ends;
        finishRender();
        if (callback)
            callback();
        return {};
    }
    UP<Render::ISyncFDManager> createSyncFDManager() override {
        return nullptr;
    }
    WP<Render::IElementRenderer> elementRenderer() override {
        return m_elements;
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
        return makeShared<CSessionTestFramebuffer>();
    }
    void disableScissor() override {
        ;
    }
    void blend(bool) override {
        ;
    }
    void drawShadow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, float, const Render::SWindowRenderPresentation&) override {
        ;
    }
    void drawShadow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float,
                    const Render::SWindowRenderPresentation&) override {
        ;
    }
    void drawGlow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, float) override {
        ;
    }
    void drawGlow(Render::CRenderContext&, const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float) override {
        ;
    }
    void setViewport(int, int, int, int) override {
        ;
    }
    SP<Render::IFramebuffer> blurFramebuffer(Render::CRenderContext&, SP<Render::IFramebuffer>, float, const CRegion&, const Render::SBlurContext&) override {
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
    void renderOffToMain(Render::CRenderContext&, SP<Render::IFramebuffer>) override {
        ;
    }
    SP<Render::IRenderbuffer> getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer>, uint32_t) override {
        return nullptr;
    }

  private:
    bool beginFullFakeRenderInternal(Render::CRenderContext& ctx, PHLMONITOR, CRegion& damage, SP<Render::IFramebuffer> fb, bool simple) override {
        EXPECT_TRUE(ctx.active());
        EXPECT_TRUE(simple);
        EXPECT_EQ(ctx.m_mode, Render::RENDER_MODE_FULL_FAKE);
        EXPECT_EQ(ctx.m_data.fbSize, fb->m_size);
        ++m_begins;
        ctx.m_data.currentFB = ctx.m_data.outFB = fb;
        setDamage(ctx, damage, std::nullopt);
        return true;
    }
};

class CWorkspaceCaptureSessionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_NE(m_display, nullptr);

        m_previousCompositor  = std::move(g_pCompositor);
        m_previousEventLoop   = std::move(g_pEventLoopManager);
        m_previousRenderer    = std::move(g_pHyprRenderer);
        m_previousScreenshare = std::move(Screenshare::mgr());
        m_previousWindows     = std::move(Desktop::windowState());
        m_previousEventBus    = std::move(Event::bus());
        m_previousWatcher     = std::move(Config::watcher());
        m_previousConfig      = std::move(Config::mgr());
        m_previousAnimations  = std::move(Animation::mgr());
        m_previousTree        = std::move(Config::animationTree());
        m_installedGlobals    = true;

        Event::bus()                      = makeUnique<Event::CEventBus>();
        Desktop::windowState()            = makeUnique<Desktop::CWindowState>();
        g_pCompositor                     = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay        = m_display.get();
        g_pCompositor->m_wlEventLoop      = wl_display_get_event_loop(m_display.get());
        g_pCompositor->m_drm.fd           = -1;
        g_pCompositor->m_drmRenderNode.fd = -1;
        // Socket2 is lazy and workspace destruction/successful copies use it.
        // An overlong UNIX path prevents binding any socket on the filesystem.
        g_pCompositor->m_instancePath = std::string(128, 'x');
        m_previousSocket2             = std::move(IPC::Socket2::sock());
        IPC::Socket2::sock()          = makeUnique<IPC::Socket2::CSocket2>();
        Aquamarine::SBackendImplementationOptions backend;
        backend.backendType        = Aquamarine::AQ_BACKEND_NULL;
        g_pCompositor->m_aqBackend = Aquamarine::CBackend::create({backend}, Aquamarine::SBackendOptions{});
        ASSERT_TRUE(g_pCompositor->m_aqBackend);
        g_pCompositor->m_sessionActive = false;
        g_pEventLoopManager            = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
        Config::watcher()              = makeUnique<Config::CConfigWatcher>();
        Config::mgr()                  = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        g_pHyprRenderer         = makeUnique<CSessionTestRenderer>();
        wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);
        Screenshare::mgr() = makeUnique<Screenshare::CScreenshareManager>();

        m_monitor              = makeMonitor({1080, 1920});
        m_monitor->m_size      = {540, 960};
        m_monitor->m_pixelSize = {1920, 1080};
        m_monitor->m_scale     = 2.F;
        m_monitor->m_transform = WL_OUTPUT_TRANSFORM_90;
        m_workspace            = makeShared<CSessionTestWorkspace>(m_monitor);
        m_workspace->m_self    = m_workspace;
    }

    void TearDown() override {
        if (!m_installedGlobals)
            return;

        Screenshare::mgr() = std::move(m_previousScreenshare);
        m_workspace.reset();
        m_monitor.reset();
        IPC::Socket2::sock()    = std::move(m_previousSocket2);
        g_pHyprRenderer         = std::move(m_previousRenderer);
        Animation::mgr()        = std::move(m_previousAnimations);
        Config::animationTree() = std::move(m_previousTree);
        Config::mgr()           = std::move(m_previousConfig);
        Config::watcher()       = std::move(m_previousWatcher);
        if (Config::mgr())
            CConfigValueBase::flushCaches();
        g_pEventLoopManager    = std::move(m_previousEventLoop);
        g_pCompositor          = std::move(m_previousCompositor);
        Desktop::windowState() = std::move(m_previousWindows);
        Event::bus()           = std::move(m_previousEventBus);
    }

    PHLMONITOR makeMonitor(const Vector2D& size) {
        auto output     = makeShared<CSessionTestOutput>();
        output->name    = "workspace-session-test";
        auto monitor    = makeShared<Monitor::CMonitor>(output);
        monitor->m_self = monitor;
        monitor->m_size = monitor->m_pixelSize = monitor->m_transformedSize = size;
        return monitor;
    }

    SP<Screenshare::CWorkspaceCaptureSource> source(uint32_t mode = 0) {
        return makeShared<Screenshare::CWorkspaceCaptureSource>(m_workspace, mode);
    }

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    PHLMONITOR                                                 m_monitor;
    SP<CSessionTestWorkspace>                                  m_workspace;

  private:
    bool                                 m_installedGlobals = false;
    UP<CCompositor>                      m_previousCompositor;
    UP<CEventLoopManager>                m_previousEventLoop;
    UP<Render::IHyprRenderer>            m_previousRenderer;
    UP<Screenshare::CScreenshareManager> m_previousScreenshare;
    UP<IPC::Socket2::CSocket2>           m_previousSocket2;
    UP<Desktop::CWindowState>            m_previousWindows;
    UP<Event::CEventBus>                 m_previousEventBus;
    UP<Config::CConfigWatcher>           m_previousWatcher;
    UP<Config::IConfigManager>           m_previousConfig;
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
};

TEST_F(CWorkspaceCaptureSessionTest, BothModesAdvertiseTransformedSizeAndAlphaFormat) {
    for (const auto mode : {0U, 1U}) {
        SCOPED_TRACE(mode);
        auto session = Screenshare::mgr()->newSession(nullptr, source(mode));
        ASSERT_TRUE(session);
        EXPECT_TRUE(session->isActive());
        EXPECT_EQ(session->monitor(), m_monitor);
        EXPECT_EQ(session->bufferSize(), Vector2D(1080, 1920));
        EXPECT_EQ(session->allowedFormats(), (std::vector<DRMFormat>{DRM_FORMAT_ARGB8888}));

        auto frame = session->nextFrame(false);
        EXPECT_FALSE(frame->done());
        EXPECT_EQ(frame->bufferSize(), session->bufferSize());
        EXPECT_EQ(frame->transform(), WL_OUTPUT_TRANSFORM_NORMAL);
    }
}

TEST_F(CWorkspaceCaptureSessionTest, OnlySizeChangesNotifyConstraintsAndFramesKeepTheirOriginalSize) {
    auto session = Screenshare::mgr()->newSession(nullptr, source());
    ASSERT_TRUE(session);
    auto frame    = session->nextFrame(false);
    int  changes  = 0;
    auto listener = session->m_events.constraintsChanged.listen([&] { ++changes; });

    m_workspace->m_events.renamed.emit();
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 0);
    m_monitor->m_transformedSize = {1280, 720};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(session->bufferSize(), Vector2D(1280, 720));
    EXPECT_EQ(frame->bufferSize(), Vector2D(1080, 1920));
    EXPECT_EQ(session->nextFrame(false)->bufferSize(), Vector2D(1280, 720));
    m_monitor->m_events.modeChanged.emit();
    m_monitor->m_transformedSize = {0, 720};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(session->bufferSize(), Vector2D(1280, 720));
    EXPECT_TRUE(session->isActive());
}

TEST_F(CWorkspaceCaptureSessionTest, MoveReplacesDisconnectListenerEvenWhenSizeIsUnchanged) {
    auto session = Screenshare::mgr()->newSession(nullptr, source());
    ASSERT_TRUE(session);
    int  changes = 0, stops = 0;
    auto changed           = session->m_events.constraintsChanged.listen([&] { ++changes; });
    auto stopped           = session->m_events.stopped.listen([&] { ++stops; });
    auto next              = makeMonitor({1080, 1920});
    m_workspace->m_monitor = next;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_EQ(session->monitor(), next);
    EXPECT_EQ(changes, 0);
    m_monitor->m_events.disconnect.emit();
    EXPECT_TRUE(session->isActive());
    EXPECT_EQ(stops, 0);
    next->m_transformedSize = {2560, 1440};
    next->m_events.modeChanged.emit();
    EXPECT_EQ(session->bufferSize(), Vector2D(2560, 1440));
    EXPECT_EQ(changes, 1);
    next->m_events.disconnect.emit();
    next->m_events.disconnect.emit();
    EXPECT_FALSE(session->isActive());
    EXPECT_EQ(stops, 1);
}

TEST_F(CWorkspaceCaptureSessionTest, RemovalRetainsConstraintsAndAllowsNewSessionsAndFrames) {
    auto state   = source();
    auto session = Screenshare::mgr()->newSession(nullptr, state);
    ASSERT_TRUE(session);
    int  changes = 0, stops = 0;
    auto changed = session->m_events.constraintsChanged.listen([&] { ++changes; });
    auto stopped = session->m_events.stopped.listen([&] { ++stops; });
    auto frame   = session->nextFrame(false);
    m_workspace.reset();
    ASSERT_TRUE(state->removed());
    m_monitor->m_transformedSize = {800, 600};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_TRUE(session->isActive());
    EXPECT_EQ(changes, 0);
    EXPECT_EQ(stops, 0);
    EXPECT_EQ(session->bufferSize(), Vector2D(1080, 1920));
    EXPECT_EQ(session->allowedFormats(), (std::vector<DRMFormat>{DRM_FORMAT_ARGB8888}));
    EXPECT_FALSE(frame->done());
    EXPECT_FALSE(session->nextFrame(false)->done());
    auto later = Screenshare::mgr()->newSession(nullptr, state);
    ASSERT_TRUE(later);
    EXPECT_TRUE(later->isActive());
    EXPECT_EQ(later->bufferSize(), session->bufferSize());
    EXPECT_EQ(later->allowedFormats(), session->allowedFormats());
    m_monitor->m_events.disconnect.emit();
    EXPECT_FALSE(session->isActive());
    EXPECT_FALSE(later->isActive());
    EXPECT_TRUE(frame->done());
    EXPECT_EQ(stops, 1);
}

TEST_F(CWorkspaceCaptureSessionTest, MissingMonitorStopsOnceAndDoesNotResumeOnReattachment) {
    auto session = Screenshare::mgr()->newSession(nullptr, source());
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    int  stops = 0, changes = 0;
    auto stopped = session->m_events.stopped.listen([&] { ++stops; });
    auto changed = session->m_events.constraintsChanged.listen([&] { ++changes; });
    m_workspace->m_monitor.reset();
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_FALSE(session->isActive());
    EXPECT_TRUE(frame->done());
    EXPECT_EQ(stops, 1);
    m_monitor->m_transformedSize = {800, 600};
    m_workspace->m_monitor       = m_monitor;
    m_workspace->m_events.monitorChanged.emit();
    session->stop();
    EXPECT_FALSE(session->isActive());
    EXPECT_EQ(stops, 1);
    EXPECT_EQ(changes, 0);
    EXPECT_EQ(session->bufferSize(), Vector2D(1080, 1920));
}

TEST_F(CWorkspaceCaptureSessionTest, FactoryRejectsNullStateButReturnsStoppedSessionForUnavailableSource) {
    EXPECT_FALSE(Screenshare::mgr()->newSession(nullptr, SP<Screenshare::CWorkspaceCaptureSource>{}));
    m_workspace->m_monitor.reset();
    auto session = Screenshare::mgr()->newSession(nullptr, source());
    ASSERT_TRUE(session);
    EXPECT_FALSE(session->isActive());
    EXPECT_EQ(session->bufferSize(), Vector2D(0, 0));
    EXPECT_TRUE(session->nextFrame(false)->done());
}

TEST_F(CWorkspaceCaptureSessionTest, WorkspaceSessionsAndUnsubmittedFramesDoNotRequestOutputCopyFB) {
    auto session = Screenshare::mgr()->newSession(nullptr, source());
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    EXPECT_FALSE(Screenshare::mgr()->isOutputBeingSSd(m_monitor));
    EXPECT_TRUE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
    const auto state = Screenshare::mgr()->outputCopyFBState(m_monitor);
    EXPECT_EQ(state.activeSessions, 0U);
    EXPECT_EQ(state.sharingSessions, 0U);
    EXPECT_EQ(state.pendingFrames, 0U);
    EXPECT_FALSE(state.needsCopyFB());
    EXPECT_FALSE(Screenshare::mgr()->outputNeedsCopyFB(m_monitor));
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_FALSE(frame->done());
    session->stop();
    EXPECT_FALSE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
    EXPECT_TRUE(frame->done());
}

// Override the public queries instead of add(), which connects a real output.
class CSessionTestMonitorState : public State::CMonitorStateTracker {
  public:
    explicit CSessionTestMonitorState(PHLMONITOR monitor) : m_monitors{monitor} {
        ;
    }
    const std::vector<PHLMONITOR>& allMonitors() const override {
        return m_monitors;
    }
    const std::vector<PHLMONITOR>& monitors() const override {
        return m_monitors;
    }

  private:
    std::vector<PHLMONITOR> m_monitors;
};

class CWorkspaceCaptureFrameTest : public CWorkspaceCaptureSessionTest {
  protected:
    void SetUp() override {
        CWorkspaceCaptureSessionTest::SetUp();
        if (HasFatalFailure())
            return;

        m_previousMonitors          = std::move(State::monitorState());
        m_previousPermissions       = std::move(g_pDynamicPermissionManager);
        m_installedFrameGlobals     = true;
        State::monitorState()       = makeUnique<CSessionTestMonitorState>(m_monitor);
        g_pDynamicPermissionManager = makeUnique<CDynamicPermissionManager>();

        *CConfigValue<Config::INTEGER>("ecosystem:enforce_permissions").ptr() = 0;

        std::array<int, 2> sockets = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        Hyprutils::OS::CFileDescriptor serverSocket{sockets[0]};
        m_socket = Hyprutils::OS::CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), serverSocket.get());
        ASSERT_NE(m_client, nullptr);
        serverSocket.take();
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (m_installedFrameGlobals) {
            g_pDynamicPermissionManager = std::move(m_previousPermissions);
            State::monitorState()       = std::move(m_previousMonitors);
        }
        CWorkspaceCaptureSessionTest::TearDown();
    }

    SP<CSessionTestBuffer> buffer(const Vector2D& size, uint32_t format = DRM_FORMAT_ARGB8888) {
        auto buffer                  = makeShared<CSessionTestBuffer>();
        buffer->size                 = size;
        buffer->m_format             = format;
        buffer->m_resource           = CWLBufferResource::create(makeShared<CWlBuffer>(m_client, 1, m_nextID++));
        buffer->m_resource->m_buffer = buffer;
        EXPECT_TRUE(buffer->m_resource->good());
        return buffer;
    }

    CSessionTestRenderer& renderer() {
        return sc<CSessionTestRenderer&>(*g_pHyprRenderer);
    }

    void expectClear(size_t index, const CHyprColor& color, const Vector2D& size) {
        ASSERT_LT(index, renderer().m_elements->m_clears.size());
        const auto& clear = renderer().m_elements->m_clears[index];
        // Element dispatch performs an sRGB color-conversion round trip.
        EXPECT_NEAR(clear.color.r, color.r, 1e-5);
        EXPECT_NEAR(clear.color.g, color.g, 1e-5);
        EXPECT_NEAR(clear.color.b, color.b, 1e-5);
        EXPECT_FLOAT_EQ(clear.color.a, color.a);
        const CRegion full{CBox{{}, size}};
        EXPECT_TRUE(clear.damage.copy().subtract(full).empty());
        EXPECT_TRUE(full.copy().subtract(clear.damage).empty());
    }

    wl_client* m_client = nullptr; // Owned by the isolated display.

  private:
    bool                            m_installedFrameGlobals = false;
    uint32_t                        m_nextID                = 2;
    Hyprutils::OS::CFileDescriptor  m_socket;
    UP<State::CMonitorStateTracker> m_previousMonitors;
    UP<CDynamicPermissionManager>   m_previousPermissions;
};

TEST_F(CWorkspaceCaptureFrameTest, LiveAndRemovedFramesClearTheFullBufferWithoutCopyFB) {
    for (const auto mode : {0U, 1U}) {
        SCOPED_TRACE(mode);
        auto state   = source(mode);
        auto session = Screenshare::mgr()->newSession(m_client, state);
        ASSERT_TRUE(session);
        auto target = buffer(session->bufferSize());
        for (const bool removed : {false, true}) {
            SCOPED_TRACE(removed);
            renderer().m_elements->m_clears.clear();
            target->m_readbacks.clear();
            std::vector<Screenshare::eScreenshareResult> results;
            auto                                         frame = session->nextFrame(false);
            ASSERT_EQ(frame->share(target, CRegion{-10, -10, 20, 20}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
            if (removed)
                m_workspace->m_events.destroy.emit();
            ASSERT_EQ(state->removed(), removed);
            EXPECT_FALSE(frame->done());
            EXPECT_FALSE(Screenshare::mgr()->outputNeedsCopyFB(m_monitor));
            EXPECT_EQ(Screenshare::mgr()->outputCopyFBState(m_monitor).pendingFrames, 0U);

            Screenshare::mgr()->onOutputCommit(m_monitor, false);
            EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
            EXPECT_TRUE(frame->done());
            EXPECT_TRUE(session->isActive());
            EXPECT_FALSE(renderer().context().active());
            ASSERT_EQ(renderer().m_elements->m_clears.size(), 1U);
            expectClear(0, removed ? CHyprColor{0, 0, 0, 0} : CHyprColor{0, 1, 0, 1}, session->bufferSize());
            EXPECT_EQ(target->m_readbacks, (std::vector<CBox>{CBox{{}, session->bufferSize()}}));
            EXPECT_FALSE(m_monitor->resources()->hasMirrorFB());
            Screenshare::mgr()->onOutputCommit(m_monitor, false);
            EXPECT_EQ(results.size(), 2U);
            EXPECT_EQ(target->m_readbacks.size(), 1U);
        }
    }
    EXPECT_EQ(renderer().m_begins, 4);
    EXPECT_EQ(renderer().m_ends, 4);
}

TEST_F(CWorkspaceCaptureFrameTest, SubmissionRejectsMissingBufferWrongSizeFormatAndStaleConstraints) {
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    std::vector<Screenshare::eScreenshareResult> results;
    auto                                         frame    = session->nextFrame(false);
    const auto                                   callback = [&](auto result) { results.push_back(result); };
    EXPECT_EQ(frame->share(nullptr, {}, callback), Screenshare::ERROR_NO_BUFFER);
    EXPECT_EQ(frame->share(buffer({800, 600}), {}, callback), Screenshare::ERROR_BUFFER_SIZE);
    EXPECT_EQ(frame->share(buffer(session->bufferSize(), DRM_FORMAT_XRGB8888), {}, callback), Screenshare::ERROR_BUFFER_FORMAT);
    auto original                = buffer(session->bufferSize());
    m_monitor->m_transformedSize = {800, 600};
    m_monitor->m_events.modeChanged.emit();
    EXPECT_EQ(frame->share(original, {}, callback), Screenshare::ERROR_BUFFER_SIZE);
    EXPECT_EQ(frame->share(buffer(session->bufferSize()), {}, callback), Screenshare::ERROR_BUFFER_SIZE);
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(renderer().m_begins, 0);
    EXPECT_FALSE(frame->done());

    auto fresh = session->nextFrame(false);
    ASSERT_EQ(fresh->share(buffer(session->bufferSize()), {}, callback), Screenshare::ERROR_NONE);
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_TRUE(fresh->done());
    EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
}

TEST_F(CWorkspaceCaptureFrameTest, ResizeAfterSubmissionFailsOnceBeforeRendering) {
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    auto                                         target = buffer(session->bufferSize());
    std::vector<Screenshare::eScreenshareResult> results;
    auto                                         frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(target, {}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
    m_monitor->m_transformedSize = {800, 600};
    m_monitor->m_events.modeChanged.emit();
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_NOT_COPIED}));
    EXPECT_TRUE(frame->done());
    EXPECT_TRUE(target->m_readbacks.empty());
    EXPECT_EQ(renderer().m_begins, 0);
    EXPECT_TRUE(renderer().m_elements->m_clears.empty());
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    frame.reset();
    EXPECT_EQ(results.size(), 2U);
}

TEST_F(CWorkspaceCaptureFrameTest, StaleSessionBlocksDirectScanoutOnlyWhileSubmittedFrameIsPending) {
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    auto                                         target = buffer(session->bufferSize());
    std::vector<Screenshare::eScreenshareResult> results;
    auto                                         frame = session->nextFrame(false);
    // Exercise the real inactivity timer without dispatching unrelated events.
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    g_pEventLoopManager->onTimerFire();
    ASSERT_TRUE(session->isStale());
    ASSERT_TRUE(session->isActive());
    EXPECT_FALSE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
    const auto callback = [&](auto result) { results.push_back(result); };
    ASSERT_EQ(frame->share(target, {}, callback), Screenshare::ERROR_NONE);
    EXPECT_TRUE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
    EXPECT_FALSE(Screenshare::mgr()->outputNeedsCopyFB(m_monitor));
    EXPECT_EQ(Screenshare::mgr()->outputCopyFBState(m_monitor).pendingFrames, 0U);
    auto other = makeMonitor({640, 480});
    EXPECT_FALSE(Screenshare::mgr()->isOutputDSBlocked(other));
    Screenshare::mgr()->onOutputCommit(other, false);
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(renderer().m_begins, 0);
    frame.reset();
    EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_NOT_COPIED}));
    EXPECT_FALSE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));

    results.clear();
    frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(target, {}, callback), Screenshare::ERROR_NONE);
    EXPECT_TRUE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
    EXPECT_TRUE(frame->done());
    EXPECT_FALSE(session->isStale());
    EXPECT_FALSE(Screenshare::mgr()->outputNeedsCopyFB(m_monitor));
    session->stop();
    EXPECT_FALSE(Screenshare::mgr()->isOutputDSBlocked(m_monitor));
}

// Seed only compositor-owned mapped state, without invoking the live map/layout
// lifecycle. Explicit instantiation keeps this test access out of production headers.
static bool& snapshotTestMapped(PHLWINDOW window);

template <bool Desktop::View::CWindow::* Mapped>
struct SSnapshotTestMappedAccess {
    friend bool& snapshotTestMapped(PHLWINDOW window) {
        return window.get()->*Mapped;
    }
};

template struct SSnapshotTestMappedAccess<&Desktop::View::CWindow::m_isMapped>;

static PHLWINDOW makeSnapshotTestWindow(PHLWORKSPACE workspace) {
    auto window = Desktop::View::CWindow::create(makeUnique<Desktop::View::CWaylandBackend>(nullptr));
    // Query tests need window metadata, not a window in the rendered scene.
    Desktop::windowState()->removeSafe(window);
    window->m_workspace        = workspace;
    window->m_monitor          = workspace->m_monitor;
    snapshotTestMapped(window) = true;
    return window;
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandRequiresSubmittedFrameRatherThanFreshSession) {
    auto window = makeSnapshotTestWindow(m_workspace);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    ASSERT_FALSE(session->isStale());
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    auto frame = session->nextFrame(false);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    EXPECT_EQ(frame->share(nullptr, {}, [](auto) { ; }), Screenshare::ERROR_NO_BUFFER);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    EXPECT_TRUE(std::as_const(*Screenshare::mgr()).needsWorkspaceCaptureSnapshot(window));

    frame.reset();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    session->stop();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    session.reset();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandRetainsRecentCopiesAndAcceptsPendingFramesAfterInactivity) {
    auto window  = makeSnapshotTestWindow(m_workspace);
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    ASSERT_TRUE(frame->done());
    frame.reset();
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    g_pEventLoopManager->onTimerFire();
    ASSERT_TRUE(session->isActive());
    ASSERT_TRUE(session->isStale());
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    frame = session->nextFrame(false);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    frame.reset();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandRejectsUnmappedHiddenAndNoScreenShareWindows) {
    auto window  = makeSnapshotTestWindow(m_workspace);
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(nullptr));
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    snapshotTestMapped(window) = false;
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    snapshotTestMapped(window) = true;
    window->setHidden(true);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->setHidden(false);
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->m_ruleApplicator->noScreenShare().unset(Desktop::Types::PRIORITY_SET_PROP);
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->m_monitor.reset();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandRequiresLiveSourceAndMatchingWorkspaceAndMonitor) {
    auto window  = makeSnapshotTestWindow(m_workspace);
    auto state   = source();
    auto session = Screenshare::mgr()->newSession(m_client, state);
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    auto otherWorkspace    = makeShared<CSessionTestWorkspace>(m_monitor);
    otherWorkspace->m_self = otherWorkspace;
    window->m_workspace    = otherWorkspace;
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->m_workspace = m_workspace;
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    auto otherMonitor      = makeMonitor({1080, 1920});
    m_workspace->m_monitor = otherMonitor;
    m_workspace->m_events.monitorChanged.emit();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    window->m_monitor = otherMonitor;
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    m_workspace->m_events.destroy.emit();
    ASSERT_TRUE(state->removed());
    ASSERT_TRUE(session->isActive());
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandIncludesOutputGlobalPinnedFloatersOnlyInEverythingMode) {
    auto otherWorkspace    = makeShared<CSessionTestWorkspace>(m_monitor);
    otherWorkspace->m_self = otherWorkspace;
    auto window            = makeSnapshotTestWindow(otherWorkspace);
    window->m_state |= Desktop::View::WINDOW_STATE_PINNED;
    window->windowTarget()->setFloatingInitial(true);

    for (const auto mode :
         {HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_WINDOWS_ONLY, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING}) {
        SCOPED_TRACE(mode);
        auto session = Screenshare::mgr()->newSession(m_client, source(mode));
        ASSERT_TRUE(session);
        auto frame = session->nextFrame(false);
        ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
        EXPECT_EQ(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window), mode == HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING);
        window->m_state &= ~Desktop::View::WINDOW_STATE_PINNED;
        EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
        window->m_state |= Desktop::View::WINDOW_STATE_PINNED;
        window->windowTarget()->setFloatingInitial(false);
        EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
        window->windowTarget()->setFloatingInitial(true);
        auto otherMonitor = makeMonitor({1080, 1920});
        window->m_monitor = otherMonitor;
        EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
        window->m_monitor = m_monitor;
    }
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandRejectsPendingAndDeniedPermissionIncludingRecentCopies) {
    auto window  = makeSnapshotTestWindow(m_workspace);
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    auto frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    *CConfigValue<Config::INTEGER>("ecosystem:enforce_permissions").ptr() = 1;

    for (const auto mode : {PERMISSION_RULE_ALLOW_MODE_PENDING, PERMISSION_RULE_ALLOW_MODE_DENY, PERMISSION_RULE_ALLOW_MODE_ALLOW}) {
        SCOPED_TRACE(mode);
        g_pDynamicPermissionManager->clearConfigPermissions();
        g_pDynamicPermissionManager->addConfigPermissionRule(".*", PERMISSION_TYPE_SCREENCOPY, mode);
        EXPECT_EQ(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window), mode == PERMISSION_RULE_ALLOW_MODE_ALLOW);
        EXPECT_FALSE(frame->done());
        EXPECT_EQ(renderer().m_begins, 0);
    }

    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    ASSERT_TRUE(frame->done());
    frame.reset();
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    g_pDynamicPermissionManager->clearConfigPermissions();
    g_pDynamicPermissionManager->addConfigPermissionRule(".*", PERMISSION_TYPE_SCREENCOPY, PERMISSION_RULE_ALLOW_MODE_DENY);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandCombinesOnlyMatchingSessionsAndTheirOwnPendingFrames) {
    auto window            = makeSnapshotTestWindow(m_workspace);
    auto otherWorkspace    = makeShared<CSessionTestWorkspace>(m_monitor);
    otherWorkspace->m_self = otherWorkspace;
    auto otherSource       = makeShared<Screenshare::CWorkspaceCaptureSource>(otherWorkspace, 0);
    auto otherSession      = Screenshare::mgr()->newSession(m_client, otherSource);
    auto first             = Screenshare::mgr()->newSession(m_client, source());
    auto second            = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(otherSession);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    auto otherFrame = otherSession->nextFrame(false);
    ASSERT_EQ(otherFrame->share(buffer(otherSession->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));

    auto firstFrame  = first->nextFrame(false);
    auto secondFrame = second->nextFrame(false);
    ASSERT_EQ(firstFrame->share(buffer(first->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    ASSERT_EQ(secondFrame->share(buffer(second->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    first->stop();
    EXPECT_TRUE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    second.reset();
    EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
}

TEST_F(CWorkspaceCaptureFrameTest, SnapshotDemandIgnoresOtherCaptureTypes) {
    auto window = makeSnapshotTestWindow(m_workspace);
    window->sizeAnimation()->setValueAndWarp({100, 80});
    m_monitor->m_output->state->setFormat(DRM_FORMAT_XRGB8888);
    auto workspaceSession = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(workspaceSession);

    for (const auto type : {Screenshare::SHARE_MONITOR, Screenshare::SHARE_REGION, Screenshare::SHARE_WINDOW}) {
        SCOPED_TRACE(type);
        UP<Screenshare::CScreenshareSession> session;
        switch (type) {
            case Screenshare::SHARE_MONITOR: session = Screenshare::mgr()->newSession(m_client, m_monitor); break;
            case Screenshare::SHARE_REGION: session = Screenshare::mgr()->newSession(m_client, m_monitor, CBox{0, 0, 50, 50}); break;
            case Screenshare::SHARE_WINDOW: session = Screenshare::mgr()->newSession(m_client, window); break;
            default: FAIL() << "Unexpected capture type";
        }
        ASSERT_TRUE(session);
        auto frame = session->nextFrame(false);
        ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
        EXPECT_FALSE(Screenshare::mgr()->needsWorkspaceCaptureSnapshot(window));
    }
}
