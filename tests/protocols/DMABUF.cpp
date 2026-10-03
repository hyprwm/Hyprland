#include <protocols/LinuxDMABUF.hpp>
#include <protocols/MesaDRM.hpp>
#include <Compositor.hpp>
#include <animation/AnimationManager.hpp>
#include <config/ConfigValue.hpp>
#include <config/lua/ConfigManager.hpp>
#include <config/shared/animation/AnimationTree.hpp>
#include <config/shared/inotify/ConfigWatcher.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <output/MonitorResources.hpp>
#include <render/Renderer.hpp>
#include <render/SceneResources.hpp>
#include <render/SyncFDManager.hpp>
#include <render/Renderbuffer.hpp>

#include <gtest/gtest.h>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprutils::OS;

// Exercise the actual request handlers using an isolated Wayland server and pipes
// as plane FDs. No compositor, renderer, DRM device or session is required.
class CDMABUFParamsTest : public ::testing::Test {
  protected:
    void                                                       SetUp() override;
    void                                                       TearDown() override;
    void                                                       addPlane(uint32_t plane, CFileDescriptor& reader, uint64_t modifier = 0, uint32_t offset = 0, uint32_t stride = 4);
    void                                                       sendFDRequest(std::span<uint32_t> request, CFileDescriptor& reader);
    void                                                       createBuffer(int32_t width, int32_t height, uint32_t flags = 0);
    void                                                       expectOpen(const CFileDescriptor& reader);
    void                                                       expectClosed(const CFileDescriptor& reader);

    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    UP<CLinuxDMABUFParamsResource>                             m_params;
    wl_client*                                                 m_client = nullptr; // Owned by m_display.
};

void CDMABUFParamsTest::SetUp() {
    m_display.reset(wl_display_create());
    ASSERT_NE(m_display, nullptr);

    std::array<int, 2> sockets = {
        -1,
        -1,
    };
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor serverSocket{sockets[0]};
    m_socket = CFileDescriptor{sockets[1]};
    m_client = wl_client_create(m_display.get(), serverSocket.get());
    ASSERT_NE(m_client, nullptr);
    serverSocket.take(); // wl_client owns the socket now.

    auto resource = makeUnique<CZwpLinuxBufferParamsV1>(m_client, 5, 2);
    ASSERT_NE(resource->resource(), nullptr);
    auto wire = resource.get();
    m_params  = makeUnique<CLinuxDMABUFParamsResource>(std::move(resource));
    ASSERT_TRUE(m_params->good());

    // This fixture owns params instead of the global protocol manager.
    wire->setOnDestroy([this](CZwpLinuxBufferParamsV1*) { m_params.reset(); });
}

void CDMABUFParamsTest::TearDown() {
    m_params.reset();
    if (m_display)
        wl_display_destroy_clients(m_display.get());
}

void CDMABUFParamsTest::addPlane(uint32_t plane, CFileDescriptor& reader, uint64_t modifier, uint32_t offset, uint32_t stride) {
    // Wayland FDs travel in SCM_RIGHTS, not in the message body.
    std::array<uint32_t, 7> request = {
        2, (7U * sizeof(uint32_t) << 16) | 1U, plane, offset, stride, sc<uint32_t>(modifier >> 32), sc<uint32_t>(modifier),
    };
    sendFDRequest(request, reader);
}

void CDMABUFParamsTest::sendFDRequest(std::span<uint32_t> request, CFileDescriptor& reader) {
    std::array<int, 2> pipeFDs = {
        -1,
        -1,
    };
    ASSERT_EQ(pipe2(pipeFDs.data(), O_CLOEXEC | O_NONBLOCK), 0);
    reader = CFileDescriptor{pipeFDs[0]};
    CFileDescriptor writer{pipeFDs[1]};

    iovec           data = {
        .iov_base = request.data(),
        .iov_len  = request.size_bytes(),
    };
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int))> control = {};
    msghdr                                                     message = {
        .msg_iov        = &data,
        .msg_iovlen     = 1,
        .msg_control    = control.data(),
        .msg_controllen = control.size(),
    };
    auto ancillary        = CMSG_FIRSTHDR(&message);
    ancillary->cmsg_level = SOL_SOCKET;
    ancillary->cmsg_type  = SCM_RIGHTS;
    ancillary->cmsg_len   = CMSG_LEN(sizeof(int));
    const int fd          = writer.get();
    std::memcpy(CMSG_DATA(ancillary), &fd, sizeof(fd));

    ASSERT_EQ(sendmsg(m_socket.get(), &message, MSG_NOSIGNAL), sc<ssize_t>(request.size_bytes()));
    writer.reset();
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
}

void CDMABUFParamsTest::createBuffer(int32_t width, int32_t height, uint32_t flags) {
    std::array<uint32_t, 7> request = {
        2, (7U * sizeof(uint32_t) << 16) | 3U, 3, sc<uint32_t>(width), sc<uint32_t>(height), DRM_FORMAT_XRGB8888, flags,
    };
    ASSERT_EQ(send(m_socket.get(), request.data(), sizeof(request), MSG_NOSIGNAL), sc<ssize_t>(sizeof(request)));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
}

void CDMABUFParamsTest::expectOpen(const CFileDescriptor& reader) {
    char byte = 0;
    ASSERT_TRUE(reader.isValid());
    EXPECT_EQ(read(reader.get(), &byte, 1), -1);
    EXPECT_EQ(errno, EAGAIN);
}

void CDMABUFParamsTest::expectClosed(const CFileDescriptor& reader) {
    char byte = 0;
    ASSERT_TRUE(reader.isValid());
    EXPECT_EQ(read(reader.get(), &byte, 1), 0);
}

TEST_F(CDMABUFParamsTest, NewParamsHaveNoPlaneDescriptors) {
    EXPECT_TRUE(m_params->m_attrs.success);
    for (const auto fd : m_params->m_attrs.fds)
        EXPECT_EQ(fd, -1);
}

TEST_F(CDMABUFParamsTest, AbandonedParamsCloseAllPlanes) {
    std::array<CFileDescriptor, 4> readers;
    for (uint32_t plane = 0; plane < readers.size(); ++plane) {
        addPlane(plane, readers[plane]);
        expectOpen(readers[plane]);
    }

    m_params.reset();
    for (const auto& reader : readers)
        expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, AbandonedSparseParamsClosePlane) {
    CFileDescriptor reader;
    addPlane(3, reader);
    expectOpen(reader);
    m_params.reset();
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, ClientDisconnectClosesPlanes) {
    CFileDescriptor reader;
    addPlane(0, reader);
    expectOpen(reader);
    m_socket.reset();
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    EXPECT_FALSE(m_params);
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, AttributeCopiesDoNotOwnPlanes) {
    CFileDescriptor reader;
    addPlane(0, reader);
    ASSERT_TRUE(m_params);
    {
        auto attrs = m_params->m_attrs;
        EXPECT_NE(fcntl(attrs.fds[0], F_GETFD), -1);
    }
    expectOpen(reader);
    m_params.reset();
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, OutOfRangePlaneClosesIncomingFD) {
    CFileDescriptor reader;
    addPlane(4, reader);
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, AlreadyUsedParamsCloseIncomingFD) {
    m_params->m_used = true;
    CFileDescriptor reader;
    addPlane(0, reader);
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, DuplicatePlaneClosesIncomingFD) {
    CFileDescriptor first, duplicate;
    addPlane(0, first);
    addPlane(0, duplicate);
    expectClosed(duplicate);
    m_params.reset();
    expectClosed(first);
}

TEST_F(CDMABUFParamsTest, MismatchedModifierClosesIncomingFD) {
    CFileDescriptor first, mismatch;
    addPlane(0, first, 1);
    addPlane(1, mismatch, 2);
    expectClosed(mismatch);
    m_params.reset();
    expectClosed(first);
}

TEST_F(CDMABUFParamsTest, MissingFirstPlaneClosesFDOnDestruction) {
    CFileDescriptor reader;
    addPlane(1, reader);
    createBuffer(1, 1);
    m_params.reset();
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, PlaneGapClosesFDsOnDestruction) {
    CFileDescriptor first, third;
    addPlane(0, first);
    addPlane(2, third);
    createBuffer(1, 1);
    m_params.reset();
    expectClosed(first);
    expectClosed(third);
}

