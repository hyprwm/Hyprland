#include "ActorManager.hpp"
#include "SeatManager.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "input/InputManager.hpp"
#include "../Compositor.hpp"
#include "../debug/log/Logger.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/state/ViewState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../devices/IPointer.hpp"
#include "../event/EventBus.hpp"
#include "../ipc/s1/S1.hpp"
#include "../ipc/s1/Unix.hpp"
#include "../ipc/s2/Unix.hpp"
#include "../output/Monitor.hpp"
#include "../protocols/DRMLease.hpp"
#include "../protocols/ExtWorkspace.hpp"
#include "../protocols/InputCapture.hpp"
#include "../protocols/OutputManagement.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Output.hpp"
#include <aquamarine/backend/Backend.hpp>
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <ranges>
#include <string_view>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using namespace Actor;
using namespace Hyprutils::OS;

UP<CActorManager>& Actor::mgr() {
    static UP<CActorManager> manager;
    return manager;
}

CActor::CActor(std::string name, std::string socket) :
    m_name(std::move(name)), m_socket(std::move(socket)), m_seat(makeUnique<CSeatManager>()),
    m_focus(m_socket.empty() ? Desktop::humanFocusState() : makeShared<Desktop::CFocusState>()) {
    ;
}

CActor::~CActor() {
    m_hyprctl.reset();
    m_events.reset();

    for (auto const& s : m_sockets) {
        if (s.source)
            wl_event_source_remove(s.source);

        unlink(s.path.c_str());
    }

    if (!m_ipcPath.empty()) {
        unlink((m_ipcPath + "/.socket2.sock").c_str());
        rmdir(m_ipcPath.c_str());
    }

    if (m_lockFD.isValid())
        unlink(std::format("{}.lock", m_socketPath).c_str());
}

bool CActor::isHuman() const {
    return m_socket.empty();
}

bool CActor::owns(wl_client* client) const {
    return mgr()->actorFor(client).get() == this;
}

bool CActor::owns(PHLWINDOW window) const {
    return mgr()->actorFor(window).get() == this;
}

bool CActor::owns(PHLMONITOR monitor) const {
    return mgr()->actorFor(monitor).get() == this;
}

bool CActor::owns(PHLWORKSPACE workspace) const {
    return workspace && owns(workspace->m_monitor.lock());
}

std::string CActor::display() const {
    return isHuman() ? g_pCompositor->m_wlDisplaySocket : m_socket;
}

std::string CActor::signature() const {
    return isHuman() ? g_pCompositor->m_instanceSignature : std::format("{}/{}", g_pCompositor->m_instanceSignature, m_name);
}

Vector2D CActor::cursor() const {
    return isHuman() ? g_pInputManager->getMouseCoordsInternal() : m_cursor;
}

SP<CActor> CActor::from(void* data) {
    return data ? sc<CActor*>(data)->m_self.lock() : mgr()->human();
}

void CActor::attachPointer(SP<IPointer> pointer) {
    auto& p = m_pointers.emplace_back(makeUnique<SPointer>());

    p->pointer = pointer;
    p->motion  = pointer->m_pointerEvents.motion.listen([this](const IPointer::SMotionEvent& e) { movePointer(m_cursor + e.delta, e.timeMs); });
    p->frame   = pointer->m_pointerEvents.frame.listen([this] { m_seat->sendPointerFrame(); });
    p->destroy = pointer->m_events.destroy.listen([this, raw = p.get()] { std::erase_if(m_pointers, [raw](const auto& other) { return other.get() == raw; }); });

    p->motionAbsolute = pointer->m_pointerEvents.motionAbsolute.listen([this](const IPointer::SMotionAbsoluteEvent& e) {
        if (const auto MONITOR = m_focus->monitor())
            movePointer(MONITOR->m_position + e.absolute * MONITOR->m_size, e.timeMs);
    });

    p->button = pointer->m_pointerEvents.button.listen([this](const IPointer::SButtonEvent& e) {
        if (const auto WINDOW = windowAt(m_cursor); WINDOW && e.state == WL_POINTER_BUTTON_STATE_PRESSED)
            m_focus->fullWindowFocus(WINDOW, Desktop::FOCUS_REASON_CLICK);

        m_seat->sendPointerButton(e.timeMs, e.button, e.state);
    });

    // a discrete step is 120
    p->axis = pointer->m_pointerEvents.axis.listen(
        [this](const IPointer::SAxisEvent& e) { m_seat->sendPointerAxis(e.timeMs, e.axis, e.delta, e.deltaDiscrete / 120, e.deltaDiscrete, e.source, e.relativeDirection); });
}

