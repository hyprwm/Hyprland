#include <Socket.hpp>

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

TEST(HyprCtlSocket, framesRequestsForNewAndOldServers) {
    const auto REQUEST = HyprCtl::Socket::frameRequest("j/clients");

    ASSERT_EQ(REQUEST.size(), 11);
    EXPECT_EQ(REQUEST.front(), HyprCtl::Socket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(std::string_view(REQUEST.data() + 1, REQUEST.size() - 2), "j/clients");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprCtlSocket, keepsBatchesCompatibleWithOldServers) {
    const auto REQUEST = HyprCtl::Socket::frameRequest("[[BATCH]]clients;monitors");

    ASSERT_GE(REQUEST.size(), 3);
    EXPECT_EQ(REQUEST.at(0), HyprCtl::Socket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(REQUEST.at(1), '/');
    EXPECT_EQ(std::string_view(REQUEST.data() + 2, REQUEST.size() - 3), "[[BATCH]]clients;monitors");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprCtlSocket, addsFlagSeparatorForUnflaggedRequests) {
    const auto REQUEST = HyprCtl::Socket::frameRequest("dispatch exit");

    ASSERT_EQ(REQUEST.size(), 16);
    EXPECT_EQ(REQUEST.at(0), HyprCtl::Socket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(REQUEST.at(1), '/');
    EXPECT_EQ(std::string_view(REQUEST.data() + 2, REQUEST.size() - 3), "dispatch exit");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprCtlSocket, writesAndReadsPartialData) {
    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor   WRITER{sockets.at(0)};
    CFileDescriptor   READER{sockets.at(1)};

    constexpr size_t  DATA_SIZE   = size_t{1024} * 1024;
    constexpr int     BUFFER_SIZE = 1024;
    const std::string DATA(DATA_SIZE, 'x');
    ASSERT_EQ(setsockopt(WRITER.get(), SOL_SOCKET, SO_SNDBUF, &BUFFER_SIZE, sizeof(BUFFER_SIZE)), 0) << errno << " fds " << WRITER.get() << ' ' << READER.get();

    std::string received;
    std::thread reader([&]() {
        char buffer[257] = {};
        while (received.size() < DATA.size()) {
            const auto SIZE = recv(READER.get(), buffer, sizeof(buffer), 0);
            if (SIZE <= 0)
                break;
            received.append(buffer, sc<size_t>(SIZE));
        }
    });

    EXPECT_TRUE(HyprCtl::Socket::writeAll(WRITER.get(), DATA));
    shutdown(WRITER.get(), SHUT_WR);
    reader.join();

    EXPECT_EQ(received, DATA);
}

TEST(HyprCtlSocket, readsUntilPeerCloses) {
    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor READER{sockets.at(0)};
    CFileDescriptor WRITER{sockets.at(1)};
    ASSERT_EQ(send(WRITER.get(), "first", 5, MSG_NOSIGNAL), 5) << errno << " fds " << READER.get() << ' ' << WRITER.get();
    ASSERT_EQ(send(WRITER.get(), " second", 7, MSG_NOSIGNAL), 7);
    WRITER.reset();

    std::string reply;
    EXPECT_EQ(HyprCtl::Socket::readAll(READER.get(), reply), HyprCtl::Socket::eReadResult::SUCCESS);
    EXPECT_EQ(reply, "first second");
}

TEST(HyprCtlSocket, reportsTimeouts) {
    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor READER{sockets.at(0)};
    CFileDescriptor WRITER{sockets.at(1)};
    const auto      FLAGS = fcntl(READER.get(), F_GETFL, 0);
    ASSERT_GE(FLAGS, 0);
    ASSERT_EQ(fcntl(READER.get(), F_SETFL, FLAGS | O_NONBLOCK), 0);

    std::string reply;
    EXPECT_EQ(HyprCtl::Socket::readAll(READER.get(), reply), HyprCtl::Socket::eReadResult::TIMEOUT);
}

TEST(HyprCtlSocket, configuresSendAndReceiveTimeouts) {
    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor CLIENT{sockets.at(0)};
    CFileDescriptor PEER{sockets.at(1)};
    ASSERT_TRUE(HyprCtl::Socket::setTimeouts(CLIENT.get(), 3)) << errno << " fds " << CLIENT.get() << ' ' << PEER.get();

    timeval   receiveTimeout = {};
    timeval   sendTimeout    = {};
    socklen_t timeoutSize    = sizeof(timeval);
    ASSERT_EQ(getsockopt(CLIENT.get(), SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, &timeoutSize), 0);
    timeoutSize = sizeof(timeval);
    ASSERT_EQ(getsockopt(CLIENT.get(), SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, &timeoutSize), 0);
    EXPECT_EQ(receiveTimeout.tv_sec, 3);
    EXPECT_EQ(sendTimeout.tv_sec, 3);
}

TEST(HyprCtlSocket, avoidsSigpipeWhenPeerCloses) {
    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor CLIENT{sockets.at(0)};
    CFileDescriptor PEER{sockets.at(1)};
    PEER.reset();

    EXPECT_FALSE(HyprCtl::Socket::writeAll(CLIENT.get(), "request"));
    EXPECT_TRUE(errno == EPIPE || errno == ECONNRESET) << errno << " fd " << CLIENT.get();
}
