#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <desktop/state/FadingOutState.hpp>
#include <desktop/state/FocusState.hpp>
#include <desktop/state/PopupFadeout.hpp>
#include <desktop/state/WindowState.hpp>
#include <desktop/view/Popup.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <desktop/view/window/Window.hpp>
#include <event/EventBus.hpp>
#include <ipc/s2/Impl.hpp>
#include <ipc/s2/S2.hpp>
#include <layout/target/WindowTarget.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <managers/SessionLockManager.hpp>
#include <managers/permissions/DynamicPermissionManager.hpp>
#include <managers/screenshare/ScreenshareManager.hpp>
#include <managers/screenshare/WorkspaceCaptureSource.hpp>
#include <output/Monitor.hpp>
#include <output/MonitorResources.hpp>
#include <pointer/PointerManager.hpp>
#include <protocols/LayerShell.hpp>
#include <protocols/LockNotify.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/SessionLock.hpp>
#include <protocols/core/Compositor.hpp>
#include <render/ElementRenderer.hpp>
#include <render/Renderbuffer.hpp>
#include <render/SceneResources.hpp>
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
    bool                          m_dma    = false;
    std::vector<CBox>             m_readbacks;

    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return m_dma ? Aquamarine::BUFFER_TYPE_DMABUF : Aquamarine::BUFFER_TYPE_SHM;
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
        if (m_dma)
            return {};
        return {.success = true, .fd = -1, .format = m_format, .size = size, .stride = sc<int>(size.x) * 4};
    }
    Aquamarine::SDMABUFAttrs dmabuf() override {
        Aquamarine::SDMABUFAttrs attrs;
        attrs.success = m_dma;
        attrs.size    = size;
        attrs.format  = m_format;
        return attrs; // Metadata only: the fake renderbuffer never imports an FD.
    }
};