TEST_F(CDMABUFParamsTest, InvalidDimensionsCloseFDOnDestruction) {
    CFileDescriptor reader;
    addPlane(0, reader);
    createBuffer(0, 1);
    m_params.reset();
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, SizeOverflowClosesFDOnDestruction) {
    CFileDescriptor reader;
    addPlane(0, reader, 0, UINT32_MAX);
    createBuffer(1, 1);
    m_params.reset();
    expectClosed(reader);
}

TEST_F(CDMABUFParamsTest, UnsupportedFlagsCloseFDOnDestruction) {
    CFileDescriptor reader;
    addPlane(0, reader);
    createBuffer(1, 1, 1);
    m_params.reset();
    expectClosed(reader);
}

class CDMABUFTestTexture : public Render::ITexture {
  public:
    explicit CDMABUFTestTexture(bool valid) : m_valid(valid) {
        ;
    }
    bool ok() override {
        return m_valid;
    }
    void setTexParameter(GLenum, GLint) override {
        ADD_FAILURE() << "Unexpected GPU operation";
    }
    void allocate(const Vector2D&, uint32_t) override {
        ADD_FAILURE() << "Unexpected GPU operation";
    }
    void update(uint32_t, uint8_t*, uint32_t, const CRegion&) override {
        ADD_FAILURE() << "Unexpected GPU operation";
    }

  private:
    bool m_valid = true;
};

// Only texture import and format advertisement participate in these tests.
// The remaining renderer interface is inert; no GL implementation is constructed.
class CDMABUFTestRenderer : public Render::IHyprRenderer {
  public:
    using IHyprRenderer::createTexture;

    bool                                  m_failImport    = false;
    bool                                  m_throwOnImport = false;
    bool                                  m_validTexture  = true;
    std::vector<Aquamarine::SDMABUFAttrs> m_imports;

    SP<Render::ITexture>                  createTexture(const Aquamarine::SDMABUFAttrs& attrs, bool) override {
        m_imports.push_back(attrs);
        for (int i = 0; i < attrs.planes; ++i)
            EXPECT_NE(fcntl(attrs.fds[i], F_GETFD), -1);
        if (m_throwOnImport)
            throw std::runtime_error("Injected DMA-BUF import exception");
        if (m_failImport)
            return nullptr;
        return makeShared<CDMABUFTestTexture>(m_validTexture);
    }
    std::vector<SDRMFormat> getDRMFormats() override {
        return {};
    }
    eType type() override {
        return RT_GL;
    }
    Render::SRenderResult endRender(const std::function<void()>&) override {
        return {};
    }
    UP<Render::ISyncFDManager> createSyncFDManager() override {
        return nullptr;
    }
    WP<Render::IElementRenderer> elementRenderer() override {
        return {};
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
    std::vector<uint64_t> getDRMFormatModifiers(DRMFormat) override {
        return {};
    }
    SP<Render::IFramebuffer> createFB(const std::string&) override {
        return nullptr;
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
};

class CDMABUFBufferTest : public CDMABUFParamsTest {
  protected:
    void                              SetUp() override;
    void                              TearDown() override;
    CDMABUFTestRenderer&              renderer();
    SP<IHLBuffer>                     createdBuffer();
    virtual UP<Render::IHyprRenderer> makeRenderer() {
        return makeUnique<CDMABUFTestRenderer>();
    }

  private:
    bool                       m_installedGlobals = false;
    UP<CCompositor>            m_previousCompositor;
    UP<CEventLoopManager>      m_previousEventLoop;
    UP<Render::IHyprRenderer>  m_previousRenderer;
    UP<Event::CEventBus>       m_previousEventBus;
    UP<CLinuxDMABufV1Protocol> m_previousLinuxDma;
    UP<CMesaDRMProtocol>       m_previousMesa;
};

void CDMABUFBufferTest::SetUp() {
    CDMABUFParamsTest::SetUp();
    if (HasFatalFailure())
        return;

    m_previousCompositor = std::move(g_pCompositor);
    m_previousEventLoop  = std::move(g_pEventLoopManager);
    m_previousRenderer   = std::move(g_pHyprRenderer);
    m_previousEventBus   = std::move(Event::bus());
    m_previousLinuxDma   = std::move(PROTO::linuxDma);
    m_previousMesa       = std::move(PROTO::mesaDRM);
    m_installedGlobals   = true;

    Event::bus() = makeUnique<Event::CEventBus>();
    // Config-only construction avoids compositor startup and all session setup.
    g_pCompositor                     = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay        = m_display.get();
    g_pCompositor->m_wlEventLoop      = wl_display_get_event_loop(m_display.get());
    g_pCompositor->m_drm.fd           = -1;
    g_pCompositor->m_drmRenderNode.fd = -1;
    Aquamarine::SBackendImplementationOptions backend;
    backend.backendType        = Aquamarine::AQ_BACKEND_NULL;
    g_pCompositor->m_aqBackend = Aquamarine::CBackend::create({backend}, Aquamarine::SBackendOptions{});
    ASSERT_TRUE(g_pCompositor->m_aqBackend);
    g_pEventLoopManager = makeUnique<CEventLoopManager>(m_display.get(), g_pCompositor->m_wlEventLoop);
    g_pHyprRenderer     = makeRenderer();
    wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 0);

    // Do not emit ready: it would initialize real DRM format tables/devices.
    PROTO::linuxDma = makeUnique<CLinuxDMABufV1Protocol>(&zwp_linux_dmabuf_v1_interface, 5, "dmabuf-test");
    PROTO::mesaDRM  = makeUnique<CMesaDRMProtocol>(&wl_drm_interface, 2, "mesa-test");
}

void CDMABUFBufferTest::TearDown() {
    CDMABUFParamsTest::TearDown();
    if (!m_installedGlobals)
        return;

    PROTO::linuxDma     = std::move(m_previousLinuxDma);
    PROTO::mesaDRM      = std::move(m_previousMesa);
    g_pHyprRenderer     = std::move(m_previousRenderer);
    g_pEventLoopManager = std::move(m_previousEventLoop);
    g_pCompositor       = std::move(m_previousCompositor);
    Event::bus()        = std::move(m_previousEventBus);
}

CDMABUFTestRenderer& CDMABUFBufferTest::renderer() {
    return sc<CDMABUFTestRenderer&>(*g_pHyprRenderer);
}

SP<IHLBuffer> CDMABUFBufferTest::createdBuffer() {
    auto resource = wl_client_get_object(m_client, 3);
    if (!resource)
        return nullptr;
    return CWLBufferResource::fromResource(resource)->m_buffer.lock();
}

// Reuse the inert renderer to exercise lifecycle entrypoints without a GPU.
TEST_F(CDMABUFBufferTest, NestedRenderBeginDoesNotTouchActiveSession) {
    auto& context = renderer().context();
    ASSERT_TRUE(context.begin());
    context.m_mode                 = Render::RENDER_MODE_TO_BUFFER_READ_ONLY;
    context.m_data.mouseZoomFactor = 2.F;
    context.m_pass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{}));

    CRegion damage{1, 2, 3, 4};
    // Invalid inputs must never be consulted when another session is active.
    EXPECT_FALSE(renderer().beginFullFakeRender(nullptr, damage, nullptr));
    EXPECT_FALSE(renderer().beginRenderToBuffer(nullptr, damage, nullptr));
    EXPECT_FALSE(renderer().makeSnapshotFB(PHLWINDOW{}));
    EXPECT_FALSE(renderer().makeSnapshotFB(PHLLS{}));
    EXPECT_FALSE(renderer().makeSnapshotFB(WP<Desktop::View::CPopup>{}));
    EXPECT_TRUE(context.active());
    EXPECT_TRUE(context.m_pass.single());
    EXPECT_EQ(context.m_mode, Render::RENDER_MODE_TO_BUFFER_READ_ONLY);
    EXPECT_FLOAT_EQ(context.m_data.mouseZoomFactor, 2.F);
    EXPECT_EQ(damage.getExtents(), CBox(1, 2, 3, 4));
    renderer().abortRender();
}

TEST_F(CDMABUFBufferTest, AbortRenderReleasesPassAndAllowsReuse) {
    auto& context = renderer().context();
    ASSERT_TRUE(context.begin());
    auto                 texture  = makeShared<CDMABUFTestTexture>(true);
    WP<Render::ITexture> retained = texture;
    context.m_pass.add(makeUnique<CTexPassElement>(CTexPassElement::SRenderData{.tex = texture}));
    texture.reset();
    ASSERT_FALSE(retained.expired());

    renderer().abortRender();
    EXPECT_FALSE(context.active());
    EXPECT_TRUE(retained.expired());
    renderer().abortRender();
    EXPECT_FALSE(context.active());
    ASSERT_TRUE(context.begin());
    renderer().abortRender();
}

