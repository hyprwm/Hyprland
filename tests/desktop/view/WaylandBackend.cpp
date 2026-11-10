#include <Compositor.hpp>
#include <desktop/view/window/WaylandBackend.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/XDGShell.hpp>
#include <protocols/core/Compositor.hpp>

#include <gtest/gtest.h>
#include <hyprutils/os/FileDescriptor.hpp>

#include <array>
#include <cerrno>
#include <initializer_list>
#include <memory>
#include <vector>
#include <sys/socket.h>

using namespace Desktop::View;
using namespace Hyprutils::OS;

// Exercise the backend with real protocol resources on an isolated Wayland server.
class CWaylandBackendTest : public ::testing::Test {
  protected:
    void                SetUp() override;
    void                TearDown() override;
    void                expectConfigure(const Vector2D& size);
    void                expectNoConfigure();
    void                request(uint32_t id, uint32_t opcode, std::initializer_list<uint32_t> args = {});
    void                acknowledge(uint32_t serial, const Vector2D& size);

    UP<CWaylandBackend> m_backend;
    uint32_t            m_lastSerial = 0;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    wl_client*                                                 m_client = nullptr; // Owned by m_display.
    SP<CWLSurfaceResource>                                     m_surface;
    SP<CXDGSurfaceResource>                                    m_xdgSurface;
    SP<CXDGToplevelResource>                                   m_toplevel;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<Event::CEventBus>                                       m_previousEventBus;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<CWLCompositorProtocol>                                  m_previousCoreCompositor;
    UP<CXDGShellProtocol>                                      m_previousXDGShell;
};

void CWaylandBackendTest::SetUp() {
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
    m_previousEventBus       = std::move(Event::bus());
    m_previousPresentation   = std::move(PROTO::presentation);
    m_previousCoreCompositor = std::move(PROTO::compositor);
    m_previousXDGShell       = std::move(PROTO::xdgShell);
    m_installedGlobals       = true;

    Event::bus()               = makeUnique<Event::CEventBus>();
    g_pCompositor              = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay = m_display.get();
    g_pEventLoopManager        = makeUnique<CEventLoopManager>(m_display.get(), wl_display_get_event_loop(m_display.get()));
    PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "wayland-backend-test-presentation");
    PROTO::compositor          = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "wayland-backend-test-compositor");
    PROTO::xdgShell            = makeUnique<CXDGShellProtocol>(&xdg_wm_base_interface, 1, "wayland-backend-test-xdg-shell");

    // Construct the role directly to avoid creating a compositor window or renderer.
    m_surface                = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, 2));
    m_surface->m_self        = m_surface;
    m_surface->m_stateQueue  = CSurfaceStateQueue{m_surface};
    m_xdgSurface             = makeShared<CXDGSurfaceResource>(makeShared<CXdgSurface>(m_client, 1, 3), nullptr, m_surface);
    m_xdgSurface->m_self     = m_xdgSurface;
    m_surface->m_role        = makeShared<CXDGSurfaceRole>(m_xdgSurface);
    m_toplevel               = makeShared<CXDGToplevelResource>(makeShared<CXdgToplevel>(m_client, 1, 4), m_xdgSurface);
    m_toplevel->m_self       = m_toplevel;
    m_xdgSurface->m_toplevel = m_toplevel;
    m_backend                = makeUnique<CWaylandBackend>(m_xdgSurface);
    ASSERT_TRUE(m_backend->valid());
}

void CWaylandBackendTest::TearDown() {
    m_backend.reset();
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_toplevel.reset();
    m_xdgSurface.reset();
    m_surface.reset();
    if (!m_installedGlobals)
        return;

    PROTO::xdgShell     = std::move(m_previousXDGShell);
    PROTO::compositor   = std::move(m_previousCoreCompositor);
    PROTO::presentation = std::move(m_previousPresentation);
    g_pEventLoopManager = std::move(m_previousEventLoop);
    g_pCompositor       = std::move(m_previousCompositor);
    Event::bus()        = std::move(m_previousEventBus);
}

void CWaylandBackendTest::expectConfigure(const Vector2D& size) {
    // Check the size passed to setSize as well as the integer wire payload.
    EXPECT_EQ(m_toplevel->m_pendingApply.size, size);
    wl_event_loop_dispatch_idle(wl_display_get_event_loop(m_display.get()));
    wl_client_flush(m_client);

    // XDG v1 has no initial capabilities event or tiled states: one toplevel
    // configure with an empty states array, followed by its surface serial.
    std::array<uint32_t, 8> events = {};
    ASSERT_EQ(recv(m_socket.get(), events.data(), sizeof(events), MSG_DONTWAIT), sc<ssize_t>(sizeof(events)));
    EXPECT_EQ(events[0], 4U);
    EXPECT_EQ(events[1], 20U << 16); // xdg_toplevel.configure
    EXPECT_EQ(events[2], sc<uint32_t>(size.x));
    EXPECT_EQ(events[3], sc<uint32_t>(size.y));
    EXPECT_EQ(events[4], 0U); // Empty states array.
    EXPECT_EQ(events[5], 3U);
    EXPECT_EQ(events[6], 12U << 16); // xdg_surface.configure
    EXPECT_NE(events[7], m_lastSerial);
    m_lastSerial = events[7];
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());
}

