#include "../../hyprtester/src/HyprCtlSocket.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include <gtest/gtest.h>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace std::chrono_literals;

// Project style prefers source-local helpers without anonymous namespaces.
class CTestHyprCtlServer { // NOLINT(misc-use-internal-linkage)
  public:
    using CHandler = std::function<void(int)>;

    explicit CTestHyprCtlServer(CHandler handler) {
        m_path = std::filesystem::temp_directory_path() / std::format("hyprtester-socket-{}-{}", getpid(), ++s_counter);

        m_socket = CFileDescriptor{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
        EXPECT_TRUE(m_socket.isValid());

        sockaddr_un address = {};
        address.sun_family  = AF_UNIX;
        std::memcpy(address.sun_path, m_path.c_str(), m_path.string().size() + 1);
        EXPECT_EQ(bind(m_socket.get(), rc<const sockaddr*>(&address), SUN_LEN(&address)), 0);
        EXPECT_EQ(listen(m_socket.get(), 1), 0);

        m_thread = std::thread([this, handler = std::move(handler)] {
            CFileDescriptor client{accept4(m_socket.get(), nullptr, nullptr, SOCK_CLOEXEC)};
            if (!client.isValid())
                return;
            handler(client.get());
        });
    }

    ~CTestHyprCtlServer() {
        m_thread.join();
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    std::string path() const {
        return m_path;
    }

  private:
    inline static size_t  s_counter = 0;
    std::filesystem::path m_path;
    CFileDescriptor       m_socket;
    std::thread           m_thread;
};

static std::string readRequest(int fd) {
    std::array<char, 4096> buffer = {};
    std::string            request;

    while (true) {
        const auto SIZE = recv(fd, buffer.data(), buffer.size(), 0);
        if (SIZE <= 0)
            break;
        request.append(buffer.data(), static_cast<size_t>(SIZE));
        if (request.contains('\0'))
            break;
    }

    return request;
}

static std::string readLegacyRequest(int fd) {
    std::array<char, 1023> buffer = {};
    std::string            request;

    while (true) {
        const auto SIZE = recv(fd, buffer.data(), buffer.size(), MSG_WAITALL);
        if (SIZE <= 0)
            break;
        request.append(buffer.data(), static_cast<size_t>(SIZE));
        if (static_cast<size_t>(SIZE) < buffer.size())
            break;
    }

    return request;
}

static bool writeReply(int fd, std::string_view reply) {
    size_t written = 0;
    while (written < reply.size()) {
        const auto SIZE = send(fd, reply.data() + written, reply.size() - written, MSG_NOSIGNAL);
        if (SIZE <= 0)
            return false;
        written += static_cast<size_t>(SIZE);
    }
    return true;
}

TEST(HyprtesterSocket, FramesRequestAndReadsFragmentedReply) {
    std::promise<std::string> received;
    auto                      future = received.get_future();
    const std::string         reply  = "first" + std::string(9000, 'x') + "last";

    CTestHyprCtlServer        server{[&](int fd) {
        received.set_value(readRequest(fd));
        ASSERT_TRUE(writeReply(fd, std::string_view{reply}.substr(0, 5)));
        std::this_thread::sleep_for(20ms);
        ASSERT_TRUE(writeReply(fd, std::string_view{reply}.substr(5)));
    }};

    const auto                RESULT = NHyprTester::HyprCtlSocket::request(server.path(), "j/clients", 1s);
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(*RESULT, reply);

    const auto REQUEST = future.get();
    ASSERT_EQ(REQUEST.size(), std::string_view{"j/clients"}.size() + 2);
    EXPECT_EQ(REQUEST.front(), NHyprTester::HyprCtlSocket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(std::string_view{REQUEST}.substr(1, REQUEST.size() - 2), "j/clients");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprtesterSocket, SendsLargeRequestsCompletely) {
    std::promise<std::string> received;
    auto                      future  = received.get_future();
    const std::string         command = "/eval " + std::string(size_t{512} * 1024, 'x');

    CTestHyprCtlServer        server{[&](int fd) {
        std::this_thread::sleep_for(20ms);
        received.set_value(readRequest(fd));
        ASSERT_TRUE(writeReply(fd, "ok"));
    }};

    const auto                RESULT = NHyprTester::HyprCtlSocket::request(server.path(), command, 2s);
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(*RESULT, "ok");

    const auto REQUEST = future.get();
    ASSERT_EQ(REQUEST.size(), command.size() + 2);
    EXPECT_EQ(std::string_view{REQUEST}.substr(1, command.size()), command);
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprtesterSocket, HalfClosesExactLegacyReadBoundary) {
    std::promise<std::string> received;
    auto                      future  = received.get_future();
    const std::string         command = "/" + std::string(1020, 'x');

    CTestHyprCtlServer        server{[&](int fd) {
        received.set_value(readLegacyRequest(fd));
        ASSERT_TRUE(writeReply(fd, "ok"));
    }};

    const auto                RESULT = NHyprTester::HyprCtlSocket::request(server.path(), command, 1s);
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(*RESULT, "ok");

    const auto REQUEST = future.get();
    ASSERT_EQ(REQUEST.size(), 1023);
    EXPECT_EQ(REQUEST.front(), NHyprTester::HyprCtlSocket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(std::string_view{REQUEST}.substr(1, command.size()), command);
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprtesterSocket, KeepsUnflaggedRequestsCompatibleWithOldServers) {
    std::promise<std::string> received;
    auto                      future = received.get_future();

    CTestHyprCtlServer        server{[&](int fd) {
        received.set_value(readRequest(fd));
        ASSERT_TRUE(writeReply(fd, "ok"));
    }};

    const auto                RESULT = NHyprTester::HyprCtlSocket::request(server.path(), "dispatch exit", 1s);
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(*RESULT, "ok");

    const auto REQUEST = future.get();
    ASSERT_EQ(REQUEST.size(), std::string_view{"dispatch exit"}.size() + 3);
    EXPECT_EQ(REQUEST.at(0), NHyprTester::HyprCtlSocket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(REQUEST.at(1), '/');
    EXPECT_EQ(std::string_view{REQUEST}.substr(2, REQUEST.size() - 3), "dispatch exit");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprtesterSocket, KeepsBatchesCompatibleWithOldServers) {
    std::promise<std::string> received;
    auto                      future = received.get_future();

    CTestHyprCtlServer        server{[&](int fd) {
        received.set_value(readRequest(fd));
        ASSERT_TRUE(writeReply(fd, "ok"));
    }};

    const auto                RESULT = NHyprTester::HyprCtlSocket::request(server.path(), "[[BATCH]]j/monitors;j/clients", 1s);
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(*RESULT, "ok");

    const auto REQUEST = future.get();
    ASSERT_EQ(REQUEST.size(), std::string_view{"[[BATCH]]j/monitors;j/clients"}.size() + 3);
    EXPECT_EQ(REQUEST.at(0), NHyprTester::HyprCtlSocket::EXPLICIT_FRAME_MARKER);
    EXPECT_EQ(REQUEST.at(1), '/');
    EXPECT_EQ(std::string_view{REQUEST}.substr(2, REQUEST.size() - 3), "[[BATCH]]j/monitors;j/clients");
    EXPECT_EQ(REQUEST.back(), '\0');
}

TEST(HyprtesterSocket, TimesOutWaitingForReply) {
    CTestHyprCtlServer server{[](int fd) {
        readRequest(fd);
        std::this_thread::sleep_for(100ms);
    }};

    const auto         RESULT = NHyprTester::HyprCtlSocket::request(server.path(), "/version", 20ms);
    ASSERT_FALSE(RESULT.has_value());
    EXPECT_EQ(RESULT.error().stage, NHyprTester::HyprCtlSocket::eErrorStage::READ);
    EXPECT_EQ(RESULT.error().code, ETIMEDOUT);
}