class CLifecycleFramebuffer : public Render::IFramebuffer {
  public:
    bool m_failAllocation = false;

    void release() override {
        m_tex.reset();
        m_fbAllocated = false;
    }
    bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
        ADD_FAILURE() << "Unexpected readback";
        return false;
    }
    void bind() override {
        ;
    }
    void addStencil(SP<Render::ITexture> tex) override {
        m_stencilTex = tex;
    }

  private:
    bool internalAlloc(int w, int h, DRMFormat) override {
        if (m_failAllocation)
            return false;
        m_tex         = makeShared<CDMABUFTestTexture>(true);
        m_tex->m_size = {w, h};
        return true;
    }
};

class CLifecycleBuffer : public IHLBuffer {
  public:
    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return Aquamarine::BUFFER_TYPE_MISC;
    }
    void update(const CRegion&) override {
        ADD_FAILURE() << "Unexpected buffer update";
    }
    bool isSynchronous() override {
        return true;
    }
    bool good() override {
        return true;
    }
};

class CLifecycleOutput : public Aquamarine::IOutput {
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

// Use the real three-buffer rotation to observe missing/double rollback without
// a DRM allocator or a production seam for intercepting CSwapchain::rollback.
class CLifecycleAllocator : public Aquamarine::IAllocator {
  public:
    std::vector<SP<Aquamarine::IBuffer>> m_buffers;

    SP<Aquamarine::IBuffer>              acquire(const Aquamarine::SAllocatorBufferParams&, SP<Aquamarine::CSwapchain>) override {
        return m_buffers.emplace_back(makeShared<CLifecycleBuffer>());
    }
    SP<Aquamarine::CBackend> getBackend() override {
        return g_pCompositor->m_aqBackend;
    }
    int drmFD() override {
        return -1;
    }
    Aquamarine::eAllocatorType type() override {
        return Aquamarine::AQ_ALLOCATOR_TYPE_DRM_DUMB;
    }
};

class CLifecycleRenderbuffer : public Render::IRenderbuffer {
  public:
    CLifecycleRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format, int& unbinds) : IRenderbuffer(buffer, format), m_unbinds(unbinds) {
        m_framebuffer = makeShared<CLifecycleFramebuffer>();
        m_good        = true;
    }
    void bind() override {
        ;
    }
    void unbind() override {
        ++m_unbinds;
    }

  private:
    int& m_unbinds;
};

class CLifecycleRenderer : public CDMABUFTestRenderer {
  public:
    using IHyprRenderer::beginRender;

    enum eFailure {
        NONE,
        INIT_BUFFER,
        BEGIN_FALSE,
        BEGIN_THROW,
        FULL_FAKE_FALSE,
        FULL_FAKE_THROW,
    };
    eFailure                                m_failure = NONE;
    int                                     m_inits = 0, m_begins = 0, m_fakeBegins = 0, m_unbinds = 0;
    WP<Render::IRenderbuffer>               m_renderbuffer;
    std::array<WP<Render::IFramebuffer>, 3> m_targets;
    WP<Render::ITexture>                    m_passTexture;

    SP<Render::IFramebuffer>                createFB(const std::string&) override {
        return makeShared<CLifecycleFramebuffer>();
    }

  private:
    bool initRenderBuffer(Render::CRenderContext& ctx, SP<Aquamarine::IBuffer> buffer, uint32_t format) override {
        ++m_inits;
        EXPECT_TRUE(ctx.active());
        EXPECT_EQ(format, DRM_FORMAT_XRGB8888);
        ctx.m_currentRenderbuffer = makeShared<CLifecycleRenderbuffer>(buffer, format, m_unbinds);
        m_renderbuffer            = ctx.m_currentRenderbuffer;
        if (m_failure != INIT_BUFFER)
            return true;
        // Fail with partial state retained so the production cleanup must release it.
        retainPassState(ctx, CRegion{1, 2, 3, 4});
        return false;
    }
    bool beginRenderInternal(Render::CRenderContext& ctx, PHLMONITOR, CRegion& damage, bool) override {
        ++m_begins;
        retainPassState(ctx, damage);
        if (m_failure == BEGIN_THROW)
            throw std::runtime_error("Injected render begin exception");
        return m_failure != BEGIN_FALSE;
    }
    bool beginFullFakeRenderInternal(Render::CRenderContext& ctx, PHLMONITOR, CRegion& damage, SP<Render::IFramebuffer> fb, bool simple) override {
        ++m_fakeBegins;
        EXPECT_TRUE(simple);
        EXPECT_EQ(ctx.m_data.fbSize, fb->m_size);
        EXPECT_FALSE(ctx.m_currentBuffer);
        EXPECT_FALSE(ctx.m_swapchainAcquired);
        retainPassState(ctx, damage);
        ctx.m_data.outFB = fb;
        m_targets[2]     = fb;
        if (m_failure == FULL_FAKE_THROW)
            throw std::runtime_error("Injected full-fake begin exception");
        return m_failure != FULL_FAKE_FALSE;
    }
    void retainPassState(Render::CRenderContext& ctx, const CRegion& damage) {
        EXPECT_TRUE(ctx.active());
        EXPECT_TRUE(ctx.m_data.damage.empty());
        EXPECT_TRUE(ctx.m_data.finalDamage.empty());
        ctx.m_data.currentFB = makeShared<CLifecycleFramebuffer>();
        ctx.m_data.mainFB    = makeShared<CLifecycleFramebuffer>();
        ctx.m_data.outFB     = makeShared<CLifecycleFramebuffer>();
        m_targets            = {ctx.m_data.currentFB, ctx.m_data.mainFB, ctx.m_data.outFB};
        auto texture         = makeShared<CDMABUFTestTexture>(true);
        m_passTexture        = texture;
        ctx.m_pass.add(makeUnique<CTexPassElement>(CTexPassElement::SRenderData{.tex = texture}));
        setDamage(ctx, damage, std::nullopt);
    }
};

class CRenderLifecycleTest : public CDMABUFBufferTest {
  protected:
    void SetUp() override {
        CDMABUFBufferTest::SetUp();
        if (HasFatalFailure())
            return;
        m_previousWatcher = std::move(Config::watcher());
        Config::watcher() = makeUnique<Config::CConfigWatcher>();
        m_previousConfig  = std::move(Config::mgr());
        Config::mgr()     = makeUnique<Config::Lua::CConfigManager>();
        CConfigValueBase::flushCaches();
        m_previousAnimations    = std::move(Animation::mgr());
        m_previousTree          = std::move(Config::animationTree());
        Animation::mgr()        = makeUnique<Animation::CHyprAnimationManager>();
        Config::animationTree() = makeUnique<Config::CAnimationTreeController>();
        m_installedAnimations   = true;
        auto output             = makeShared<CLifecycleOutput>();
        output->name            = "render-lifecycle-test";
        output->state->setFormat(DRM_FORMAT_XRGB8888);
        m_allocator       = makeShared<CLifecycleAllocator>();
        output->swapchain = Aquamarine::CSwapchain::create(m_allocator, nullptr);
        ASSERT_TRUE(output->swapchain->reconfigure({.length = 3, .size = {64, 48}, .format = DRM_FORMAT_XRGB8888}));
        m_monitor         = makeShared<Monitor::CMonitor>(output);
        m_monitor->m_self = m_monitor;
        m_monitor->m_size = m_monitor->m_pixelSize = m_monitor->m_transformedSize = {64, 48};
    }
    void TearDown() override {
        if (m_installedAnimations) {
            renderer().abortRender();
            m_monitor.reset();
            m_allocator.reset();
            Animation::mgr()        = std::move(m_previousAnimations);
            Config::animationTree() = std::move(m_previousTree);
            Config::mgr()           = std::move(m_previousConfig);
            Config::watcher()       = std::move(m_previousWatcher);
            if (Config::mgr())
                CConfigValueBase::flushCaches();
        }
        CDMABUFBufferTest::TearDown();
    }
    UP<Render::IHyprRenderer> makeRenderer() override {
        return makeUnique<CLifecycleRenderer>();
    }
    CLifecycleRenderer& renderer() {
        return sc<CLifecycleRenderer&>(*g_pHyprRenderer);
    }
    void expectClean() {
        auto& ctx = renderer().context();
        EXPECT_FALSE(ctx.active());
        EXPECT_FALSE(ctx.m_currentBuffer);
        EXPECT_FALSE(ctx.m_currentRenderbuffer);
        EXPECT_FALSE(ctx.m_swapchainAcquired);
        EXPECT_FALSE(ctx.m_data.pMonitor);
        EXPECT_FALSE(ctx.m_data.currentFB);
        EXPECT_FALSE(ctx.m_data.mainFB);
        EXPECT_FALSE(ctx.m_data.outFB);
        EXPECT_TRUE(ctx.m_data.damage.empty());
        EXPECT_TRUE(ctx.m_data.finalDamage.empty());
        EXPECT_TRUE(renderer().m_renderbuffer.expired());
        EXPECT_TRUE(renderer().m_passTexture.expired());
        for (const auto& target : renderer().m_targets)
            EXPECT_TRUE(target.expired());
    }