void CActor::movePointer(const Vector2D& pos, uint32_t timeMs) {
    const auto MONITOR = m_focus->monitor();
    if (!MONITOR)
        return;

    m_cursor = MONITOR->logicalBox().closestPoint(pos);

    Vector2D   local;
    const auto WINDOW  = windowAt(m_cursor);
    const auto SURFACE = WINDOW ? Desktop::viewState()->hitTest().windowSurfaceAt(m_cursor, WINDOW, local) : nullptr;

    m_seat->setPointerFocus(SURFACE, local);
    m_seat->sendPointerMotion(timeMs, local);
}

PHLWINDOW CActor::windowAt(const Vector2D& pos) const {
    const auto WINDOW = Desktop::viewState()->hitTest().windowAt(pos, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);
    return WINDOW && owns(WINDOW) ? WINDOW : nullptr;
}

bool CActor::listen() {
    if (m_name.contains('/')) {
        LOG(Log::ERR, "[actor] {} has no socket: its name has a /", m_name);
        return false;
    }

    const auto RUNTIMEDIR = getenv("XDG_RUNTIME_DIR");
    if (!RUNTIMEDIR) {
        LOG(Log::ERR, "[actor] {} has no socket: XDG_RUNTIME_DIR is not set", m_name);
        return false;
    }

    m_socketPath = std::format("{}/{}", RUNTIMEDIR, m_socket);

    // the lock follows libwayland, so this never takes a socket that another compositor serves
    m_lockFD = CFileDescriptor{open(std::format("{}.lock", m_socketPath).c_str(), O_CREAT | O_CLOEXEC | O_RDWR, 0660)};
    if (!m_lockFD.isValid() || flock(m_lockFD.get(), LOCK_EX | LOCK_NB) < 0) {
        LOG(Log::ERR, "[actor] {} has no socket: {} is in use", m_name, m_socketPath);
        m_lockFD.reset();
        return false;
    }

    unlink(m_socketPath.c_str());

    const auto IPCPATH = std::format("{}/{}", g_pCompositor->m_instancePath, m_name);
    if (mkdir(IPCPATH.c_str(), S_IRWXU) < 0) {
        LOG(Log::ERR, "[actor] {} has no hyprctl: cannot create {}", m_name, IPCPATH);
        return false;
    }
    m_ipcPath = IPCPATH;

    if (!listenOn(m_socketPath, onListen))
        return false;

    // a request on its hyprctl runs as this actor
    m_hyprctl = makeUnique<IPC::Socket1::CUnixImpl>(m_ipcPath + "/.socket.sock");
    m_hyprctl->start([this](std::string&& request, pid_t pid) {
        CScope scope{m_self.lock()};
        return IPC::Socket1::sock()->dispatch(std::move(request), pid);
    });

    m_events = makeUnique<IPC::Socket2::CUnixImpl>(m_ipcPath + "/.socket2.sock");
    if (!m_hyprctl->listening() || !m_events->listening()) {
        LOG(Log::ERR, "[actor] {} cannot listen on {}", m_name, m_ipcPath);
        return false;
    }

    return true;
}

bool CActor::listenOn(const std::string& path, wl_event_loop_fd_func_t onReadable) {
    sockaddr_un address = {.sun_family = AF_UNIX};
    if (path.size() >= sizeof(address.sun_path)) {
        LOG(Log::ERR, "[actor] {} cannot listen: {} is too long", m_name, path);
        return false;
    }
    std::ranges::copy(path, address.sun_path);

    CFileDescriptor fd{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    if (!fd.isValid() || bind(fd.get(), rc<sockaddr*>(&address), sizeof(address)) < 0) {
        LOG(Log::ERR, "[actor] {} cannot listen on {}", m_name, path);
        return false;
    }

    // bound, so it is ours to unlink
    auto& s  = m_sockets.emplace_back(SSocket{.fd = std::move(fd), .path = path});
    s.source = ::listen(s.fd.get(), 128) < 0 ? nullptr : wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, s.fd.get(), WL_EVENT_READABLE, onReadable, this);
    if (!s.source) {
        LOG(Log::ERR, "[actor] {} cannot listen on {}", m_name, path);
        return false;
    }

    return true;
}

