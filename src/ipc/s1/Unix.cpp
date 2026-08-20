#include "Unix.hpp"

#include "../../Compositor.hpp"
#include "../../debug/log/RollingLogFollow.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <optional>
#include <ranges>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace Hyprutils::OS;
using namespace IPC::Socket1;

#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/ucred.h>
#define CRED_T   xucred
#define CRED_LVL SOL_LOCAL
#define CRED_OPT LOCAL_PEERCRED
#define CRED_PID cr_pid
#elif defined(__NetBSD__)
#define CRED_T   unpcbid
#define CRED_LVL SOL_LOCAL
#define CRED_OPT LOCAL_PEEREID
#define CRED_PID unp_pid
#else
#if defined(__OpenBSD__)
#define CRED_T sockpeercred
#else
#define CRED_T ucred
#endif
#define CRED_LVL SOL_SOCKET
#define CRED_OPT SO_PEERCRED
#define CRED_PID pid
#endif

static constexpr size_t REQUEST_LIMIT      = 1024 * 1024;
static constexpr size_t FOLLOW_QUEUE_LIMIT = 64 * 1024;

static int              onServerEvent(int fd, uint32_t mask, void* data) {
    return rc<CUnixImpl*>(data)->onServerEvent(mask);
}

static int onClientEvent(int fd, uint32_t mask, void* data) {
    return rc<CUnixImpl*>(data)->onClientEvent(fd, mask);
}

static pid_t peerPid(int fd) {
    CRED_T   credentials{};
    uint32_t length = sizeof(credentials);

    if (getsockopt(fd, CRED_LVL, CRED_OPT, &credentials, &length) < 0) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] failed to get peer credentials: {}", strerror(errno));
        return 0;
    }

    return credentials.CRED_PID;
}

CUnixPeer::CUnixPeer(CFileDescriptor&& fd, pid_t pid, CUnixImpl& parent) :
    m_fd(std::move(fd)), m_pid(pid), m_parent(parent), m_eventSource(wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_fd.get(), WL_EVENT_READABLE, ::onClientEvent, &parent)) {
    ;
}

CUnixPeer::~CUnixPeer() {
    if (m_state == eState::FOLLOWING)
        Log::SRollingLogFollow::get().stopFor(m_fd.get());

    if (m_eventSource)
        wl_event_source_remove(m_eventSource);
}