    PHLMONITOR              m_monitor;
    SP<CLifecycleAllocator> m_allocator;

  private:
    bool                                 m_installedAnimations = false;
    UP<Animation::CHyprAnimationManager> m_previousAnimations;
    UP<Config::CAnimationTreeController> m_previousTree;
    UP<Config::IConfigManager>           m_previousConfig;
    UP<Config::CConfigWatcher>           m_previousWatcher;
};

TEST_F(CRenderLifecycleTest, IsolatedBlurSelectionNeverReadsOrCompletesMonitorBlur) {
    const auto monitorResources = m_monitor->resources();
    const auto monitorTexture   = monitorResources->m_blurFB->getTexture();
    ASSERT_TRUE(monitorTexture);
    m_monitor->m_blurFBDirty        = true;
    m_monitor->m_blurFBShouldRender = true;
    auto  framebuffer               = makeShared<CLifecycleFramebuffer>();
    auto  resources                 = makeShared<Render::CSceneResources>(framebuffer);
    auto& ctx                       = renderer().context();
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;

    EXPECT_FALSE(renderer().getBlurTexture(ctx));
    ASSERT_TRUE(resources->prepare({64, 48}, DRM_FORMAT_XRGB8888, nullptr));
    EXPECT_FALSE(renderer().getBlurTexture(ctx));
    resources->setBlurQueued(true);
    resources->completePreBlur();
    EXPECT_EQ(renderer().getBlurTexture(ctx), framebuffer->getTexture());
    EXPECT_NE(renderer().getBlurTexture(ctx), monitorTexture);
    EXPECT_FALSE(resources->blurDirty());
    EXPECT_FALSE(resources->blurQueued());
    EXPECT_TRUE(m_monitor->m_blurFBDirty);
    EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
    resources->setBlurDirty(true);
    EXPECT_FALSE(renderer().getBlurTexture(ctx));
    EXPECT_EQ(monitorResources->m_blurFB->getTexture(), monitorTexture);
}

TEST_F(CRenderLifecycleTest, IsolatedDrawScopesDoNotWriteBackMonitorFlags) {
    auto  resources = makeShared<Render::CSceneResources>(makeShared<CLifecycleFramebuffer>());
    auto& ctx       = renderer().context();
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;
    for (const bool initial : {false, true}) {
        SCOPED_TRACE(initial);
        m_monitor->m_blurFBDirty        = initial;
        m_monitor->m_blurFBShouldRender = initial;
        resources->setBlurQueued(initial);
        {
            auto outer = ctx.saveDrawState();
            resources->setBlurQueued(!initial);
            {
                auto inner = ctx.saveDrawState();
                resources->setBlurQueued(initial);
                m_monitor->m_blurFBDirty        = !initial;
                m_monitor->m_blurFBShouldRender = !initial;
            }
            EXPECT_EQ(resources->blurQueued(), !initial);
            EXPECT_EQ(m_monitor->m_blurFBDirty, !initial);
            EXPECT_EQ(m_monitor->m_blurFBShouldRender, !initial);
        }
        EXPECT_EQ(resources->blurQueued(), initial);
        EXPECT_EQ(m_monitor->m_blurFBDirty, !initial);
        EXPECT_EQ(m_monitor->m_blurFBShouldRender, !initial);
    }
}

TEST_F(CRenderLifecycleTest, DefaultRenderBeginSelectsActualMonitorResourceAdapter) {
    const auto monitorResources = m_monitor->resources();
    CRegion    damage{0, 0, 64, 48};
    ASSERT_TRUE(renderer().beginRender(m_monitor, damage, Render::RENDER_MODE_TO_BUFFER, makeShared<CLifecycleBuffer>()));
    auto& ctx = renderer().context();
    ASSERT_EQ(ctx.sceneResources(), monitorResources->sceneResources());
    ASSERT_TRUE(ctx.sceneResources());
    EXPECT_FALSE(ctx.sceneResources()->isolated());
    EXPECT_EQ(ctx.sceneResources()->blurFramebuffer(), monitorResources->m_blurFB);
    EXPECT_EQ(renderer().getBlurTexture(ctx), monitorResources->m_blurFB->getTexture());
    m_monitor->m_blurFBDirty        = true;
    m_monitor->m_blurFBShouldRender = true;
    {
        auto scope = ctx.saveDrawState();
        ctx.sceneResources()->completePreBlur();
        EXPECT_FALSE(m_monitor->m_blurFBDirty);
        EXPECT_FALSE(m_monitor->m_blurFBShouldRender);
    }
    EXPECT_FALSE(m_monitor->m_blurFBDirty);
    EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
}

TEST_F(CRenderLifecycleTest, UnselectedContextRetainsLegacyMonitorBlurAndQueueRestoration) {
    const auto monitorResources = m_monitor->resources();
    auto&      ctx              = renderer().context();
    ASSERT_TRUE(ctx.begin());
    EXPECT_FALSE(renderer().getBlurTexture(ctx));
    ctx.m_data.pMonitor = m_monitor;
    EXPECT_EQ(renderer().getBlurTexture(ctx), monitorResources->m_blurFB->getTexture());
    for (const bool queued : {false, true}) {
        SCOPED_TRACE(queued);
        m_monitor->m_blurFBDirty        = true;
        m_monitor->m_blurFBShouldRender = queued;
        EXPECT_THROW(
            {
                auto scope                      = ctx.saveDrawState();
                m_monitor->m_blurFBShouldRender = !queued;
                m_monitor->m_blurFBDirty        = false;
                ctx.m_data.pMonitor.reset();
                throw std::runtime_error("Injected legacy draw exception");
            },
            std::runtime_error);
        EXPECT_EQ(ctx.m_data.pMonitor, m_monitor);
        EXPECT_EQ(m_monitor->m_blurFBShouldRender, queued);
        EXPECT_FALSE(m_monitor->m_blurFBDirty);
    }
}

