#include <protocols/LinuxDMABUF.hpp>
#include <protocols/MesaDRM.hpp>
#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <render/Renderer.hpp>
#include <render/SyncFDManager.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
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
    void endRender(const std::function<void()>&) override {
        ;
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
    void drawShadow(const CBox&, int, float, int, const Config::CGradientValueData&, float) override {
        ;
    }
    void drawShadow(const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float) override {
        ;
    }
    void drawGlow(const CBox&, int, float, int, const Config::CGradientValueData&, float) override {
        ;
    }
    void drawGlow(const CBox&, int, float, int, const Config::CGradientValueData&, const Config::CGradientValueData&, float, float) override {
        ;
    }
    void setViewport(int, int, int, int) override {
        ;
    }
    SP<Render::IFramebuffer> blurFramebuffer(SP<Render::IFramebuffer>, float, const CRegion&, const Render::SBlurContext&) override {
        return nullptr;
    }
    void refreshBlurProvider() override {
        ;
    }
    void expandBlurDamage(CRegion&, float) const override {
        ;
    }
    bool blurProviderIsAnimated() const override {
        return false;
    }
    bool blurProviderRequiresLiveBlur() const override {
        return false;
    }
    bool reloadShaders(const std::string&) override {
        return false;
    }
    void renderOffToMain(SP<Render::IFramebuffer>) override {
        ;
    }
    SP<Render::IRenderbuffer> getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer>, uint32_t) override {
        return nullptr;
    }
};

class CDMABUFBufferTest : public CDMABUFParamsTest {
  protected:
    void                 SetUp() override;
    void                 TearDown() override;
    CDMABUFTestRenderer& renderer();
    SP<IHLBuffer>        createdBuffer();

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
    g_pHyprRenderer     = makeUnique<CDMABUFTestRenderer>();
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
