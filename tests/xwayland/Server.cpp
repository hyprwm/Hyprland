#ifndef NO_XWAYLAND

#include <helpers/signal/Signal.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <array>

// Inspect the socket peers without adding a production test API.
#define private public
#include <xwayland/Server.hpp>
#undef private

#include <Compositor.hpp>
#include <gtest/gtest.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static void expectXWaylandSocketsClosed() {
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> display{
        wl_display_create(),
        wl_display_destroy,
    };
    ASSERT_NE(display, nullptr);

    g_pCompositor                = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay   = display.get();
    g_pCompositor->m_wlEventLoop = wl_display_get_event_loop(display.get());

    CXWaylandServer server;
    // No X11 listen sockets: the real forked child exits in runXWayland before exec.
    // This exercises start() without launching Xwayland or touching a desktop session.
    ASSERT_TRUE(server.start());
    ASSERT_NE(server.m_xwaylandClient, nullptr);

    int status = 0;
    ASSERT_GT(waitpid(-1, &status, 0), 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), EXIT_FAILURE);

    const std::array sockets = {
        server.m_xwmFDs[0].get(),
        wl_client_get_fd(server.m_xwaylandClient),
    };
    for (const auto fd : sockets) {
        SCOPED_TRACE(fd == server.m_xwmFDs[0].get() ? "XWM" : "Wayland");
        pollfd peer = {
            .fd     = fd,
            .events = POLLIN,
        };
        ASSERT_EQ(poll(&peer, 1, 1000), 1) << "Parent retained a child socket after child exit";
        EXPECT_NE(peer.revents & POLLHUP, 0);

        char byte = 0;
        EXPECT_EQ(recv(fd, &byte, 1, MSG_DONTWAIT), 0);
        EXPECT_EQ(send(fd, &byte, 1, MSG_DONTWAIT | MSG_NOSIGNAL), -1);
        EXPECT_EQ(errno, EPIPE);
    }
}

TEST(XWaylandServerDeathTest, ChildExitDisconnectsBothSockets) {
    // Isolate compositor globals and child reaping; bound any accidental blocking.
    ASSERT_EXIT(
        {
            alarm(5);
            expectXWaylandSocketsClosed();
            const auto& result = *::testing::UnitTest::GetInstance()->current_test_info()->result();
            for (int i = 0; i < result.total_part_count(); ++i)
                std::cerr << result.GetTestPartResult(i).summary() << '\n';
            _exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        },
        ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

#endif