TEST_F(CRenderLifecycleTest, IsolatedPreBlurMarkerUsesPreparedDirtyAllocationBeforeQueueOrContents) {
    CConfigValue<Config::BOOL>          enabled{"decoration:blur:enabled"};
    CConfigValue<Config::BOOL>          optimized{"decoration:blur:new_optimizations"};
    CConfigValue<Config::BOOL>          xray{"decoration:blur:xray"};
    const auto                          oldEnabled = *enabled, oldOptimized = *optimized, oldXray = *xray;
    const Hyprutils::Utils::CScopeGuard restoreConfig{[&] {
        *enabled.ptr()   = oldEnabled;
        *optimized.ptr() = oldOptimized;
        *xray.ptr()      = oldXray;
    }};
    *enabled.ptr()   = true;
    *optimized.ptr() = true;
    *xray.ptr()      = true;
    ASSERT_TRUE(m_monitor->resources()->m_blurFB->getTexture());
    m_monitor->m_blurFBDirty        = true;
    m_monitor->m_blurFBShouldRender = true;
    Render::CRenderContext legacy;
    ASSERT_TRUE(legacy.begin());
    legacy.m_data.pMonitor = m_monitor;
    auto  resources        = makeShared<Render::CSceneResources>(makeShared<CLifecycleFramebuffer>());
    auto& ctx              = renderer().context();
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;

    EXPECT_TRUE(renderer().preBlurQueued(legacy));
    EXPECT_FALSE(renderer().preBlurQueued(ctx));
    EXPECT_FALSE(renderer().shouldUseNewBlurOptimizations(ctx, nullptr, nullptr));
    ASSERT_TRUE(resources->prepare({64, 48}, DRM_FORMAT_XRGB8888, nullptr));
    EXPECT_FALSE(resources->blurQueued());
    EXPECT_FALSE(renderer().getBlurTexture(ctx));
    EXPECT_TRUE(renderer().preBlurQueued(ctx));
    EXPECT_TRUE(renderer().shouldUseNewBlurOptimizations(ctx, nullptr, nullptr));

    *optimized.ptr() = false;
    EXPECT_FALSE(renderer().preBlurQueued(legacy));
    EXPECT_TRUE(renderer().preBlurQueued(ctx));
    *optimized.ptr()                = true;
    m_monitor->m_blurFBShouldRender = false;
    EXPECT_FALSE(renderer().preBlurQueued(legacy));
    EXPECT_TRUE(renderer().preBlurQueued(ctx));
    m_monitor->m_blurFBShouldRender = true;
    m_monitor->m_blurFBDirty        = false;
    EXPECT_FALSE(renderer().preBlurQueued(legacy));
    EXPECT_TRUE(renderer().preBlurQueued(ctx));
    m_monitor->m_blurFBDirty = true;
    *enabled.ptr()           = false;
    EXPECT_FALSE(renderer().preBlurQueued(legacy));
    EXPECT_FALSE(renderer().preBlurQueued(ctx));
    *enabled.ptr() = true;

    resources->completePreBlur();
    resources->setBlurQueued(true);
    EXPECT_FALSE(renderer().preBlurQueued(ctx));
    resources->setBlurDirty(true);
    EXPECT_TRUE(renderer().preBlurQueued(ctx));
    EXPECT_FALSE(resources->prepare({0, 48}, DRM_FORMAT_XRGB8888, nullptr));
    EXPECT_FALSE(renderer().preBlurQueued(ctx));
    EXPECT_FALSE(renderer().shouldUseNewBlurOptimizations(ctx, nullptr, nullptr));
    EXPECT_TRUE(renderer().preBlurQueued(legacy));
}

TEST_F(CRenderLifecycleTest, MissingIsolatedBlurSourceNeverFallsBackToMonitorFramebuffer) {
    const auto monitorFramebuffer = m_monitor->resources()->m_blurFB;
    ASSERT_TRUE(monitorFramebuffer->getTexture());
    m_monitor->m_blurFBDirty        = true;
    m_monitor->m_blurFBShouldRender = true;
    auto  framebuffer               = makeShared<CLifecycleFramebuffer>();
    auto  resources                 = makeShared<Render::CSceneResources>(framebuffer);
    auto& ctx                       = renderer().context();
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;
    ASSERT_FALSE(ctx.m_data.currentFB);
    const CRegion damage{0, 0, 64, 48};
    EXPECT_FALSE(renderer().blurMainFramebuffer(ctx, 1.F, damage));
    ASSERT_TRUE(resources->prepare({64, 48}, DRM_FORMAT_XRGB8888, nullptr));
    resources->setBlurQueued(true);
    ASSERT_FALSE(renderer().blurMainFramebuffer(ctx, 1.F, damage));
    renderer().preBlurForCurrentMonitor(ctx, damage);
    EXPECT_TRUE(resources->blurDirty());
    EXPECT_TRUE(resources->blurQueued());
    EXPECT_FALSE(resources->blurTexture());
    EXPECT_TRUE(m_monitor->m_blurFBDirty);
    EXPECT_TRUE(m_monitor->m_blurFBShouldRender);

    resources->completePreBlur();
    EXPECT_EQ(renderer().blurMainFramebuffer(ctx, 1.F, damage), framebuffer);
    EXPECT_NE(framebuffer, monitorFramebuffer);
    resources->setBlurDirty(true);
    EXPECT_FALSE(renderer().blurMainFramebuffer(ctx, 1.F, damage));
}

TEST_F(CRenderLifecycleTest, IsolatedBeginUsesFullTargetDamageAndPreservesMonitorResources) {
    const auto monitorResources  = m_monitor->resources();
    const auto mirrorFramebuffer = monitorResources->mirrorFB();
    const auto mirrorTexture     = mirrorFramebuffer->getTexture();
    ASSERT_TRUE(mirrorTexture);
    monitorResources->m_mirrorTex = makeShared<CDMABUFTestTexture>(true);
    const auto mirrorAttachment   = monitorResources->m_mirrorTex;
    const auto monitorBlurTexture = monitorResources->m_blurFB->getTexture();
    m_monitor->m_damage.setSize(m_monitor->m_transformedSize);
    m_monitor->m_damage.rotate();
    m_monitor->m_damage.damage(CBox{1, 2, 3, 4});
    auto outputBuffer = makeShared<CLifecycleBuffer>();
    m_monitor->m_output->state->setBuffer(outputBuffer);

    for (const auto mode : {Render::RENDER_MODE_FULL_FAKE, Render::RENDER_MODE_TO_BUFFER, Render::RENDER_MODE_TO_BUFFER_READ_ONLY}) {
        SCOPED_TRACE(mode);
        for (const bool stale : {false, true}) {
            SCOPED_TRACE(stale);
            monitorResources->markMirrorFBUpdated();
            if (stale)
                monitorResources->markMirrorFBStale(CRegion{5, 6, 7, 8});
            const auto mirrorDamage           = monitorResources->pendingMirrorFBDamage();
            m_monitor->m_blurFBDirty          = !stale;
            m_monitor->m_blurFBShouldRender   = stale;
            const auto expectMonitorUnchanged = [&] {
                EXPECT_TRUE(monitorResources->hasMirrorFB());
                EXPECT_TRUE(mirrorFramebuffer->isAllocated());
                EXPECT_EQ(mirrorFramebuffer->getTexture(), mirrorTexture);
                EXPECT_EQ(monitorResources->m_mirrorTex, mirrorAttachment);
                EXPECT_TRUE(monitorResources->pendingMirrorFBDamage().subtract(mirrorDamage).empty());
                EXPECT_TRUE(mirrorDamage.copy().subtract(monitorResources->pendingMirrorFBDamage()).empty());
                EXPECT_EQ(monitorResources->m_blurFB->getTexture(), monitorBlurTexture);
                EXPECT_EQ(m_monitor->m_blurFBDirty, !stale);
                EXPECT_EQ(m_monitor->m_blurFBShouldRender, stale);
                EXPECT_EQ(m_monitor->m_damage.getBufferDamage(1).getExtents(), CBox(1, 2, 3, 4));
                EXPECT_EQ(m_monitor->m_output->state->state().buffer, outputBuffer);
            };

            auto privateFramebuffer = makeShared<CLifecycleFramebuffer>();
            auto resources          = makeShared<Render::CSceneResources>(privateFramebuffer);
            ASSERT_TRUE(resources->prepare({16, 12}, DRM_FORMAT_XRGB8888, nullptr));
            resources->completePreBlur();
            resources->setBlurQueued(true);
            auto target = mode == Render::RENDER_MODE_FULL_FAKE ? makeShared<CLifecycleFramebuffer>() : nullptr;
            auto buffer = mode == Render::RENDER_MODE_FULL_FAKE ? nullptr : makeShared<CLifecycleBuffer>();
            ASSERT_TRUE(!target || target->alloc(stale ? 96 : 16, stale ? 72 : 12));
            const CBox                   targetDamage{{}, target ? target->m_size : m_monitor->m_transformedSize};
            const Render::SRenderOptions options{.sceneResources = resources};
            CRegion                      damage{2, 3, 4, 5};
            ASSERT_TRUE(renderer().beginRender(m_monitor, damage, mode, buffer, target, !!target, options));
            auto& ctx = renderer().context();
            EXPECT_EQ(ctx.sceneResources(), resources);
            EXPECT_TRUE(ctx.readOnlyEffects());
            EXPECT_EQ(ctx.m_mode, mode);
            EXPECT_FALSE(ctx.m_swapchainAcquired);
            EXPECT_EQ(ctx.m_currentBuffer, buffer);
            EXPECT_EQ(damage.getExtents(), targetDamage);
            EXPECT_EQ(ctx.m_data.damage.getExtents(), targetDamage);
            EXPECT_EQ(ctx.m_data.finalDamage.getExtents(), targetDamage);
            EXPECT_EQ(privateFramebuffer->m_size, m_monitor->m_transformedSize);
            EXPECT_EQ(privateFramebuffer->m_drmFormat, monitorResources->m_blurFB->m_drmFormat);
            EXPECT_EQ(privateFramebuffer->imageDescription(), monitorResources->m_blurFB->imageDescription());
            EXPECT_TRUE(resources->canPrecomputeBlur());
            EXPECT_TRUE(resources->blurDirty());
            EXPECT_FALSE(resources->blurQueued());
            EXPECT_FALSE(resources->blurTexture());
            expectMonitorUnchanged();

            target.reset();
            renderer().abortRender();
            expectClean();
            EXPECT_FALSE(ctx.sceneResources());
            expectMonitorUnchanged();
        }
    }
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);
}