void CUnixPeer::init(const SP<CUnixPeer>& self) {
    m_self         = self;
    m_requestTimer = makeShared<CEventLoopTimer>(
        std::chrono::seconds(5),
        [weak = m_self](SP<CEventLoopTimer>, void*) {
            const auto peer = weak.lock();
            if (!peer)
                return;

            peer->m_parent.removeById(peer->id());
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_requestTimer);
}

size_t CUnixPeer::id() const {
    return m_fd.isValid() ? sc<size_t>(m_fd.get()) : 0;
}

bool CUnixPeer::good() const {
    return m_fd.isValid() && m_eventSource;
}

pid_t CUnixPeer::pid() const {
    return m_pid;
}

void CUnixPeer::updateMask(uint32_t mask) {
    if (m_eventSource)
        wl_event_source_fd_update(m_eventSource, mask);
}

bool CUnixPeer::readRequest(std::string& request) {
    std::array<char, 4096> buffer{};

    while (true) {
        const auto size = read(m_fd.get(), buffer.data(), buffer.size());
        if (size > 0) {
            m_input.append(buffer.data(), sc<size_t>(size));
            if (m_input.size() > REQUEST_LIMIT)
                return false;
            continue;
        }

        if (size < 0 && errno == EINTR)
            continue;

        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        if (size == 0 && m_input.empty())
            return false;

        break;
    }

    if (m_input.empty())
        return true;

    request = std::move(m_input);
    m_requestTimer.reset();
    m_state = eState::DEFERRED;
    updateMask(0);
    return true;
}

bool CUnixPeer::setResponse(SResponse&& response) {
    m_replyMode = response.mode;

    if (std::holds_alternative<SP<CPromise<std::string>>>(response.result)) {
        m_state            = eState::DEFERRED;
        const auto PROMISE = std::get<SP<CPromise<std::string>>>(std::move(response.result));
        PROMISE->then([weak = m_self, mode = response.mode](SP<CPromiseResult<std::string>> result) {
            const auto peer = weak.lock();
            if (!peer)
                return;
            peer->m_parent.completeDeferred(weak, result, mode);
        });
        return true;
    }

    m_output = std::get<std::string>(std::move(response.result));
    m_state  = eState::WRITING;
    return flush();
}

bool CUnixPeer::addFollowData(std::string&& data) {
    if (queuedBytes() + data.size() > FOLLOW_QUEUE_LIMIT)
        return false;

    m_output += std::move(data);
    return flush();
}

bool CUnixPeer::flush() {
    while (m_writeOffset < m_output.size()) {
        const auto written = write(m_fd.get(), m_output.data() + m_writeOffset, m_output.size() - m_writeOffset);
        if (written > 0) {
            m_writeOffset += sc<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            updateMask(WL_EVENT_WRITABLE);
            return true;
        }

        return false;
    }

    m_output.clear();
    m_writeOffset = 0;
    updateMask(0);
    return true;
}

bool CUnixPeer::shouldClose() const {
    return m_state == eState::WRITING && m_output.empty() && m_replyMode == REPLY_MODE_CLOSE;
}

bool CUnixPeer::shouldFollow() const {
    return m_state == eState::WRITING && m_output.empty() && m_replyMode == REPLY_MODE_FOLLOW;
}

bool CUnixPeer::isFollowing() const {
    return m_state == eState::FOLLOWING;
}

size_t CUnixPeer::queuedBytes() const {
    return m_output.size() - m_writeOffset;
}

void CUnixPeer::markFollowing() {
    m_state = eState::FOLLOWING;
}

CUnixImpl::CUnixImpl() : m_socket(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) {
    ;
}

CUnixImpl::~CUnixImpl() {
    m_peers.clear();
    m_followTimer.reset();

    if (m_eventSource)
        wl_event_source_remove(m_eventSource);

    if (!m_socketPath.empty())
        unlink(m_socketPath.c_str());
}

void CUnixImpl::start(FRequestHandler&& handler) {
    m_requestHandler = std::move(handler);

    if (!m_socket.isValid()) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] couldn't create socket");
        return;
    }

    sockaddr_un address = {.sun_family = AF_UNIX};
    m_socketPath        = std::format("{}/.socket.sock", g_pCompositor->m_instancePath);

    if (m_socketPath.size() > sizeof(address.sun_path) - 1) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] socket path is too long");
        m_socket.reset();
        return;
    }

    strncpy(address.sun_path, m_socketPath.c_str(), sizeof(address.sun_path) - 1);

    if (bind(m_socket.get(), rc<sockaddr*>(&address), SUN_LEN(&address)) < 0) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] couldn't bind socket: {}", strerror(errno));
        m_socket.reset();
        return;
    }

    if (listen(m_socket.get(), 10) < 0) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] couldn't listen on socket: {}", strerror(errno));
        m_socket.reset();
        return;
    }

    m_eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_socket.get(), WL_EVENT_READABLE, ::onServerEvent, this);
    if (!m_eventSource) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] couldn't register socket with the event loop");
        m_socket.reset();
        return;
    }

    Log::logger->log(Log::DEBUG, "[Socket1::Unix] socket started at {}", m_socketPath);
}