class CSessionTestTexture : public Render::ITexture {
  public:
    explicit CSessionTestTexture(const Vector2D& size) {
        m_size             = size;
        m_imageDescription = NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION;
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

// Record readback requests rather than allocating GPU storage or mapping SHM.
class CSessionTestFramebuffer : public Render::IFramebuffer {
  public:
    void release() override {
        m_fbAllocated = false;
    }
    bool readPixels(CHLBufferReference buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {
        EXPECT_EQ(buffer->size, m_size);
        EXPECT_EQ(buffer->shm().format, m_drmFormat);
        EXPECT_LE(x + width, m_size.x);
        EXPECT_LE(y + height, m_size.y);
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

// Fadeout rendering requires a texture, but only its metadata participates here.
class CSessionTestSnapshotFramebuffer : public CSessionTestFramebuffer {
  private:
    bool internalAlloc(int width, int height, DRMFormat format) override {
        m_tex              = makeShared<CSessionTestTexture>(Vector2D{width, height});
        m_tex->m_drmFormat = format;
        return true;
    }
};

class CSessionTestElements : public Render::IElementRenderer {
  public:
    struct SClear {
        CHyprColor color;
        CRegion    damage;
        Vector2D   framebufferSize;
        Vector2D   exportSize;
    };
    std::vector<SClear> m_clears;
    struct STexture {
        WP<Render::ITexture>   texture;
        WP<CWLSurfaceResource> surface;
        CBox                   box;
    };
    std::vector<STexture> m_textures;

  protected:
    void draw(Render::CRenderContext& ctx, WP<CClearPassElement> element, const CRegion& damage) override {
        EXPECT_TRUE(ctx.m_renderingCapture);
        EXPECT_TRUE(ctx.m_blockSurfaceFeedback);
        m_clears.push_back({element->m_data.color, damage, ctx.m_data.currentFB->m_size, ctx.m_data.outFB->m_size});
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
    void draw(Render::CRenderContext& ctx, WP<CTexPassElement> element, const CRegion&) override {
        EXPECT_TRUE(ctx.m_renderingCapture);
        EXPECT_TRUE(ctx.m_blockSurfaceFeedback);
        EXPECT_TRUE(ctx.m_data.blockScreenShader);
        m_textures.push_back({element->m_data.tex, element->m_data.surface, element->m_data.box});
    }
    void draw(Render::CRenderContext&, WP<CTextureMatteElement>, const CRegion&) override {
        ADD_FAILURE() << "Unexpected texture matte draw";
    }
};

class CSessionTestRenderbuffer : public Render::IRenderbuffer {
  public:
    CSessionTestRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format, int& unbinds) : IRenderbuffer(buffer, format), m_unbinds(unbinds) {
        m_framebuffer = makeShared<CSessionTestFramebuffer>();
        m_framebuffer->alloc(buffer->size.x, buffer->size.y, buffer->dmabuf().format);
        m_good = true;
    }
    void bind() override {
        ;
    }
    void unbind() override {
        EXPECT_TRUE(g_pHyprRenderer->context().m_renderingCapture);
        EXPECT_TRUE(g_pHyprRenderer->context().m_blockSurfaceFeedback);
        ++m_unbinds;
    }

  private:
    int& m_unbinds;
};

// Exercise the real render lifecycle and element dispatch without a GL backend.
class CSessionTestRenderer : public Render::IHyprRenderer {
  public:
    using IHyprRenderer::createTexture;

    UP<CSessionTestElements> m_elements = makeUnique<CSessionTestElements>();
    int                      m_begins = 0, m_ends = 0;
    int                                      m_dmaBegins = 0, m_unbinds = 0, m_passStarts = 0;
    int                                      m_blurs = 0;
    std::vector<WP<Render::CSceneResources>> m_resources;

    eType                    type() override {
        return RT_GL;
    }
    Render::SRenderResult endRender(const std::function<void()>& callback) override {
        EXPECT_TRUE(context().active());
        EXPECT_TRUE(context().m_data.blockScreenShader);
        expectCapture(context());
        expectCaptureTarget(context());
        ++m_ends;
        // Match the backend's deferred dispatch, including surface feedback guards.
        currentPass(context()).render(context(), context().m_data.damage);
        expectCapture(context());
        finishRender();
        if (callback)
            callback();
        return {};
    }
    void startRenderPass(Render::CRenderContext& ctx) override {
        expectCapture(ctx);
        ++m_passStarts;
    }
    SP<Render::ITexture> renderText(const std::string&, CHyprColor, int, bool, const std::string&, int, int) override {
        return nullptr; // initAssets must not load fonts in this fixture.
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
        ++m_blurs;
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
    void expectCapture(Render::CRenderContext& ctx) {
        EXPECT_TRUE(ctx.active());
        EXPECT_TRUE(ctx.m_renderingCapture);
        EXPECT_TRUE(ctx.m_blockSurfaceFeedback);
        EXPECT_FALSE(ctx.m_renderingSnapshot);
        EXPECT_TRUE(ctx.readOnlyEffects());
        EXPECT_FALSE(ctx.m_swapchainAcquired);
    }
    void expectCaptureTarget(Render::CRenderContext& ctx) {
        ASSERT_TRUE(ctx.m_data.outFB);
        EXPECT_EQ(ctx.m_data.outFB->imageDescription(), NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        EXPECT_EQ(ctx.m_data.outFB->m_drmFormat, DRM_FORMAT_ARGB8888);
    }
    void prepareTarget(Render::CRenderContext& ctx, PHLMONITOR monitor, CRegion& damage, SP<Render::IFramebuffer> fb, bool simple) {
        EXPECT_TRUE(ctx.active());
        EXPECT_FALSE(simple);
        EXPECT_EQ(ctx.m_data.projectionType, Render::RPT_MONITOR);
        ASSERT_TRUE(ctx.sceneResources());
        EXPECT_TRUE(ctx.sceneResources()->isolated());
        EXPECT_NE(ctx.sceneResources(), monitor->resources()->sceneResources());
        ASSERT_TRUE(ctx.sceneResources()->bufferDescription());
        const auto& description = *ctx.sceneResources()->bufferDescription();
        EXPECT_EQ(description.format, DRM_FORMAT_ARGB8888);
        EXPECT_EQ(description.imageDescription, NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        ASSERT_TRUE(ctx.sceneResources()->blurFramebuffer());
        EXPECT_EQ(ctx.sceneResources()->blurFramebuffer()->m_drmFormat, DRM_FORMAT_ARGB8888);
        EXPECT_EQ(ctx.sceneResources()->blurFramebuffer()->imageDescription(), NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        m_resources.emplace_back(ctx.sceneResources());
        ++m_begins;
        // Keep output and working FB distinct so setting only currentFB's
        // description cannot accidentally satisfy the DMA output assertion.
        ctx.m_data.currentFB = ctx.m_data.mainFB = makeShared<CSessionTestFramebuffer>();
        ctx.m_data.currentFB->alloc(monitor->m_transformedSize.x, monitor->m_transformedSize.y, DRM_FORMAT_ARGB8888);
        ctx.m_data.currentFB->setImageDescription(description.imageDescription);
        ctx.m_data.outFB  = fb;
        ctx.m_data.fbSize = monitor->m_transformedSize;
        setDamage(ctx, damage, std::nullopt);
    }
    bool initRenderBuffer(Render::CRenderContext& ctx, SP<Aquamarine::IBuffer> buffer, uint32_t format) override {
        EXPECT_TRUE(buffer->dmabuf().success);
        ctx.m_currentRenderbuffer = makeShared<CSessionTestRenderbuffer>(buffer, format, m_unbinds);
        return true;
    }
    bool beginRenderInternal(Render::CRenderContext& ctx, PHLMONITOR monitor, CRegion& damage, bool simple) override {
        EXPECT_EQ(ctx.m_mode, Render::RENDER_MODE_TO_BUFFER);
        ++m_dmaBegins;
        prepareTarget(ctx, monitor, damage, ctx.m_currentRenderbuffer->getFB(), simple);
        return !::testing::Test::HasFatalFailure();
    }
    bool beginFullFakeRenderInternal(Render::CRenderContext& ctx, PHLMONITOR monitor, CRegion& damage, SP<Render::IFramebuffer> fb, bool simple) override {
        EXPECT_EQ(ctx.m_mode, Render::RENDER_MODE_FULL_FAKE);
        prepareTarget(ctx, monitor, damage, fb, simple);
        return !::testing::Test::HasFatalFailure();
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
        m_previousSessionLock       = std::move(g_pSessionLockManager);
        m_previousLockProtocol      = std::move(PROTO::sessionLock);
        m_previousFadeouts          = std::move(Desktop::fadingOutState());
        m_installedFrameGlobals     = true;
        State::monitorState()       = makeUnique<CSessionTestMonitorState>(m_monitor);
        g_pDynamicPermissionManager = makeUnique<CDynamicPermissionManager>();
        PROTO::sessionLock          = makeUnique<CSessionLockProtocol>(&ext_session_lock_manager_v1_interface, 1, "workspace-capture-lock-test");
        g_pSessionLockManager       = makeUnique<CSessionLockManager>();
        Desktop::fadingOutState()   = makeUnique<Desktop::CFadingOutState>();

        *CConfigValue<Config::INTEGER>("ecosystem:enforce_permissions").ptr() = 0;
        *CConfigValue<Config::INTEGER>("misc:disable_hyprland_logo").ptr()    = 1;
        *CConfigValue<Config::INTEGER>("misc:disable_splash_rendering").ptr() = 1;
        *CConfigValue<Config::INTEGER>("misc:background_color").ptr()         = 0;
        *CConfigValue<Config::INTEGER>("render:xp_mode").ptr()                = 0;
        *CConfigValue<Config::INTEGER>("debug:pass").ptr()                    = 0;
        *CConfigValue<Config::BOOL>("decoration:blur:enabled").ptr()          = false;
        m_monitor->m_output->state->setFormat(DRM_FORMAT_XRGB8888);

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
            Desktop::fadingOutState()   = std::move(m_previousFadeouts);
            g_pSessionLockManager       = std::move(m_previousSessionLock);
            PROTO::sessionLock          = std::move(m_previousLockProtocol);
            g_pDynamicPermissionManager = std::move(m_previousPermissions);
            State::monitorState()       = std::move(m_previousMonitors);
        }
        CWorkspaceCaptureSessionTest::TearDown();
    }

    SP<CSessionTestBuffer> buffer(const Vector2D& size, uint32_t format = DRM_FORMAT_ARGB8888, bool dma = false) {
        auto buffer                  = makeShared<CSessionTestBuffer>();
        buffer->size                 = size;
        buffer->m_format             = format;
        buffer->m_dma                = dma;
        buffer->m_resource           = CWLBufferResource::create(makeShared<CWlBuffer>(m_client, 1, m_nextID++));
        buffer->m_resource->m_buffer = buffer;
        EXPECT_TRUE(buffer->m_resource->good());
        return buffer;
    }

    CSessionTestRenderer& renderer() {
        return sc<CSessionTestRenderer&>(*g_pHyprRenderer);
    }

    void capture(Screenshare::CScreenshareSession& session, bool dma = false, bool overlayCursor = false) {
        renderer().m_elements->m_clears.clear();
        renderer().m_elements->m_textures.clear();
        std::vector<Screenshare::eScreenshareResult> results;
        const auto                                   target = buffer(session.bufferSize(), DRM_FORMAT_ARGB8888, dma);
        auto                                         frame  = session.nextFrame(overlayCursor);
        ASSERT_EQ(frame->share(target, {}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
        Screenshare::mgr()->onOutputCommit(session.monitor(), false);
        EXPECT_TRUE(frame->done());
        EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
        EXPECT_EQ(target->m_readbacks.size(), dma ? 0U : 1U);
        EXPECT_FALSE(renderer().context().active());
        EXPECT_FALSE(renderer().context().m_renderingCapture);
        EXPECT_FALSE(renderer().context().m_blockSurfaceFeedback);
        EXPECT_FALSE(renderer().context().sceneResources());
        EXPECT_FALSE(Screenshare::mgr()->outputNeedsCopyFB(session.monitor()));
    }

    void expectClear(size_t index, const CHyprColor& color, const Vector2D& size) {
        ASSERT_LT(index, renderer().m_elements->m_clears.size());
        const auto& clear = renderer().m_elements->m_clears[index];
        // Element dispatch performs an sRGB color-conversion round trip.
        EXPECT_NEAR(clear.color.r, color.r, 1e-5);
        EXPECT_NEAR(clear.color.g, color.g, 1e-5);
        EXPECT_NEAR(clear.color.b, color.b, 1e-5);
        EXPECT_FLOAT_EQ(clear.color.a, color.a);
        EXPECT_EQ(clear.framebufferSize, size);
        const CRegion full{CBox{{}, size}};
        EXPECT_TRUE(clear.damage.copy().subtract(full).empty());
        EXPECT_TRUE(full.copy().subtract(clear.damage).empty());
    }

    wl_client* m_client = nullptr; // Owned by the isolated display.
    uint32_t   m_nextID = 2;

  private:
    bool                            m_installedFrameGlobals = false;
    Hyprutils::OS::CFileDescriptor  m_socket;
    UP<State::CMonitorStateTracker> m_previousMonitors;
    UP<CDynamicPermissionManager>   m_previousPermissions;
    UP<CSessionLockManager>         m_previousSessionLock;
    UP<CSessionLockProtocol>        m_previousLockProtocol;
    UP<Desktop::CFadingOutState>    m_previousFadeouts;
};

TEST_F(CWorkspaceCaptureFrameTest, EmptyAndRemovedFramesClearTransparentWithoutCopyFB) {
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
            // Everything additionally draws the fixture's transparent background.
            ASSERT_EQ(renderer().m_elements->m_clears.size(), !removed && mode == HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING ? 2U : 1U);
            for (size_t i = 0; i < renderer().m_elements->m_clears.size(); ++i)
                expectClear(i, CHyprColor{0, 0, 0, 0}, session->bufferSize());
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

TEST_F(CWorkspaceCaptureFrameTest, DmaAndShmRetainSessionOwnedSRGBResourcesThroughDeferredEnd) {
    auto first  = Screenshare::mgr()->newSession(m_client, source());
    auto second = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    m_monitor->m_imageDescription   = NColorManagement::LINEAR_IMAGE_DESCRIPTION;
    const auto monitorResources     = m_monitor->resources();
    m_monitor->m_blurFBDirty        = false;
    m_monitor->m_blurFBShouldRender = true;

    capture(*first);
    ASSERT_EQ(renderer().m_resources.size(), 1U);
    auto firstResources = renderer().m_resources.back();
    ASSERT_TRUE(firstResources);
    capture(*first, true);
    ASSERT_EQ(renderer().m_resources.size(), 2U);
    EXPECT_EQ(renderer().m_resources.back(), firstResources);
    capture(*second, true);
    ASSERT_EQ(renderer().m_resources.size(), 3U);
    EXPECT_NE(renderer().m_resources.back(), firstResources);
    EXPECT_FALSE(m_monitor->m_blurFBDirty);
    EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
    EXPECT_EQ(monitorResources->m_blurFB->imageDescription(), NColorManagement::LINEAR_IMAGE_DESCRIPTION);
    EXPECT_FALSE(monitorResources->hasMirrorFB());
    EXPECT_EQ(renderer().m_dmaBegins, 2);
    EXPECT_EQ(renderer().m_unbinds, 2);
    EXPECT_EQ(renderer().m_begins, 3);
    EXPECT_EQ(renderer().m_ends, 3);
    EXPECT_GE(renderer().m_passStarts, 3);

    first.reset();
    EXPECT_TRUE(firstResources.expired());
}

// Seed mapped state without emitting map signals, which arrange outputs and
// change focus. As with the snapshot tests below, all access stays test-only.
static bool& sessionTestLayerMapped(PHLLS layer);

template <bool Desktop::View::CLayerSurface::* Mapped>
struct SSessionTestLayerAccess {
    friend bool& sessionTestLayerMapped(PHLLS layer) {
        return layer.get()->*Mapped;
    }
};

template struct SSessionTestLayerAccess<&Desktop::View::CLayerSurface::m_mapped>;

static void seedSessionCursor(Pointer::CPointerManager& pointer, SP<Desktop::View::CWLSurface> surface, const Vector2D& position);

template <auto Image, auto Position>
struct SSessionTestCursorAccess {
    friend void seedSessionCursor(Pointer::CPointerManager& pointer, SP<Desktop::View::CWLSurface> surface, const Vector2D& position) {
        auto& image       = pointer.*Image;
        image.surface     = surface;
        image.size        = surface->resource()->m_current.size;
        image.hotspot     = {1, 2};
        image.scale       = 1.F;
        pointer.*Position = position;
    }
};

template struct SSessionTestCursorAccess<&Pointer::CPointerManager::m_currentCursorImage, &Pointer::CPointerManager::m_pointerPos>;

class CWorkspaceCaptureSceneTest : public CWorkspaceCaptureFrameTest {
  protected:
    void SetUp() override {
        CWorkspaceCaptureFrameTest::SetUp();
        if (HasFatalFailure())
            return;
        m_previousPresentation  = std::move(PROTO::presentation);
        m_previousLockNotify    = std::move(PROTO::lockNotify);
        m_previousPointer       = std::move(Pointer::mgr());
        m_installedSceneGlobals = true;
        PROTO::presentation     = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "workspace-capture-presentation-test");
        PROTO::lockNotify       = makeUnique<CLockNotifyProtocol>(&hyprland_lock_notifier_v1_interface, 1, "workspace-capture-lock-notify-test");
        Pointer::mgr()          = makeUnique<Pointer::CPointerManager>();
        Animation::mgr()->createAnimation(Vector2D{}, m_workspace->m_renderOffset, Config::animationTree()->getAnimationPropertyConfig("workspacesIn"), AVARDAMAGE_NONE);
        Animation::mgr()->createAnimation(1.F, m_workspace->m_alpha, Config::animationTree()->getAnimationPropertyConfig("workspacesIn"), AVARDAMAGE_NONE);
    }

    void TearDown() override {
        if (m_installedSceneGlobals) {
            renderer().m_elements->m_textures.clear();
            Pointer::mgr() = std::move(m_previousPointer);
            m_layers.clear();
            m_layerResources.clear();
            m_surfaces.clear();
            PROTO::presentation = std::move(m_previousPresentation);
            PROTO::lockNotify   = std::move(m_previousLockNotify);
        }
        CWorkspaceCaptureFrameTest::TearDown();
    }

    SP<CWLSurfaceResource> surface(const Vector2D& size = {40, 30}) {
        auto wire    = makeShared<CWlSurface>(m_client, 6, m_nextID++);
        auto surface = makeShared<CWLSurfaceResource>(wire);
        EXPECT_TRUE(surface->good());
        // This fixture owns resources directly instead of a compositor protocol.
        wire->setDestroy([](CWlSurface*) { ADD_FAILURE() << "Unexpected surface destroy request"; });
        wire->setOnDestroy([](CWlSurface*) { ; });
        surface->m_self         = surface;
        surface->m_current.size = surface->m_current.bufferSize = size;
        surface->m_current.texture                              = makeShared<CSessionTestTexture>(size);
        surface->m_current.callbacks.emplace_back(makeShared<CWLCallbackResource>(makeShared<CWlCallback>(m_client, 1, m_nextID++)));
        m_surfaces.emplace_back(surface);
        return surface;
    }

    PHLLS layer(zwlrLayerShellV1Layer plane, const Vector2D& position = {10, 20}) {
        auto surf     = surface();
        auto wire     = makeShared<CZwlrLayerSurfaceV1>(m_client, 4, m_nextID++);
        auto resource = makeShared<CLayerShellResource>(wire, surf, "workspace-capture-test", m_monitor, plane);
        EXPECT_TRUE(resource->good());
        wire->setDestroy([](CZwlrLayerSurfaceV1*) { ADD_FAILURE() << "Unexpected layer destroy request"; });
        wire->setOnDestroy([](CZwlrLayerSurfaceV1*) { ; });
        auto layer                    = Desktop::View::CLayerSurface::create(resource);
        sessionTestLayerMapped(layer) = true;
        resource->m_mapped            = true;
        layer->positionAnimation()->setValueAndWarp(position + m_monitor->m_position);
        layer->sizeAnimation()->setValueAndWarp(surf->m_current.size);
        layer->alpha()[Desktop::View::LS_ALPHA_FADE]->setValueAndWarp(1.F);
        m_layerResources.emplace_back(resource);
        m_layers.emplace_back(layer);
        EXPECT_TRUE(layer->mapped());
        EXPECT_EQ(layer->m_monitor, m_monitor);
        return layer;
    }

    std::vector<WP<CWLSurfaceResource>> drawnSurfaces() {
        std::vector<WP<CWLSurfaceResource>> surfaces;
        for (const auto& texture : renderer().m_elements->m_textures) {
            if (texture.surface)
                surfaces.emplace_back(texture.surface);
        }
        return surfaces;
    }

    void expectUnpaced(SP<CWLSurfaceResource> surface) {
        ASSERT_EQ(surface->m_current.callbacks.size(), 1U);
        EXPECT_TRUE(surface->m_current.callbacks.front()->good());
    }

    SP<Render::IFramebuffer> popupSnapshot() {
        auto snapshot = makeShared<CSessionTestSnapshotFramebuffer>();
        EXPECT_TRUE(snapshot->alloc(m_monitor->m_transformedSize.x, m_monitor->m_transformedSize.y, DRM_FORMAT_ARGB8888));
        snapshot->setImageDescription(NColorManagement::DEFAULT_SRGB_IMAGE_DESCRIPTION);
        return snapshot;
    }

    void releaseLayer(PHLLS& layer) {
        std::erase(m_layers, layer);
        layer.reset();
    }

  private:
    bool                                 m_installedSceneGlobals = false;
    UP<CPresentationProtocol>            m_previousPresentation;
    UP<CLockNotifyProtocol>              m_previousLockNotify;
    UP<Pointer::CPointerManager>         m_previousPointer;
    std::vector<SP<CWLSurfaceResource>>  m_surfaces;
    std::vector<SP<CLayerShellResource>> m_layerResources;
    std::vector<PHLLS>                   m_layers;
};

TEST_F(CWorkspaceCaptureSceneTest, ModesSelectShellSurfacesAndFilterPrivateContentAtCopyTime) {
    auto bottom       = layer(ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
    auto top          = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP, {80, 20});
    auto privateLayer = layer(ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, {140, 20});
    privateLayer->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);

    for (const bool dma : {false, true}) {
        for (const auto mode :
             {HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_WINDOWS_ONLY, HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING}) {
            SCOPED_TRACE(dma);
            SCOPED_TRACE(mode);
            auto session = Screenshare::mgr()->newSession(m_client, source(mode));
            ASSERT_TRUE(session);
            capture(*session, dma);
            const std::vector<WP<CWLSurfaceResource>> expected = mode == HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING ?
                std::vector<WP<CWLSurfaceResource>>{bottom->resource(), top->resource()} :
                std::vector<WP<CWLSurfaceResource>>{};
            EXPECT_EQ(drawnSurfaces(), expected);
            expectUnpaced(bottom->resource());
            expectUnpaced(top->resource());
            expectUnpaced(privateLayer->resource());
        }
    }

    auto session = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
    ASSERT_TRUE(session);
    privateLayer->m_ruleApplicator->noScreenShare().unset(Desktop::Types::PRIORITY_SET_PROP);
    capture(*session);
    EXPECT_EQ(drawnSurfaces(), (std::vector<WP<CWLSurfaceResource>>{bottom->resource(), top->resource(), privateLayer->resource()}));
    auto frame = session->nextFrame(false);
    ASSERT_EQ(frame->share(buffer(session->bufferSize()), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
    privateLayer->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
    renderer().m_elements->m_textures.clear();
    Screenshare::mgr()->onOutputCommit(m_monitor, false);
    EXPECT_TRUE(frame->done());
    EXPECT_EQ(drawnSurfaces(), (std::vector<WP<CWLSurfaceResource>>{bottom->resource(), top->resource()}));
}

TEST_F(CWorkspaceCaptureSceneTest, LayerPopupFadeoutTracksLivePrivacyWithoutRetainingOwner) {
    auto           owner     = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    const PHLLSREF weakOwner = owner;
    auto           popup     = Desktop::View::CPopup::create(owner);
    auto           snapshot  = popupSnapshot();
    auto           fadeout   = Desktop::CPopupFadeout::create(popup, snapshot, 1.F);
    ASSERT_TRUE(fadeout);
    ASSERT_TRUE(snapshot->isAllocated());
    EXPECT_EQ(fadeout->source().type, Desktop::eFadeoutSource::LAYER);
    EXPECT_FALSE(fadeout->source().noScreenShare);
    Desktop::fadingOutState()->add(fadeout);
    auto session = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
    ASSERT_TRUE(session);

    for (const bool dma : {false, true}) {
        SCOPED_TRACE(dma);
        capture(*session, dma);
        ASSERT_EQ(renderer().m_elements->m_textures.size(), 2U);
        EXPECT_EQ(renderer().m_elements->m_textures.back().texture, snapshot->getTexture());

        // The snapshot already exists and the frame is pending when privacy changes.
        auto frame = session->nextFrame(false);
        ASSERT_EQ(frame->share(buffer(session->bufferSize(), DRM_FORMAT_ARGB8888, dma), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
        owner->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
        EXPECT_TRUE(fadeout->source().noScreenShare);
        renderer().m_elements->m_textures.clear();
        Screenshare::mgr()->onOutputCommit(m_monitor, false);
        EXPECT_TRUE(frame->done());
        EXPECT_TRUE(renderer().m_elements->m_textures.empty());
        expectUnpaced(owner->resource());

        owner->m_ruleApplicator->noScreenShare().unset(Desktop::Types::PRIORITY_SET_PROP);
        EXPECT_FALSE(fadeout->source().noScreenShare);
    }

    owner->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
    releaseLayer(owner);
    EXPECT_TRUE(weakOwner.expired());
    EXPECT_FALSE(popup->layerOwner());
    EXPECT_EQ(fadeout->source().type, Desktop::eFadeoutSource::LAYER);
    EXPECT_FALSE(fadeout->source().noScreenShare); // The initial snapshot was permitted.
    capture(*session);
    ASSERT_EQ(renderer().m_elements->m_textures.size(), 1U);
    EXPECT_EQ(renderer().m_elements->m_textures.front().texture, snapshot->getTexture());
}

TEST_F(CWorkspaceCaptureSceneTest, LayerPopupFadeoutKeepsInitialDenialAndSuppressesEffectsAfterOwnerExpiry) {
    auto           owner     = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    const PHLLSREF weakOwner = owner;
    owner->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
    *CConfigValue<Config::BOOL>("decoration:blur:enabled").ptr()   = true;
    *CConfigValue<Config::INTEGER>("decoration:blur:popups").ptr() = 1;
    auto popup                                                     = Desktop::View::CPopup::create(owner);
    auto snapshot                                                  = popupSnapshot();
    auto fadeout                                                   = Desktop::CPopupFadeout::create(popup, snapshot, 1.F);
    ASSERT_TRUE(fadeout);
    ASSERT_TRUE(fadeout->effects().textureBlur.enabled);
    // Snapshot effects are frozen; disabling live blur must not remove this check.
    *CConfigValue<Config::BOOL>("decoration:blur:enabled").ptr() = false;
    Desktop::fadingOutState()->add(fadeout);
    auto session = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
    ASSERT_TRUE(session);

    capture(*session);
    EXPECT_TRUE(renderer().m_elements->m_textures.empty());
    owner->m_ruleApplicator->noScreenShare().unset(Desktop::Types::PRIORITY_SET_PROP);
    EXPECT_FALSE(owner->m_ruleApplicator->noScreenShare().valueOrDefault());
    EXPECT_TRUE(fadeout->source().noScreenShare);
    for (const bool dma : {false, true}) {
        capture(*session, dma);
        ASSERT_EQ(renderer().m_elements->m_textures.size(), 1U);
        EXPECT_EQ(renderer().m_elements->m_textures.front().surface, owner->resource());
        EXPECT_EQ(renderer().m_blurs, 0);
    }

    releaseLayer(owner);
    EXPECT_TRUE(weakOwner.expired());
    EXPECT_FALSE(popup->layerOwner());
    EXPECT_EQ(fadeout->source().type, Desktop::eFadeoutSource::LAYER);
    EXPECT_TRUE(fadeout->source().noScreenShare);
    EXPECT_TRUE(fadeout->effects().textureBlur.enabled);
    for (const bool dma : {false, true}) {
        capture(*session, dma);
        EXPECT_TRUE(renderer().m_elements->m_textures.empty());
        EXPECT_EQ(renderer().m_blurs, 0);
    }
}

TEST_F(CWorkspaceCaptureSceneTest, DeferredSurfaceDrawAndDiscardDoNotConsumeFrameOrPresentationFeedback) {
    auto shell    = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    auto surf     = shell->resource();
    auto feedback = makeShared<CPresentationFeedback>(makeUnique<CWpPresentationFeedback>(m_client, 2, m_nextID++), surf);
    ASSERT_TRUE(feedback->good());
    surf->m_current.presentationFeedbacks.emplace_back(feedback);
    auto session = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
    ASSERT_TRUE(session);
    for (const bool dma : {false, true}) {
        for (const bool visible : {true, false}) {
            SCOPED_TRACE(dma);
            SCOPED_TRACE(visible);
            shell->positionAnimation()->setValueAndWarp(m_monitor->m_position + (visible ? Vector2D{10, 20} : Vector2D{2000, 3000}));
            capture(*session, dma);
            EXPECT_EQ(drawnSurfaces(), visible ? std::vector<WP<CWLSurfaceResource>>{surf} : std::vector<WP<CWLSurfaceResource>>{});
            expectUnpaced(surf);
            ASSERT_EQ(surf->m_current.presentationFeedbacks.size(), 1U);
            EXPECT_EQ(surf->m_current.presentationFeedbacks.front(), feedback);
            EXPECT_FALSE(PROTO::presentation->hasPendingFeedbacks());
        }
    }
}

TEST_F(CWorkspaceCaptureSceneTest, LockAndRemovalSuppressQueuedContentEvenWithLockXrayEnabled) {
    auto shell         = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    auto cursorSurface = surface({12, 16});
    auto cursor        = Desktop::View::CWLSurface::create();
    cursor->assign(cursorSurface);
    seedSessionCursor(*Pointer::mgr(), cursor, m_monitor->m_position + Vector2D{20, 30});
    m_monitor->m_activeWorkspace                                   = m_workspace;
    *CConfigValue<Config::INTEGER>("misc:session_lock_xray").ptr() = 1;
    for (const bool dma : {false, true}) {
        auto state   = source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING);
        auto session = Screenshare::mgr()->newSession(m_client, state);
        ASSERT_TRUE(session);
        capture(*session, dma);
        ASSERT_FALSE(drawnSurfaces().empty());
        for (const bool removed : {false, true}) {
            auto frame = session->nextFrame(true);
            ASSERT_EQ(frame->share(buffer(session->bufferSize(), DRM_FORMAT_ARGB8888, dma), {}, [](auto) { ; }), Screenshare::ERROR_NONE);
            // State changes after submission must govern the copy, not its request.
            if (removed)
                m_workspace->m_events.destroy.emit();
            else
                PROTO::sessionLock->forceLock();
            renderer().m_elements->m_clears.clear();
            renderer().m_elements->m_textures.clear();
            Screenshare::mgr()->onOutputCommit(m_monitor, false);
            EXPECT_TRUE(frame->done());
            EXPECT_TRUE(session->isActive());
            EXPECT_TRUE(renderer().m_elements->m_textures.empty());
            ASSERT_EQ(renderer().m_elements->m_clears.size(), 1U);
            expectClear(0, {0, 0, 0, 0}, session->bufferSize());
            expectUnpaced(shell->resource());
            expectUnpaced(cursorSurface);
            if (!removed)
                PROTO::sessionLock->forceUnlock();
        }
    }
}

TEST_F(CWorkspaceCaptureSceneTest, RemovedSourceClearsResizedScratchAndPreservesFrozenExportDamage) {
    auto shell   = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    auto state   = source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING);
    auto session = Screenshare::mgr()->newSession(m_client, state);
    ASSERT_TRUE(session);
    const auto    exportSize = session->bufferSize();
    const CRegion exportDamage{CBox{{}, exportSize}};
    int           changes = 0;
    auto          changed = session->m_events.constraintsChanged.listen([&] { ++changes; });

    capture(*session);
    ASSERT_EQ(drawnSurfaces(), (std::vector<WP<CWLSurfaceResource>>{shell->resource()}));
    m_workspace->m_events.destroy.emit();
    ASSERT_TRUE(state->removed());

    // The owner can grow or shrink after removal; exports keep their last size.
    for (const auto& scratchSize : {Vector2D{1440, 2560}, Vector2D{640, 480}}) {
        for (const bool dma : {false, true}) {
            SCOPED_TRACE(::testing::Message() << scratchSize.x << "x" << scratchSize.y);
            SCOPED_TRACE(dma);
            std::vector<Screenshare::eScreenshareResult> results;
            auto                                         target = buffer(exportSize, DRM_FORMAT_ARGB8888, dma);
            auto                                         frame  = session->nextFrame(false);
            ASSERT_EQ(frame->share(target, {-10, -10, 20, 20}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
            m_monitor->m_transformedSize = scratchSize;
            m_monitor->m_pixelSize       = {scratchSize.y, scratchSize.x};
            m_monitor->m_size            = scratchSize / m_monitor->m_scale;
            m_monitor->m_events.modeChanged.emit();
            EXPECT_EQ(changes, 0);
            EXPECT_EQ(session->bufferSize(), exportSize);
            EXPECT_EQ(frame->bufferSize(), exportSize);

            renderer().m_elements->m_clears.clear();
            renderer().m_elements->m_textures.clear();
            Screenshare::mgr()->onOutputCommit(m_monitor, false);
            EXPECT_TRUE(frame->done());
            EXPECT_TRUE(session->isActive());
            EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
            ASSERT_EQ(renderer().m_elements->m_clears.size(), 1U);
            expectClear(0, {0, 0, 0, 0}, scratchSize);
            EXPECT_EQ(renderer().m_elements->m_clears.front().exportSize, exportSize);
            EXPECT_TRUE(renderer().m_elements->m_textures.empty());
            EXPECT_TRUE(frame->damage().copy().subtract(exportDamage).empty());
            EXPECT_TRUE(exportDamage.copy().subtract(frame->damage()).empty());
            EXPECT_EQ(target->m_readbacks, (dma ? std::vector<CBox>{} : std::vector<CBox>{CBox{{}, exportSize}}));
            EXPECT_FALSE(renderer().context().active());
            expectUnpaced(shell->resource());
        }
    }
}

TEST_F(CWorkspaceCaptureSceneTest, PendingPermissionDefersSceneAndDeniedPermissionNeverRenders) {
    auto shell                                                            = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    *CConfigValue<Config::INTEGER>("ecosystem:enforce_permissions").ptr() = 1;
    for (const bool dma : {false, true}) {
        g_pDynamicPermissionManager->clearConfigPermissions();
        g_pDynamicPermissionManager->addConfigPermissionRule(".*", PERMISSION_TYPE_SCREENCOPY, PERMISSION_RULE_ALLOW_MODE_PENDING);
        auto session = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
        ASSERT_TRUE(session);
        std::vector<Screenshare::eScreenshareResult> results;
        auto                                         target = buffer(session->bufferSize(), DRM_FORMAT_ARGB8888, dma);
        auto                                         frame  = session->nextFrame(false);
        ASSERT_EQ(frame->share(target, {}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
        const auto begins = renderer().m_begins;
        Screenshare::mgr()->onOutputCommit(m_monitor, false);
        EXPECT_FALSE(frame->done());
        EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP}));
        EXPECT_EQ(renderer().m_begins, begins);
        EXPECT_TRUE(target->m_readbacks.empty());

        g_pDynamicPermissionManager->clearConfigPermissions();
        g_pDynamicPermissionManager->addConfigPermissionRule(".*", PERMISSION_TYPE_SCREENCOPY, PERMISSION_RULE_ALLOW_MODE_ALLOW);
        renderer().m_elements->m_textures.clear();
        Screenshare::mgr()->onOutputCommit(m_monitor, false);
        EXPECT_TRUE(frame->done());
        EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_COPIED}));
        EXPECT_EQ(drawnSurfaces(), (std::vector<WP<CWLSurfaceResource>>{shell->resource()}));

        results.clear();
        frame = session->nextFrame(false);
        ASSERT_EQ(frame->share(target, {}, [&](auto result) { results.push_back(result); }), Screenshare::ERROR_NONE);
        g_pDynamicPermissionManager->clearConfigPermissions();
        g_pDynamicPermissionManager->addConfigPermissionRule(".*", PERMISSION_TYPE_SCREENCOPY, PERMISSION_RULE_ALLOW_MODE_DENY);
        renderer().m_elements->m_textures.clear();
        Screenshare::mgr()->onOutputCommit(m_monitor, false);
        EXPECT_TRUE(frame->done());
        EXPECT_EQ(results, (std::vector<Screenshare::eScreenshareResult>{Screenshare::RESULT_TIMESTAMP, Screenshare::RESULT_NOT_COPIED}));
        EXPECT_EQ(renderer().m_begins, begins + 1);
        EXPECT_TRUE(renderer().m_elements->m_textures.empty());
        expectUnpaced(shell->resource());
    }
}

TEST_F(CWorkspaceCaptureSceneTest, InactiveCaptureDoesNotChangeWorkspaceSwitchOrMonitorState) {
    auto active    = makeShared<CSessionTestWorkspace>(m_monitor);
    active->m_self = active;
    active->setVisible(true);
    m_monitor->m_activeWorkspace = active;
    m_workspace->setVisible(false);
    m_workspace->m_renderOffset->setValueAndWarp({700, -300});
    m_workspace->m_alpha->setValueAndWarp(0.F);
    const auto focusWindow  = Desktop::focusState()->window();
    const auto focusMonitor = Desktop::focusState()->monitor();
    const auto position     = m_monitor->m_position;
    const auto transform    = m_monitor->m_transform;
    auto       shell        = layer(ZWLR_LAYER_SHELL_V1_LAYER_TOP);
    auto       session      = Screenshare::mgr()->newSession(m_client, source(HYPRLAND_WORKSPACE_IMAGE_CAPTURE_SOURCE_MANAGER_V1_CAPTURE_MODE_EVERYTHING));
    ASSERT_TRUE(session);
    capture(*session);
    EXPECT_EQ(drawnSurfaces(), (std::vector<WP<CWLSurfaceResource>>{shell->resource()}));
    EXPECT_EQ(m_monitor->m_activeWorkspace, active);
    EXPECT_FALSE(m_workspace->visible());
    EXPECT_TRUE(active->visible());
    EXPECT_EQ(m_workspace->m_renderOffset->value(), Vector2D(700, -300));
    EXPECT_FLOAT_EQ(m_workspace->m_alpha->value(), 0.F);
    EXPECT_EQ(m_monitor->m_position, position);
    EXPECT_EQ(m_monitor->m_transform, transform);
    EXPECT_EQ(Desktop::focusState()->window(), focusWindow);
    EXPECT_EQ(Desktop::focusState()->monitor(), focusMonitor);
}

TEST_F(CWorkspaceCaptureSceneTest, CursorRequiresOverlayAndActiveOwnerAndOutputIntersectionWithoutPacing) {
    auto cursorSurface = surface({12, 16});
    auto cursor        = Desktop::View::CWLSurface::create();
    cursor->assign(cursorSurface);
    auto other    = makeShared<CSessionTestWorkspace>(m_monitor);
    other->m_self = other;
    // Visibility alone is not enough: another output can also display a workspace.
    m_workspace->setVisible(true);
    for (const bool dma : {false, true}) {
        for (const bool active : {false, true}) {
            m_monitor->m_activeWorkspace = active ? m_workspace : other;
            for (const bool overlay : {false, true}) {
                for (const auto& [pos, intersects] : std::array<std::pair<Vector2D, bool>, 3>{
                         std::pair{Vector2D{20, 30}, true},
                         std::pair{Vector2D{-2, 30}, true},
                         std::pair{Vector2D{2000, 3000}, false},
                     }) {
                    SCOPED_TRACE(dma);
                    SCOPED_TRACE(active);
                    SCOPED_TRACE(overlay);
                    SCOPED_TRACE(intersects);
                    seedSessionCursor(*Pointer::mgr(), cursor, m_monitor->m_position + pos);
                    auto session = Screenshare::mgr()->newSession(m_client, source());
                    ASSERT_TRUE(session);
                    capture(*session, dma, overlay);
                    const bool expected = active && overlay && intersects;
                    ASSERT_EQ(renderer().m_elements->m_textures.size(), expected ? 1U : 0U);
                    if (expected) {
                        const auto& texture = renderer().m_elements->m_textures.front();
                        EXPECT_EQ(texture.texture, cursorSurface->m_current.texture);
                        EXPECT_EQ(texture.box, (CBox{pos - Vector2D{1, 2}, cursorSurface->m_current.size}.scale(m_monitor->m_scale)));
                    }
                    expectUnpaced(cursorSurface);
                }
            }
        }
    }
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

TEST_F(CWorkspaceCaptureSceneTest, WindowPopupFadeoutsKeepOriginAndInitialPrivacyWithWeakOwners) {
    auto               owner     = makeSnapshotTestWindow(m_workspace);
    const PHLWINDOWREF weakOwner = owner;
    owner->m_state |= Desktop::View::WINDOW_STATE_PINNED;
    owner->windowTarget()->setFloatingInitial(true);
    auto popup             = Desktop::View::CPopup::create(owner);
    auto permittedSnapshot = popupSnapshot();
    auto permitted         = Desktop::CPopupFadeout::create(popup, nullptr, 1.F, permittedSnapshot);
    ASSERT_TRUE(permitted);
    EXPECT_FALSE(permitted->source().noScreenShare);

    owner->m_ruleApplicator->noScreenShare().set(true, Desktop::Types::PRIORITY_SET_PROP);
    auto denied = Desktop::CPopupFadeout::create(popup, nullptr, 1.F, popupSnapshot());
    ASSERT_TRUE(denied);
    EXPECT_TRUE(permitted->source().noScreenShare);
    EXPECT_TRUE(denied->source().noScreenShare);
    Desktop::fadingOutState()->add(permitted);
    Desktop::fadingOutState()->add(denied);
    auto session = Screenshare::mgr()->newSession(m_client, source());
    ASSERT_TRUE(session);
    capture(*session);
    EXPECT_TRUE(renderer().m_elements->m_textures.empty());

    owner->m_ruleApplicator->noScreenShare().unset(Desktop::Types::PRIORITY_SET_PROP);
    EXPECT_FALSE(permitted->source().noScreenShare);
    EXPECT_TRUE(denied->source().noScreenShare);
    capture(*session);
    ASSERT_EQ(renderer().m_elements->m_textures.size(), 1U);
    EXPECT_EQ(renderer().m_elements->m_textures.front().texture, permittedSnapshot->getTexture());

    owner->m_state &= ~Desktop::View::WINDOW_STATE_PINNED;
    owner.reset();
    EXPECT_TRUE(weakOwner.expired());
    EXPECT_FALSE(popup->windowOwner());
    for (const auto& fadeout : {permitted, denied}) {
        const auto origin = fadeout->source();
        EXPECT_EQ(origin.type, Desktop::eFadeoutSource::WINDOW);
        EXPECT_EQ(origin.workspace.lock(), PHLWORKSPACE{m_workspace});
        EXPECT_TRUE(origin.pinned);
    }
    EXPECT_FALSE(permitted->source().noScreenShare);
    EXPECT_TRUE(denied->source().noScreenShare);
    capture(*session, true);
    ASSERT_EQ(renderer().m_elements->m_textures.size(), 1U);
    EXPECT_EQ(renderer().m_elements->m_textures.front().texture, permittedSnapshot->getTexture());
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