TEST_F(CRenderLifecycleTest, IsolatedBeginRejectsUnsafeModesAndMissingExternalDestinations) {
    const auto monitorResources  = m_monitor->resources();
    const auto mirrorFramebuffer = monitorResources->mirrorFB();
    const auto mirrorTexture     = mirrorFramebuffer->getTexture();
    monitorResources->markMirrorFBUpdated();
    monitorResources->markMirrorFBStale(CRegion{5, 6, 7, 8});
    m_monitor->m_blurFBDirty        = false;
    m_monitor->m_blurFBShouldRender = true;
    auto resources                  = makeShared<Render::CSceneResources>(makeShared<CLifecycleFramebuffer>());
    ASSERT_TRUE(resources->prepare({16, 12}, DRM_FORMAT_XRGB8888, nullptr));
    resources->completePreBlur();
    resources->setBlurQueued(true);
    const auto                   privateTexture = resources->blurTexture();
    const Render::SRenderOptions options{.sceneResources = resources};
    auto                         target = makeShared<CLifecycleFramebuffer>();
    ASSERT_TRUE(target->alloc(16, 12));
    auto buffer = makeShared<CLifecycleBuffer>();

    for (const auto mode : {Render::RENDER_MODE_NORMAL, Render::RENDER_MODE_FULL_FAKE, Render::RENDER_MODE_TO_BUFFER, Render::RENDER_MODE_TO_BUFFER_READ_ONLY}) {
        SCOPED_TRACE(mode);
        CRegion damage{2, 3, 4, 5};
        // NORMAL is forbidden even with both destinations. Other modes receive
        // only the wrong destination type, so they must not acquire a swapchain.
        EXPECT_FALSE(renderer().beginRender(m_monitor, damage, mode, mode == Render::RENDER_MODE_NORMAL || mode == Render::RENDER_MODE_FULL_FAKE ? buffer : nullptr,
                                            mode == Render::RENDER_MODE_FULL_FAKE ? nullptr : target, false, options));
        expectClean();
        EXPECT_FALSE(renderer().context().sceneResources());
        EXPECT_EQ(damage.getExtents(), CBox(2, 3, 4, 5));
        EXPECT_EQ(resources->blurTexture(), privateTexture);
        EXPECT_FALSE(resources->blurDirty());
        EXPECT_TRUE(resources->blurQueued());
        EXPECT_TRUE(mirrorFramebuffer->isAllocated());
        EXPECT_EQ(mirrorFramebuffer->getTexture(), mirrorTexture);
        EXPECT_EQ(monitorResources->pendingMirrorFBDamage().getExtents(), CBox(5, 6, 7, 8));
        EXPECT_FALSE(m_monitor->m_blurFBDirty);
        EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
    }
    EXPECT_EQ(renderer().m_inits, 0);
    EXPECT_EQ(renderer().m_begins, 0);
    EXPECT_EQ(renderer().m_fakeBegins, 0);
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);
}

TEST_F(CRenderLifecycleTest, FailedIsolatedPrepareCleansContextWithoutChangingMonitorState) {
    const auto monitorResources  = m_monitor->resources();
    const auto mirrorFramebuffer = monitorResources->mirrorFB();
    const auto mirrorTexture     = mirrorFramebuffer->getTexture();
    const auto blurTexture       = monitorResources->m_blurFB->getTexture();
    monitorResources->markMirrorFBUpdated();
    monitorResources->markMirrorFBStale(CRegion{5, 6, 7, 8});
    m_monitor->m_blurFBDirty        = false;
    m_monitor->m_blurFBShouldRender = true;
    m_monitor->m_damage.setSize(m_monitor->m_transformedSize);
    m_monitor->m_damage.rotate();
    auto privateFramebuffer = makeShared<CLifecycleFramebuffer>();
    auto resources          = makeShared<Render::CSceneResources>(privateFramebuffer);
    ASSERT_TRUE(resources->prepare({16, 12}, DRM_FORMAT_XRGB8888, nullptr));
    resources->completePreBlur();
    privateFramebuffer->m_failAllocation = true;
    const Render::SRenderOptions options{.sceneResources = resources};
    auto                         target = makeShared<CLifecycleFramebuffer>();
    ASSERT_TRUE(target->alloc(16, 12));
    CRegion damage{2, 3, 4, 5};

    EXPECT_FALSE(renderer().beginRender(m_monitor, damage, Render::RENDER_MODE_FULL_FAKE, nullptr, target, true, options));
    expectClean();
    EXPECT_FALSE(renderer().context().sceneResources());
    EXPECT_FALSE(resources->canPrecomputeBlur());
    EXPECT_FALSE(resources->blurTexture());
    EXPECT_TRUE(resources->blurDirty());
    EXPECT_EQ(damage.getExtents(), CBox(2, 3, 4, 5));
    EXPECT_TRUE(mirrorFramebuffer->isAllocated());
    EXPECT_EQ(mirrorFramebuffer->getTexture(), mirrorTexture);
    EXPECT_EQ(monitorResources->pendingMirrorFBDamage().getExtents(), CBox(5, 6, 7, 8));
    EXPECT_EQ(monitorResources->m_blurFB->getTexture(), blurTexture);
    EXPECT_FALSE(m_monitor->m_blurFBDirty);
    EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
    EXPECT_FALSE(m_monitor->m_damage.hasChanged());
    EXPECT_EQ(renderer().m_inits, 0);
    EXPECT_EQ(renderer().m_fakeBegins, 0);
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);
}

TEST_F(CRenderLifecycleTest, WorkBufferRoutingDetachesOnlyIsolatedBorrowersFromActualPools) {
    const auto monitorResources   = m_monitor->resources();
    monitorResources->m_mirrorTex = makeShared<CDMABUFTestTexture>(true);
    const auto mirror             = monitorResources->m_mirrorTex;
    auto       resources          = makeShared<Render::CSceneResources>(makeShared<CLifecycleFramebuffer>());
    ASSERT_TRUE(resources->prepare({64, 48}, DRM_FORMAT_XRGB8888, nullptr));
    resources->completePreBlur();
    resources->setBlurQueued(true);
    const auto privateTexture       = resources->blurTexture();
    m_monitor->m_blurFBDirty        = false;
    m_monitor->m_blurFBShouldRender = true;
    auto& ctx                       = renderer().context();

    for (const auto size : {std::optional<Vector2D>{}, std::optional<Vector2D>{Vector2D{16, 12}}}) {
        SCOPED_TRACE(size.has_value());
        auto pooled = size ? monitorResources->getUnusedWorkBuffer(*size) : monitorResources->getUnusedWorkBuffer();
        ASSERT_TRUE(pooled);
        WP<Render::IFramebuffer> original = pooled;
        pooled->enableMirror(mirror);
        pooled.reset();
        for (const auto& selected : {SP<Render::CSceneResources>{}, monitorResources->sceneResources(), resources}) {
            ASSERT_TRUE(ctx.begin(selected));
            ctx.m_data.pMonitor = m_monitor;
            auto work           = renderer().getWorkBuffer(ctx, size);
            ASSERT_TRUE(work);
            EXPECT_EQ(work, original.lock());
            EXPECT_EQ(work->m_size, size.value_or(m_monitor->m_transformedSize));
            EXPECT_TRUE(work->isAllocated());
            EXPECT_EQ(work->getMirrorTexture(), selected == resources ? nullptr : mirror);
            EXPECT_EQ(monitorResources->m_mirrorTex, mirror);
            EXPECT_EQ(resources->blurTexture(), privateTexture);
            EXPECT_FALSE(resources->blurDirty());
            EXPECT_TRUE(resources->blurQueued());
            EXPECT_FALSE(m_monitor->m_blurFBDirty);
            EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
            ctx.reset();
        }
    }
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;
    EXPECT_FALSE(renderer().getWorkBuffer(ctx, Vector2D{0, 12}));
}