int CActor::onListen(int fd, uint32_t, void* data) {
    const auto      ACTOR = sc<CActor*>(data);

    CFileDescriptor clientFD{accept4(fd, nullptr, nullptr, SOCK_CLOEXEC)};
    if (!clientFD.isValid()) {
        LOG(Log::ERR, "[actor] {} failed to accept a client", ACTOR->m_name);
        return 0;
    }

    const auto CLIENT = wl_client_create(g_pCompositor->m_wlDisplay, clientFD.get());
    if (!CLIENT) {
        LOG(Log::ERR, "[actor] {} failed to create a client", ACTOR->m_name);
        return 0;
    }

    clientFD.take(); // the client owns it now
    mgr()->m_clients.at(CLIENT)->actor = ACTOR->m_self;
    return 0;
}

CActorManager::CActorManager() {
    m_actors.emplace_back(makeShared<CActor>("human", ""));

    if (const auto ACTORS = getenv("HYPRLAND_ACTORS")) {
        for (const auto& range : std::string_view{ACTORS} | std::views::split(',')) {
            const std::string_view ENTRY{range.begin(), range.end()};
            const auto             EQUALS = ENTRY.find('=');

            if (EQUALS == std::string_view::npos || EQUALS == 0 || EQUALS + 1 == ENTRY.size()) {
                LOG(Log::ERR, "[actor] {} in HYPRLAND_ACTORS is not name=socket", ENTRY);
                continue;
            }

            auto agent = makeShared<CActor>(std::string{ENTRY.substr(0, EQUALS)}, std::string{ENTRY.substr(EQUALS + 1)});
            if (!agent->listen())
                continue;

            LOG(Log::DEBUG, "[actor] {} listens on {} and {}", agent->m_name, agent->m_socketPath, agent->m_ipcPath);
            m_actors.emplace_back(std::move(agent));
        }
    }

    for (auto const& a : m_actors) {
        a->m_self = a;
    }

    m_clientCreated.notify = onClientCreated;
    wl_display_add_client_created_listener(g_pCompositor->m_wlDisplay, &m_clientCreated);
    m_logger = wl_display_add_protocol_logger(g_pCompositor->m_wlDisplay, onRequest, nullptr);

    m_monitorAdded = Event::bus()->m_events.monitor.added.listen([this](PHLMONITOR monitor) {
        const auto ACTOR = actorFor(monitor);
        if (ACTOR->isHuman())
            return;

        CScope scope{ACTOR};
        Desktop::focusState()->rawMonitorFocus(monitor);
    });

    if (m_actors.size() > 1)
        g_pEventLoopManager->doLater([this] { createMonitors(); });
}

CActorManager::~CActorManager() {
    for (auto const& [_, c] : m_clients) {
        wl_list_remove(&c->destroy.link);
        wl_list_remove(&c->destroyLate.link);
    }

    wl_list_remove(&m_clientCreated.link);

    if (m_logger)
        wl_protocol_logger_destroy(m_logger);
}

SP<CActor> CActorManager::human() const {
    return m_actors.front();
}

SP<CActor> CActorManager::current() const {
    const auto ACTOR = m_current.lock();
    return ACTOR ? ACTOR : human();
}

SP<CActor> CActorManager::actorFor(wl_client* client) const {
    const auto IT = m_clients.find(client);
    if (IT == m_clients.end())
        return human();

    const auto ACTOR = IT->second->actor.lock();
    return ACTOR ? ACTOR : human();
}

SP<CActor> CActorManager::actorFor(PHLMONITOR monitor) const {
    for (auto const& a : m_actors | std::views::drop(1)) {
        if (monitor && a->m_name == monitor->m_name)
            return a;
    }

    return human();
}