int CUnixImpl::onServerEvent(uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Log::logger->log(Log::ERR, "[Socket1::Unix] listener hung up");
        wl_event_source_remove(m_eventSource);
        m_eventSource = nullptr;
        m_socket.reset();
        return 0;
    }

    while (true) {
        CFileDescriptor connection{accept4(m_socket.get(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK)};
        if (!connection.isValid()) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                Log::logger->log(Log::ERR, "[Socket1::Unix] failed to accept connection: {}", strerror(errno));
            if (errno == EINTR)
                continue;
            break;
        }

        const auto PID  = peerPid(connection.get());
        auto       peer = makeShared<CUnixPeer>(std::move(connection), PID, *this);
        if (!peer->good())
            continue;

        peer->init(peer);
        m_peers.emplace_back(std::move(peer));
        Log::logger->log(Log::DEBUG, "[Socket1::Unix] accepted fd {} from pid {}", m_peers.back()->id(), PID);
    }

    return 0;
}

int CUnixImpl::onClientEvent(int fd, uint32_t mask) {
    const auto peer = findByFd(fd);
    if (!peer)
        return 0;

    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        removeById(peer->id());
        return 0;
    }

    if (mask & WL_EVENT_READABLE) {
        std::string request;
        if (!peer->readRequest(request)) {
            removeById(peer->id());
            return 0;
        }

        if (!request.empty()) {
            auto response = m_requestHandler(std::move(request), peer->pid());
            if (!peer->setResponse(std::move(response))) {
                removeById(peer->id());
                return 0;
            }
        }
    }

    if (mask & WL_EVENT_WRITABLE && !peer->flush()) {
        removeById(peer->id());
        return 0;
    }

    handleResponse(peer);
    return 0;
}

SP<CUnixPeer> CUnixImpl::findByFd(int fd) const {
    const auto peer = std::ranges::find_if(m_peers, [fd](const auto& candidate) { return candidate->id() == sc<size_t>(fd); });
    return peer == m_peers.end() ? nullptr : *peer;
}

void CUnixImpl::removeById(size_t id) {
    std::erase_if(m_peers, [id](const auto& peer) { return peer->id() == id; });
}

void CUnixImpl::completeDeferred(const WP<CUnixPeer>& weak, SP<CPromiseResult<std::string>> result, eReplyMode mode) {
    const auto peer = weak.lock();
    if (!peer || !std::ranges::contains(m_peers, peer))
        return;

    const auto reply = result->hasError() ? result->error() : result->result();
    if (!peer->setResponse(SResponse{reply, mode})) {
        removeById(peer->id());
        return;
    }

    handleResponse(peer);
}

void CUnixImpl::handleResponse(const SP<CUnixPeer>& peer) {
    if (peer->shouldClose()) {
        removeById(peer->id());
        return;
    }

    if (peer->shouldFollow())
        beginFollowing(peer);
}

void CUnixImpl::beginFollowing(const SP<CUnixPeer>& peer) {
    peer->markFollowing();
    Log::SRollingLogFollow::get().startFor(sc<int>(peer->id()));

    if (!m_followTimer) {
        m_followTimer = makeShared<CEventLoopTimer>(
            std::chrono::milliseconds(100),
            [this](SP<CEventLoopTimer> self, void*) {
                flushFollowers();
                self->updateTimeout(hasFollowers() ? std::optional<Time::steady_dur>{std::chrono::milliseconds(100)} : std::nullopt);
            },
            nullptr);
        g_pEventLoopManager->addTimer(m_followTimer);
    } else
        m_followTimer->updateTimeout(std::chrono::milliseconds(100));
}

void CUnixImpl::flushFollowers() {
    const auto peers = m_peers;
    for (const auto& peer : peers) {
        if (!peer->isFollowing() || !std::ranges::contains(m_peers, peer))
            continue;

        auto data = Log::SRollingLogFollow::get().takeLog(sc<int>(peer->id()));
        if (data.empty())
            continue;

        if (!peer->addFollowData(std::move(data)))
            removeById(peer->id());
    }
}

bool CUnixImpl::hasFollowers() const {
    return std::ranges::any_of(m_peers, [](const auto& peer) { return peer->isFollowing(); });
}