TEST_F(CRenderLifecycleTest, IsolatedAnimatedBlurNeverDamagesMonitorEvenInNormalMode) {
    auto  resources = makeShared<Render::CSceneResources>(makeShared<CLifecycleFramebuffer>());
    auto& ctx       = renderer().context();
    ASSERT_TRUE(ctx.begin(resources));
    ctx.m_data.pMonitor = m_monitor;
    ctx.m_mode          = Render::RENDER_MODE_NORMAL;
    ASSERT_TRUE(ctx.readOnlyEffects());
    ASSERT_FALSE(m_monitor->isMirror());
    m_monitor->m_damage.setSize(m_monitor->m_transformedSize);
    m_monitor->m_damage.rotate();
    m_monitor->m_blurFBDirty        = false;
    m_monitor->m_blurFBShouldRender = true;
    m_monitor->m_pendingFrame       = false;
    for (const bool precomputed : {false, true}) {
        SCOPED_TRACE(precomputed);
        renderer().scheduleFrameForAnimatedBlur(ctx, CRegion{1, 2, 3, 4}, precomputed);
        EXPECT_FALSE(m_monitor->m_damage.hasChanged());
        EXPECT_TRUE(m_monitor->m_damage.getBufferDamage(1).empty());
        EXPECT_FALSE(m_monitor->m_blurFBDirty);
        EXPECT_TRUE(m_monitor->m_blurFBShouldRender);
        EXPECT_FALSE(m_monitor->m_pendingFrame);
    }
}

TEST_F(CRenderLifecycleTest, MonitorHookRejectsReentryAndUnwindsWhileAllowingOffscreenCapture) {
    struct SStopRender {};
    auto& r                        = renderer();
    g_pCompositor->m_sessionActive = true;
    m_monitor->m_scheduledRecalc   = true;
    m_monitor->m_pendingFrame      = true;
    m_monitor->m_damage.setSize(m_monitor->m_pixelSize);
    m_monitor->m_damage.damage(CBox{1, 2, 3, 4});
    const auto damage = m_monitor->m_damage.getBufferDamage(1);
    auto       buffer = makeShared<CLifecycleBuffer>();
    m_monitor->m_output->state->setBuffer(buffer);

    int  callbacks = 0, expectedCallbacks = 0;
    auto listener = Event::bus()->m_events.render.preChecks.listen([&](PHLMONITOR monitor) {
        ++callbacks;
        // Bound recursion if the production guard regresses, before reaching GPU code.
        if (callbacks != expectedCallbacks)
            throw SStopRender{};
        EXPECT_EQ(monitor, m_monitor);
        EXPECT_FALSE(r.context().active());
        EXPECT_FALSE(monitor->m_renderingActive);
        EXPECT_NO_THROW(r.renderMonitor(monitor));
        EXPECT_EQ(callbacks, expectedCallbacks);
        expectClean();
        EXPECT_TRUE(monitor->m_scheduledRecalc);
        EXPECT_TRUE(monitor->m_pendingFrame);
        EXPECT_FALSE(monitor->m_renderingActive);
        EXPECT_EQ(monitor->m_damage.getBufferDamage(1).getExtents(), damage.copy().getExtents());
        EXPECT_EQ(monitor->m_output->state->state().buffer, buffer);
        EXPECT_EQ(r.m_inits, 0);
        EXPECT_EQ(r.m_begins, 0);

        // PRE/POST hooks may still capture offscreen between render sessions.
        auto fb = makeShared<CLifecycleFramebuffer>();
        EXPECT_TRUE(fb->alloc(16, 12));
        CRegion captureDamage{0, 0, 16, 12};
        EXPECT_TRUE(r.beginFullFakeRender(monitor, captureDamage, fb));
        r.abortRender();
        throw SStopRender{};
    });

    for (expectedCallbacks = 1; expectedCallbacks <= 2; ++expectedCallbacks) {
        EXPECT_THROW(r.renderMonitor(m_monitor), SStopRender);
        EXPECT_EQ(callbacks, expectedCallbacks);
        EXPECT_EQ(r.m_fakeBegins, expectedCallbacks);
        expectClean();
    }
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);
}

class CRenderBufferFailureTest : public CRenderLifecycleTest, public ::testing::WithParamInterface<std::tuple<CLifecycleRenderer::eFailure, bool>> {};

TEST_P(CRenderBufferFailureTest, CleansFailedBeginAndCanBeginAgain) {
    const auto [failure, supplied]         = GetParam();
    auto& r                                = renderer();
    r.m_failure                            = failure;
    auto                    buffer         = supplied ? makeShared<CLifecycleBuffer>() : nullptr;
    WP<Aquamarine::IBuffer> suppliedBuffer = buffer;
    CRegion                 damage{1, 2, 3, 4};
    const auto              begin = [&] { return r.beginRender(m_monitor, damage, Render::RENDER_MODE_TO_BUFFER, buffer); };
    if (failure == CLifecycleRenderer::BEGIN_THROW)
        EXPECT_THROW(begin(), std::runtime_error);
    else
        EXPECT_FALSE(begin());

    EXPECT_EQ(r.m_inits, 1);
    EXPECT_EQ(r.m_begins, failure == CLifecycleRenderer::INIT_BUFFER ? 0 : 1);
    EXPECT_EQ(r.m_unbinds, 1);
    EXPECT_EQ(damage.getExtents(), CBox(1, 2, 3, 4));
    expectClean();
    r.abortRender();
    EXPECT_EQ(r.m_unbinds, 1);
    // A supplied buffer must not rotate/rollback the swapchain. An acquired
    // buffer must roll back exactly once, including on exception or repeated abort.
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);
    if (supplied) {
        ASSERT_FALSE(suppliedBuffer.expired());
        buffer.reset();
        EXPECT_TRUE(suppliedBuffer.expired());
        buffer = makeShared<CLifecycleBuffer>();
    }

    r.m_failure = CLifecycleRenderer::NONE;
    damage      = CRegion{10, 11, 12, 13};
    const Render::SRenderOptions options{.mouseZoomFactor = 2.5F, .mouseZoomUseMouse = false, .useNearestNeighbor = true};
    ASSERT_TRUE(r.beginRender(m_monitor, damage, Render::RENDER_MODE_TO_BUFFER, buffer, nullptr, false, options));
    auto& ctx = r.context();
    EXPECT_TRUE(ctx.active());
    EXPECT_EQ(ctx.m_currentBuffer, supplied ? buffer : m_allocator->m_buffers[2]);
    EXPECT_EQ(ctx.m_swapchainAcquired, !supplied);
    EXPECT_EQ(ctx.m_mode, Render::RENDER_MODE_TO_BUFFER);
    EXPECT_FLOAT_EQ(ctx.m_data.mouseZoomFactor, options.mouseZoomFactor);
    EXPECT_EQ(ctx.m_data.mouseZoomUseMouse, options.mouseZoomUseMouse);
    EXPECT_EQ(ctx.m_data.useNearestNeighbor, options.useNearestNeighbor);
    EXPECT_EQ(ctx.m_data.damage.getExtents(), CBox(10, 11, 12, 13));
    EXPECT_EQ(ctx.m_data.finalDamage.getExtents(), CBox(10, 11, 12, 13));
    EXPECT_FALSE(r.m_passTexture.expired());
    r.abortRender();
    EXPECT_EQ(r.m_unbinds, 2);
    expectClean();
}

INSTANTIATE_TEST_SUITE_P(BeginFailures, CRenderBufferFailureTest,
                         ::testing::Combine(::testing::Values(CLifecycleRenderer::INIT_BUFFER, CLifecycleRenderer::BEGIN_FALSE, CLifecycleRenderer::BEGIN_THROW),
                                            ::testing::Bool()));

class CFullFakeFailureTest : public CRenderLifecycleTest, public ::testing::WithParamInterface<CLifecycleRenderer::eFailure> {};

