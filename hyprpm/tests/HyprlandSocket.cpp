#include "../src/core/HyprlandSocket.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <optional>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include <gtest/gtest.h>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

class CEnvironmentGuard { // NOLINT(misc-use-internal-linkage)
  public:
    CEnvironmentGuard(std::string name, const std::string& value) : m_name(std::move(name)) {
        if (const auto* const CURRENT = getenv(m_name.c_str()))
            m_previous = CURRENT;

        m_valid = setenv(m_name.c_str(), value.c_str(), 1) == 0;
    }

    ~CEnvironmentGuard() {
        if (m_previous)
            setenv(m_name.c_str(), m_previous->c_str(), 1);
        else
            unsetenv(m_name.c_str());
    }

    bool valid() const {
        return m_valid;
    }

  private:
    std::string                m_name;
    std::optional<std::string> m_previous;
    bool                       m_valid = false;
};

struct SServerResult { // NOLINT(misc-use-internal-linkage)
    std::string request;
    std::string error;
};

static SServerResult serverError(std::string error) {
    return {
        .request = {},
        .error   = std::move(error),
    };
}

static SServerResult serveRequest(const int listener, const std::string& reply, const bool emulateLegacyReader) {
    pollfd pollFD = {
        .fd      = listener,
        .events  = POLLIN,
        .revents = 0,
    };

    if (poll(&pollFD, 1, 5000) != 1)
        return serverError("timed out waiting for the client");

    CFileDescriptor client{accept4(listener, nullptr, nullptr, SOCK_CLOEXEC)};
    if (!client.isValid())
        return serverError("failed to accept the client");

    const timeval timeout = {
        .tv_sec  = 5,
        .tv_usec = 0,
    };
    if (setsockopt(client.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        return serverError("failed to set the receive timeout");

    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    SServerResult          result;
    std::array<char, 4096> buffer = {};
    while (true) {
        const auto READ_SIZE = emulateLegacyReader ? size_t{1023} : buffer.size();
        const auto FLAGS     = emulateLegacyReader ? MSG_WAITALL : 0;
        const auto SIZE      = recv(client.get(), buffer.data(), READ_SIZE, FLAGS);
        if (SIZE < 0)
            return serverError("failed to read the framed request");
        if (SIZE == 0)
            break;

        result.request.append(buffer.data(), sc<size_t>(SIZE));
        if (emulateLegacyReader ? sc<size_t>(SIZE) < READ_SIZE : result.request.contains('\0'))
            break;
    }

    size_t written = 0;
    while (written < reply.size()) {
        const auto SIZE = send(client.get(), reply.data() + written, reply.size() - written, MSG_NOSIGNAL);
        if (SIZE <= 0)
            return serverError("failed to write the reply");

        written += sc<size_t>(SIZE);
    }

    return result;
}

TEST(HyprpmSocket, framesLargeRequestsAndCollectsReplies) {
    const auto ROOT = std::filesystem::temp_directory_path() / std::format("hyprpm-socket-test-{}-{}", getpid(), std::chrono::steady_clock::now().time_since_epoch().count());
    const auto INSTANCE_DIR = ROOT / "hypr" / "test-instance";
    ASSERT_TRUE(std::filesystem::create_directories(INSTANCE_DIR));

    CEnvironmentGuard runtimeDir{"XDG_RUNTIME_DIR", ROOT.string()};
    CEnvironmentGuard instance{"HYPRLAND_INSTANCE_SIGNATURE", "test-instance"};
    ASSERT_TRUE(runtimeDir.valid());
    ASSERT_TRUE(instance.valid());

    CFileDescriptor listener{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    ASSERT_TRUE(listener.isValid());

    sockaddr_un address    = {};
    address.sun_family     = AF_UNIX;
    const auto SOCKET_PATH = (INSTANCE_DIR / ".socket.sock").string();
    ASSERT_LT(SOCKET_PATH.size(), sizeof(address.sun_path));
    std::memcpy(address.sun_path, SOCKET_PATH.c_str(), SOCKET_PATH.size() + 1);
    ASSERT_EQ(bind(listener.get(), rc<const sockaddr*>(&address), SUN_LEN(&address)), 0) << strerror(errno) << ": " << SOCKET_PATH;
    ASSERT_EQ(listen(listener.get(), 1), 0);

    const std::string COMMAND(size_t{512} * 1024, 'x');
    const std::string EXPECTED_REPLY(size_t{20} * 1024, 'r');
    auto              server = std::async(std::launch::async, serveRequest, listener.get(), std::cref(EXPECTED_REPLY), false);

    EXPECT_EQ(NHyprlandSocket::send(COMMAND), EXPECTED_REPLY);

    const auto RESULT = server.get();
    EXPECT_TRUE(RESULT.error.empty()) << RESULT.error;

    std::string expectedRequest;
    expectedRequest.reserve(COMMAND.size() + 3);
    expectedRequest.push_back('\x1F');
    expectedRequest.push_back('/');
    expectedRequest.append(COMMAND);
    expectedRequest.push_back('\0');
    EXPECT_EQ(RESULT.request, expectedRequest);

    std::error_code ec;
    std::filesystem::remove_all(ROOT, ec);
}

TEST(HyprpmSocket, halfClosesExactLegacyReadBoundary) {
    const auto ROOT = std::filesystem::temp_directory_path() / std::format("hyprpm-socket-test-{}-{}", getpid(), std::chrono::steady_clock::now().time_since_epoch().count());
    const auto INSTANCE_DIR = ROOT / "hypr" / "test-instance";
    ASSERT_TRUE(std::filesystem::create_directories(INSTANCE_DIR));

    CEnvironmentGuard runtimeDir{"XDG_RUNTIME_DIR", ROOT.string()};
    CEnvironmentGuard instance{"HYPRLAND_INSTANCE_SIGNATURE", "test-instance"};
    ASSERT_TRUE(runtimeDir.valid());
    ASSERT_TRUE(instance.valid());

    CFileDescriptor listener{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    ASSERT_TRUE(listener.isValid());

    sockaddr_un address    = {};
    address.sun_family     = AF_UNIX;
    const auto SOCKET_PATH = (INSTANCE_DIR / ".socket.sock").string();
    ASSERT_LT(SOCKET_PATH.size(), sizeof(address.sun_path));
    std::memcpy(address.sun_path, SOCKET_PATH.c_str(), SOCKET_PATH.size() + 1);
    ASSERT_EQ(bind(listener.get(), rc<const sockaddr*>(&address), SUN_LEN(&address)), 0) << strerror(errno) << ": " << SOCKET_PATH;
    ASSERT_EQ(listen(listener.get(), 1), 0);

    // The complete frame is exactly one old-server read buffer. The client
    // must close its write half so the legacy reader can observe EOF.
    const std::string COMMAND = "/" + std::string(1020, 'x');
    auto              server  = std::async(std::launch::async, serveRequest, listener.get(), "ok", true);

    EXPECT_EQ(NHyprlandSocket::send(COMMAND), "ok");

    const auto RESULT = server.get();
    EXPECT_TRUE(RESULT.error.empty()) << RESULT.error;
    ASSERT_EQ(RESULT.request.size(), 1023);
    EXPECT_EQ(RESULT.request.front(), '\x1F');
    EXPECT_EQ(std::string_view{RESULT.request}.substr(1, COMMAND.size()), COMMAND);
    EXPECT_EQ(RESULT.request.back(), '\0');

    std::error_code ec;
    std::filesystem::remove_all(ROOT, ec);
}