SP<CActor> CActorManager::actorFor(PHLWINDOW window) const {
    const auto SURFACE = window ? window->wlSurface() : nullptr;
    return actorFor(SURFACE && SURFACE->resource() ? SURFACE->resource()->client() : nullptr);
}

std::optional<std::string> CActorManager::workspaceFor(PHLMONITOR monitor) const {
    const auto ACTOR = actorFor(monitor);
    return ACTOR->isHuman() ? std::nullopt : std::optional{"name:" + ACTOR->m_name};
}

bool CActorManager::isOthersSelection(const SP<IDataSource>& source) const {
    const auto CURRENT = current();
    return source && std::ranges::any_of(m_actors, [&](const auto& a) {
               return a != CURRENT && (a->m_seat->m_selection.currentSelection == source || a->m_seat->m_selection.currentPrimarySelection == source);
           });
}

bool CActorManager::hides(const wl_client* client, PHLWINDOW window) const {
    return actorFor(const_cast<wl_client*>(client)) != actorFor(window);
}

bool CActorManager::hides(const wl_client* client, const wl_global* global) const {
    const auto ACTOR = actorFor(const_cast<wl_client*>(client));

    for (auto const& [_, output] : PROTO::outputs) {
        if (output->getGlobal() == global)
            return actorFor(output->m_monitor.lock()) != ACTOR;
    }

    if (ACTOR->isHuman())
        return false;

    const std::array MACHINE = {PROTO::sessionLock->getGlobal(), PROTO::outputManagement->getGlobal(), PROTO::inputCapture->getGlobal(), PROTO::extWorkspace->getGlobal()};
    return std::ranges::contains(MACHINE, global) || std::ranges::any_of(PROTO::lease, [global](const auto& lease) { return lease.second->getGlobal() == global; });
}

void CActorManager::createMonitors() {
    const auto IMPLEMENTATIONS = g_pCompositor->m_aqBackend->getImplementations();
    const auto HEADLESS        = std::ranges::find_if(IMPLEMENTATIONS, [](const auto& i) { return i->type() == Aquamarine::AQ_BACKEND_HEADLESS; });
    if (HEADLESS == IMPLEMENTATIONS.end()) {
        LOG(Log::ERR, "[actor] no headless backend for the agents' monitors");
        return;
    }

    for (auto const& a : m_actors | std::views::drop(1)) {
        CScope scope{a};
        if (!(*HEADLESS)->createOutput(a->m_name))
            LOG(Log::ERR, "[actor] failed to create the monitor of {}", a->m_name);
    }
}

void CActorManager::onClientCreated(wl_listener*, void* data) {
    const auto CLIENT = sc<wl_client*>(data);
    auto&      c      = mgr()->m_clients[CLIENT];

    c                     = makeUnique<SClient>();
    c->destroy.notify     = onClientDestroy;
    c->destroyLate.notify = onClientDestroyLate;
    wl_client_add_destroy_listener(CLIENT, &c->destroy);
    wl_client_add_destroy_late_listener(CLIENT, &c->destroyLate);
}

// a client's resources go away between these two, so they go away as the client's actor
void CActorManager::onClientDestroy(wl_listener*, void* data) {
    const auto& c = mgr()->m_clients.at(sc<wl_client*>(data));

    c->previous      = mgr()->m_current;
    mgr()->m_current = c->actor;
}

void CActorManager::onClientDestroyLate(wl_listener*, void* data) {
    const auto IT = mgr()->m_clients.find(sc<wl_client*>(data));

    mgr()->m_current = IT->second->previous;
    mgr()->m_clients.erase(IT);
}

void CActorManager::onRequest(void*, wl_protocol_logger_type type, const wl_protocol_logger_message* message) {
    if (type == WL_PROTOCOL_LOGGER_REQUEST)
        mgr()->m_current = mgr()->actorFor(wl_resource_get_client(message->resource));
}

CScope::CScope(SP<CActor> actor) : m_previous(mgr()->m_current) {
    mgr()->m_current = actor;
}

CScope::~CScope() {
    mgr()->m_current = m_previous;
}
