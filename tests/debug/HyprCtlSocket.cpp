#include <debug/HyprCtlSocket.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

struct SSocketPair {
    CFileDescriptor server;
    CFileDescriptor client;
};

static SSocketPair makeSocketPair() {
    std::array<int, 2> sockets = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) < 0)
        return {};

    for (const int fd : sockets) {
        const int FLAGS = fcntl(fd, F_GETFL, 0);
        if (FLAGS < 0 || fcntl(fd, F_SETFL, FLAGS | O_NONBLOCK) < 0) {
            close(sockets.at(0));
            close(sockets.at(1));
            return {};
        }
    }

    return {
        .server = CFileDescriptor{sockets.at(0)},
        .client = CFileDescriptor{sockets.at(1)},
    };
}

static bool sendAll(int fd, std::string_view data) {
    size_t written = 0;
    while (written < data.size()) {
        const auto SIZE = send(fd, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (SIZE > 0) {
            written += sc<size_t>(SIZE);
            continue;
        }

        if (SIZE < 0 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

TEST(HyprCtlSocket, waitsForFragmentedExplicitRequest) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    NHyprCtlSocket::SRequest request;
    const std::string        first = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + "disp";

    ASSERT_TRUE(sendAll(sockets.client.get(), first)) << "send failed: " << strerror(errno);
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INCOMPLETE);
    EXPECT_TRUE(request.explicitlyFramed);
    EXPECT_EQ(request.payload, "disp");

    ASSERT_TRUE(sendAll(sockets.client.get(), "atch"));
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INCOMPLETE);
    EXPECT_EQ(request.payload, "dispatch");

    ASSERT_TRUE(sendAll(sockets.client.get(), std::string_view{"\0", 1}));
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_COMPLETE);
    EXPECT_EQ(request.payload, "dispatch");
}

TEST(HyprCtlSocket, completesLegacyRequestAtEOF) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    NHyprCtlSocket::SRequest request;
    ASSERT_TRUE(sendAll(sockets.client.get(), "ver"));
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INCOMPLETE);

    ASSERT_TRUE(sendAll(sockets.client.get(), "sion"));
    ASSERT_EQ(shutdown(sockets.client.get(), SHUT_WR), 0);
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_COMPLETE);
    EXPECT_EQ(request.payload, "version");
}

TEST(HyprCtlSocket, completesFragmentedExplicitRequestAtEOF) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    NHyprCtlSocket::SRequest request;
    const std::string        first = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + "ver";

    ASSERT_TRUE(sendAll(sockets.client.get(), first));
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INCOMPLETE);
    EXPECT_TRUE(request.explicitlyFramed);

    ASSERT_TRUE(sendAll(sockets.client.get(), "sion"));
    ASSERT_EQ(shutdown(sockets.client.get(), SHUT_WR), 0);
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_COMPLETE);
    EXPECT_EQ(request.payload, "version");
}

TEST(HyprCtlSocket, acceptsMarkerOnlyAtEOF) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    NHyprCtlSocket::SRequest request;
    ASSERT_TRUE(sendAll(sockets.client.get(), std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER}));
    ASSERT_EQ(shutdown(sockets.client.get(), SHUT_WR), 0);

    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_COMPLETE);
    EXPECT_TRUE(request.explicitlyFramed);
    EXPECT_TRUE(request.payload.empty());
}

TEST(HyprCtlSocket, excludesFramingFromPayloadLimit) {
    constexpr size_t LIMIT = 8;

    auto             sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    const std::string valid = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + "12345678" + '\0';
    ASSERT_TRUE(sendAll(sockets.client.get(), valid));

    NHyprCtlSocket::SRequest request;
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request, LIMIT), NHyprCtlSocket::eReadResult::READ_COMPLETE);
    EXPECT_EQ(request.payload, "12345678");

    sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    const std::string oversized = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + "123456789" + '\0';
    ASSERT_TRUE(sendAll(sockets.client.get(), oversized));

    request = {};
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request, LIMIT), NHyprCtlSocket::eReadResult::READ_TOO_LARGE);
}

TEST(HyprCtlSocket, rejectsDataAfterTerminator) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    std::string framed = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + "version" + '\0';
    framed += "another request";
    ASSERT_TRUE(sendAll(sockets.client.get(), framed));

    NHyprCtlSocket::SRequest request;
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INVALID);
}

TEST(HyprCtlSocket, rejectsQueuedDataAfterBufferBoundaryTerminator) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    std::string framed = std::string{NHyprCtlSocket::EXPLICIT_FRAME_MARKER} + std::string(4094, 'x') + '\0';
    framed += 'y';
    ASSERT_TRUE(sendAll(sockets.client.get(), framed));

    NHyprCtlSocket::SRequest request;
    EXPECT_EQ(NHyprCtlSocket::readRequest(sockets.server.get(), request), NHyprCtlSocket::eReadResult::READ_INVALID);
}

TEST(HyprCtlSocket, resumesPartialReplies) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    int sendBufferSize = 4096;
    ASSERT_EQ(setsockopt(sockets.server.get(), SOL_SOCKET, SO_SNDBUF, &sendBufferSize, sizeof(sendBufferSize)), 0) << "setsockopt failed: " << strerror(errno);

    const std::string reply(size_t{2} * 1024 * 1024, 'x');
    std::string       received;
    size_t            written = 0;

    EXPECT_EQ(NHyprCtlSocket::writeReply(sockets.server.get(), reply, written), NHyprCtlSocket::eWriteResult::WRITE_BLOCKED);
    EXPECT_GT(written, 0);
    EXPECT_LT(written, reply.size());

    std::array<char, 8192> buffer = {};
    for (size_t attempts = 0; written < reply.size() && attempts < 10000; ++attempts) {
        while (true) {
            const auto SIZE = recv(sockets.client.get(), buffer.data(), buffer.size(), 0);
            if (SIZE > 0) {
                received.append(buffer.data(), sc<size_t>(SIZE));
                continue;
            }
            ASSERT_TRUE(SIZE < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
            break;
        }

        const auto RESULT = NHyprCtlSocket::writeReply(sockets.server.get(), reply, written);
        ASSERT_NE(RESULT, NHyprCtlSocket::eWriteResult::WRITE_ERROR);
    }

    EXPECT_EQ(written, reply.size());

    while (true) {
        const auto SIZE = recv(sockets.client.get(), buffer.data(), buffer.size(), 0);
        if (SIZE > 0) {
            received.append(buffer.data(), sc<size_t>(SIZE));
            continue;
        }
        ASSERT_TRUE(SIZE < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        break;
    }

    EXPECT_EQ(received, reply);
}

TEST(HyprCtlSocket, reportsClosedPeerWithoutSIGPIPE) {
    auto sockets = makeSocketPair();
    ASSERT_TRUE(sockets.server.isValid());
    ASSERT_TRUE(sockets.client.isValid());

    sockets.client.reset();

    size_t written = 0;
    EXPECT_EQ(NHyprCtlSocket::writeReply(sockets.server.get(), "reply", written), NHyprCtlSocket::eWriteResult::WRITE_ERROR);
    EXPECT_EQ(written, 0);
}