void CWaylandBackendTest::expectNoConfigure() {
    wl_event_loop_dispatch_idle(wl_display_get_event_loop(m_display.get()));
    wl_client_flush(m_client);
    uint32_t event = 0;
    ASSERT_EQ(recv(m_socket.get(), &event, sizeof(event), MSG_DONTWAIT), -1);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
}

void CWaylandBackendTest::request(uint32_t id, uint32_t opcode, std::initializer_list<uint32_t> args) {
    std::vector<uint32_t> message = {
        id,
        sc<uint32_t>((args.size() + 2) * sizeof(uint32_t) << 16) | opcode,
    };
    message.insert(message.end(), args);
    const auto size = message.size() * sizeof(uint32_t);
    ASSERT_EQ(send(m_socket.get(), message.data(), size, MSG_NOSIGNAL), sc<ssize_t>(size));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
}

void CWaylandBackendTest::acknowledge(uint32_t serial, const Vector2D& size) {
    const auto previousSize = m_backend->reportedSize();
    ASSERT_NO_FATAL_FAILURE(request(3, 4, {serial})); // xdg_surface.ack_configure
    EXPECT_EQ(m_surface->m_pending.ackedSize, size);
    EXPECT_TRUE(m_surface->m_pending.updated.bits.acked);
    EXPECT_EQ(m_backend->reportedSize(), previousSize);

    ASSERT_NO_FATAL_FAILURE(request(2, 6)); // wl_surface.commit
    EXPECT_EQ(m_backend->reportedSize(), size);
}

TEST_F(CWaylandBackendTest, DeduplicatesFractionalRequestsWithTheSameClientSize) {
    m_backend->configure({0, 0, 640.25, 480.75}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({640, 480}));

    m_backend->configure({0, 0, 640.9, 480.1}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());
    m_backend->configure({0, 0, 640, 480}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());

    ASSERT_NO_FATAL_FAILURE(acknowledge(m_lastSerial, {640, 480}));
    m_backend->configure({0, 0, 640.1, 480.9}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());
}

TEST_F(CWaylandBackendTest, IntegerBoundariesConfigureAndAcknowledgeEachClientSize) {
    m_backend->configure({0, 0, 640.9, 480.9}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({640, 480}));
    const auto firstSerial = m_lastSerial;

    m_backend->configure({0, 0, 641.1, 480.9}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({641, 480}));
    const auto secondSerial = m_lastSerial;

    m_backend->configure({0, 0, 641.8, 481.1}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({641, 481}));

    // Outstanding acknowledgements must use their own size, not the latest request.
    ASSERT_NO_FATAL_FAILURE(acknowledge(firstSerial, {640, 480}));
    ASSERT_NO_FATAL_FAILURE(acknowledge(secondSerial, {641, 480}));
    ASSERT_NO_FATAL_FAILURE(acknowledge(m_lastSerial, {641, 481}));
}

TEST_F(CWaylandBackendTest, ForcedConfigureResendsTheSameClientSize) {
    m_backend->configure({0, 0, 640.25, 480.75}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({640, 480}));
    ASSERT_NO_FATAL_FAILURE(acknowledge(m_lastSerial, {640, 480}));

    m_backend->configure({0, 0, 640.25, 480.75}, nullptr, true);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({640, 480}));
    ASSERT_NO_FATAL_FAILURE(acknowledge(m_lastSerial, {640, 480}));

    m_backend->configure({0, 0, 640.9, 480.1}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());
}

TEST_F(CWaylandBackendTest, CoalescedConfigureAcknowledgesTheLastClientSize) {
    m_backend->configure({0, 0, 640.25, 480.75}, nullptr);
    m_backend->configure({0, 0, 640.9, 480.1}, nullptr);
    m_backend->configure({0, 0, 641.5, 481.5}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectConfigure({641, 481}));
    ASSERT_NO_FATAL_FAILURE(acknowledge(m_lastSerial, {641, 481}));

    m_backend->configure({0, 0, 641.9, 481.1}, nullptr);
    ASSERT_NO_FATAL_FAILURE(expectNoConfigure());
}