TEST_P(CFullFakeFailureTest, CleansEarlyFailureAndCanBeginAgain) {
    auto& r     = renderer();
    r.m_failure = GetParam();
    CRegion damage{1, 2, 3, 4};
    auto    fb = makeShared<CLifecycleFramebuffer>();
    ASSERT_TRUE(fb->alloc(32, 24));
    const auto begin = [&] { return r.beginFullFakeRender(m_monitor, damage, fb); };
    if (GetParam() == CLifecycleRenderer::FULL_FAKE_THROW)
        EXPECT_THROW(begin(), std::runtime_error);
    else
        EXPECT_FALSE(begin());
    fb.reset();
    EXPECT_EQ(r.m_fakeBegins, 1);
    EXPECT_EQ(r.m_inits, 0);
    EXPECT_EQ(r.m_begins, 0);
    EXPECT_EQ(r.m_unbinds, 0);
    expectClean();
    r.abortRender();
    EXPECT_EQ(m_monitor->m_output->swapchain->next(nullptr), m_allocator->m_buffers[1]);

    r.m_failure = CLifecycleRenderer::NONE;
    fb          = makeShared<CLifecycleFramebuffer>();
    ASSERT_TRUE(fb->alloc(16, 12));
    damage = CRegion{5, 6, 7, 8};
    ASSERT_TRUE(begin());
    EXPECT_TRUE(r.context().active());
    EXPECT_EQ(r.context().m_mode, Render::RENDER_MODE_FULL_FAKE);
    EXPECT_EQ(r.context().m_data.outFB, fb);
    EXPECT_EQ(r.context().m_data.damage.getExtents(), CBox(5, 6, 7, 8));
    fb.reset();
    r.abortRender();
    expectClean();
    EXPECT_EQ(r.m_fakeBegins, 2);
    EXPECT_EQ(r.m_inits, 0);
    EXPECT_EQ(r.m_unbinds, 0);
}

INSTANTIATE_TEST_SUITE_P(EarlyFailures, CFullFakeFailureTest, ::testing::Values(CLifecycleRenderer::FULL_FAKE_FALSE, CLifecycleRenderer::FULL_FAKE_THROW));

TEST_F(CDMABUFBufferTest, TransferredPlanesSurviveParamsAndAttributeCopies) {
    std::array<CFileDescriptor, 4> readers;
    for (uint32_t plane = 0; plane < readers.size(); ++plane)
        addPlane(plane, readers[plane]);
    createBuffer(1, 1);
    auto buffer = createdBuffer();
    ASSERT_TRUE(buffer);
    ASSERT_TRUE(buffer->good());
    ASSERT_EQ(renderer().m_imports.size(), 1U);
    ASSERT_TRUE(m_params);
    EXPECT_EQ(m_params->m_attrs.planes, 0);
    for (const auto fd : m_params->m_attrs.fds)
        EXPECT_EQ(fd, -1);

    m_params.reset();
    {
        const auto attrs = buffer->dmabuf();
        EXPECT_EQ(attrs.planes, 4);
        for (const auto fd : attrs.fds)
            EXPECT_NE(fcntl(fd, F_GETFD), -1);
    }
    for (const auto& reader : readers)
        expectOpen(reader);

    // Keep the wl_buffer alive and let final C++ destruction close its planes.
    PROTO::linuxDma.reset();
    for (const auto& reader : readers)
        expectOpen(reader);
    buffer.reset();
    for (const auto& reader : readers)
        expectClosed(reader);
}

TEST_F(CDMABUFBufferTest, ImportFailureClosesTransferredPlanesBeforeParamsDestruction) {
    renderer().m_failImport = true;
    CFileDescriptor first, second;
    addPlane(0, first, 1);
    addPlane(1, second, 1);
    createBuffer(1, 1);
    ASSERT_TRUE(m_params);
    EXPECT_FALSE(createdBuffer());
    ASSERT_EQ(renderer().m_imports.size(), 2U);
    EXPECT_EQ(renderer().m_imports[0].modifier, 1U);
    EXPECT_EQ(renderer().m_imports[1].modifier, DRM_FORMAT_MOD_INVALID);
    expectClosed(first);
    expectClosed(second);

    // Params still exist, but must not close a reused descriptor after transfer.
    const int       fd = renderer().m_imports[0].fds[0];
    CFileDescriptor reused{fcntl(first.get(), F_DUPFD_CLOEXEC, fd)};
    ASSERT_EQ(reused.get(), fd);
    m_params.reset();
    EXPECT_NE(fcntl(reused.get(), F_GETFD), -1);
}

TEST_F(CDMABUFBufferTest, InvalidTextureClosesTransferredPlane) {
    renderer().m_validTexture = false;
    CFileDescriptor reader;
    addPlane(0, reader);
    createBuffer(1, 1);
    ASSERT_TRUE(m_params);
    EXPECT_FALSE(createdBuffer());
    EXPECT_EQ(renderer().m_imports.size(), 1U);
    expectClosed(reader);
}

TEST_F(CDMABUFBufferTest, ResourceDestroyThenDestructorDoesNotCloseReusedFD) {
    CFileDescriptor reader;
    addPlane(0, reader);
    createBuffer(1, 1);
    auto buffer = createdBuffer();
    ASSERT_TRUE(buffer);
    const auto attrs = buffer->dmabuf();
    const int  fd    = attrs.fds[0];
    m_params.reset();
    expectOpen(reader);

    wl_resource_destroy(buffer->m_resource->getResource());
    expectClosed(reader);
    EXPECT_EQ(buffer->dmabuf().planes, 0);
    EXPECT_EQ(buffer->dmabuf().fds[0], -1);

    CFileDescriptor reused{fcntl(reader.get(), F_DUPFD_CLOEXEC, fd)};
    ASSERT_EQ(reused.get(), fd);
    buffer->events.destroy.emit();
    buffer.reset();
    EXPECT_NE(fcntl(reused.get(), F_GETFD), -1);
}

TEST_F(CDMABUFBufferTest, ConstructorUnwindClosesAllMovedFDs) {
    renderer().m_throwOnImport = true;
    std::array<CFileDescriptor, 4> fds;
    fds[0] = CFileDescriptor{open("/dev/null", O_RDONLY | O_CLOEXEC)};
    fds[3] = CFileDescriptor{open("/dev/null", O_RDONLY | O_CLOEXEC)};
    ASSERT_TRUE(fds[0].isValid());
    ASSERT_TRUE(fds[3].isValid());
    const int                first = fds[0].get();
    const int                last  = fds[3].get();
    Aquamarine::SDMABUFAttrs attrs;
    attrs.size   = {1, 1};
    attrs.format = DRM_FORMAT_XRGB8888;
    attrs.planes = 1;
    attrs.fds[0] = first;
    attrs.fds[3] = last;

    // An exception prevents CDMABuffer's destructor from running. Its FD members
    // must still close every slot, independent of the borrowed attrs' plane count.
    EXPECT_THROW(makeShared<CDMABuffer>(3, m_client, attrs, std::move(fds)), std::runtime_error);
    for (const auto& fd : fds)
        EXPECT_FALSE(fd.isValid());
    EXPECT_EQ(fcntl(first, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
    EXPECT_EQ(fcntl(last, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
    EXPECT_EQ(renderer().m_imports.size(), 1U);
}

class CMesaDMABUFRejectedTest : public CDMABUFBufferTest, public ::testing::WithParamInterface<std::array<int32_t, 3>> {};

TEST_P(CMesaDMABUFRejectedTest, ClosesIncomingPrimeFD) {
    auto wire = makeShared<CWlDrm>(m_client, 2, 3);
    auto mesa = makeUnique<CMesaDRMResource>(wire);
    ASSERT_TRUE(mesa->good());
    const auto [width, height, offset] = GetParam();
    std::array<uint32_t, 12> request   = {
        3, (12U * sizeof(uint32_t) << 16) | 3U, 4, sc<uint32_t>(width), sc<uint32_t>(height), DRM_FORMAT_XRGB8888, sc<uint32_t>(offset), 4, 0, 0, 0, 0,
    };
    CFileDescriptor reader;
    sendFDRequest(request, reader);
    expectClosed(reader);
    EXPECT_TRUE(renderer().m_imports.empty());
}

INSTANTIATE_TEST_SUITE_P(InvalidDimensionsAndOffset, CMesaDMABUFRejectedTest,
                         ::testing::Values(std::array<int32_t, 3>{0, 1, 0}, std::array<int32_t, 3>{1, 0, 0}, std::array<int32_t, 3>{1, 1, -1}));
