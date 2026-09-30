#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <deque>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>
#include <utility>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <xdg-activation-v1.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <linux/input-event-codes.h>

using namespace Hyprutils::Memory;
using Hyprutils::OS::CFileDescriptor;

struct SConnection {
    wl_display* display = wl_display_connect(nullptr);

    ~SConnection() {
        if (display)
            wl_display_disconnect(display);
    }
};

struct SCompositor {
    wl_compositor* resource = nullptr;

    ~SCompositor() {
        // wl_compositor v6 has no protocol destructor. The generated wrapper
        // would unconditionally send the v7 release request.
        if (resource)
            wl_compositor_destroy(resource);
    }
};

struct SWindow {
    CSharedPointer<CCWlBuffer>    buffer;
    CSharedPointer<CCWlSurface>   surface;
    CSharedPointer<CCXdgSurface>  xdgSurface;
    CSharedPointer<CCXdgToplevel> toplevel;
    int32_t                       width        = 320;
    int32_t                       height       = 240;
    int32_t                       bufferWidth  = 0;
    int32_t                       bufferHeight = 0;
    bool                          submitted    = false;
};

struct SRequest {
    uint32_t                id = 0;
    std::optional<uint32_t> serial;
};

struct SState {
    wl_display*                            display = nullptr;
    CSharedPointer<CCWlRegistry>           registry;
    SCompositor                            compositor;
    CSharedPointer<CCWlShm>                shm;
    CSharedPointer<CCWlSeat>               seat;
    CSharedPointer<CCXdgWmBase>            shell;
    CSharedPointer<CCXdgActivationV1>      activation;
    SWindow                                source;
    SWindow                                target;
    CSharedPointer<CCWlPointer>            pointer;
    CSharedPointer<CCXdgActivationTokenV1> token;
    CSharedPointer<CCWlCallback>           sync;
    std::deque<SRequest>                   requests;
    std::string                            input;
    bool                                   pointerOnSource = false;
    bool                                   ready           = false;
    bool                                   requestFinished = false;
    bool                                   inputClosed     = false;
    bool                                   shouldExit      = false;
    bool                                   failed          = false;
};

template <typename T>
static CSharedPointer<T> wrapResource(wl_proxy* proxy) {
    // Some generated destructors require a non-null proxy.
    return proxy ? makeShared<T>(proxy) : CSharedPointer<T>{};
}

static void fail(SState& state, std::string_view message) {
    std::cerr << "xdg-activation: " << message << '\n';
    state.failed = true;
}

static void updatePointer(SState& state, wl_seat_capability capabilities) {
    if (!(capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        state.pointer.reset();
        state.pointerOnSource = false;
        return;
    }

    if (state.pointer)
        return;

    state.pointer = wrapResource<CCWlPointer>(state.seat->sendGetPointer());
    if (!state.pointer) {
        fail(state, "failed to create pointer");
        return;
    }

    state.pointer->setEnter([&state](CCWlPointer*, uint32_t, wl_proxy* surface, wl_fixed_t, wl_fixed_t) {
        state.pointerOnSource = state.source.surface && surface == state.source.surface->resource();
    });
    state.pointer->setLeave([&state](CCWlPointer*, uint32_t, wl_proxy*) { state.pointerOnSource = false; });
    state.pointer->setButton([&state](CCWlPointer*, uint32_t serial, uint32_t, uint32_t button, wl_pointer_button_state buttonState) {
        if (state.pointerOnSource && button == BTN_LEFT && buttonState == WL_POINTER_BUTTON_STATE_PRESSED)
            std::cout << "button " << serial << '\n' << std::flush;
    });
}

static bool bindRegistry(SState& state) {
    state.registry = wrapResource<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(state.display)));
    if (!state.registry)
        return false;

    state.registry->setGlobal([&state](CCWlRegistry* registry, uint32_t id, const char* name, uint32_t version) {
        const std::string_view NAME     = name;
        auto* const            REGISTRY = rc<wl_registry*>(registry->resource());
        if (NAME == "wl_compositor" && !state.compositor.resource)
            state.compositor.resource = sc<wl_compositor*>(wl_registry_bind(REGISTRY, id, &wl_compositor_interface, std::min(version, 6U)));
        else if (NAME == "wl_shm" && !state.shm && version >= 2)
            state.shm = wrapResource<CCWlShm>(rc<wl_proxy*>(wl_registry_bind(REGISTRY, id, &wl_shm_interface, 2)));
        else if (NAME == "xdg_wm_base" && !state.shell) {
            state.shell = wrapResource<CCXdgWmBase>(rc<wl_proxy*>(wl_registry_bind(REGISTRY, id, &xdg_wm_base_interface, 1)));
            if (state.shell)
                state.shell->setPing([](CCXdgWmBase* shell, uint32_t serial) { shell->sendPong(serial); });
        } else if (NAME == "xdg_activation_v1" && !state.activation)
            state.activation = wrapResource<CCXdgActivationV1>(rc<wl_proxy*>(wl_registry_bind(REGISTRY, id, &xdg_activation_v1_interface, 1)));
        else if (NAME == "wl_seat" && !state.seat) {
            state.seat = wrapResource<CCWlSeat>(rc<wl_proxy*>(wl_registry_bind(REGISTRY, id, &wl_seat_interface, std::min(version, 9U))));
            if (state.seat)
                state.seat->setCapabilities([&state](CCWlSeat*, wl_seat_capability capabilities) { updatePointer(state, capabilities); });
        }
    });

    return wl_display_roundtrip(state.display) >= 0 && state.compositor.resource && state.shm && state.shell && state.activation && state.seat && !state.failed;
}

static bool prepareBuffer(SState& state, SWindow& window) {
    if (window.buffer && window.bufferWidth == window.width && window.bufferHeight == window.height)
        return true;

    if (window.width > std::numeric_limits<int32_t>::max() / 4 / window.height)
        return false;

    const int32_t         STRIDE = window.width * 4;
    const int32_t         SIZE   = STRIDE * window.height;
    const CFileDescriptor FD{memfd_create("xdg-activation", MFD_CLOEXEC)};
    if (FD.get() < 0 || ftruncate(FD.get(), SIZE) < 0)
        return false;

    const auto POOL = wrapResource<CCWlShmPool>(state.shm->sendCreatePool(FD.get(), SIZE));
    if (!POOL)
        return false;

    // Fresh, zero-filled backing gives an opaque black XRGB buffer. Never modify
    // it, so repeated commits do not need to wait for wl_buffer.release.
    auto buffer = wrapResource<CCWlBuffer>(POOL->sendCreateBuffer(0, window.width, window.height, STRIDE, WL_SHM_FORMAT_XRGB8888));
    if (!buffer)
        return false;

    window.buffer       = std::move(buffer);
    window.bufferWidth  = window.width;
    window.bufferHeight = window.height;
    return true;
}

static bool setupWindow(SState& state, SWindow& window, const char* appID) {
    window.surface = wrapResource<CCWlSurface>(rc<wl_proxy*>(wl_compositor_create_surface(state.compositor.resource)));
    if (!window.surface)
        return false;

    window.xdgSurface = wrapResource<CCXdgSurface>(state.shell->sendGetXdgSurface(window.surface->resource()));
    if (!window.xdgSurface)
        return false;

    window.toplevel = wrapResource<CCXdgToplevel>(window.xdgSurface->sendGetToplevel());
    if (!window.toplevel)
        return false;

    window.toplevel->setClose([&state](CCXdgToplevel*) { state.shouldExit = true; });
    window.toplevel->setConfigure([&window](CCXdgToplevel*, int32_t width, int32_t height, wl_array*) {
        if (width > 0)
            window.width = width;
        if (height > 0)
            window.height = height;
    });
    window.xdgSurface->setConfigure([&state, &window](CCXdgSurface*, uint32_t serial) {
        if (!prepareBuffer(state, window)) {
            fail(state, "failed to create SHM buffer");
            return;
        }

        window.xdgSurface->sendAckConfigure(serial);
        window.xdgSurface->sendSetWindowGeometry(0, 0, window.width, window.height);
        window.surface->sendAttach(window.buffer.get(), 0, 0);
        window.surface->sendDamage(0, 0, window.width, window.height);
        window.surface->sendCommit();
        window.submitted = true;

        if (!state.ready && state.source.submitted && state.target.submitted) {
            state.ready = true;
            std::cout << "ready\n" << std::flush;
        }
    });
    window.toplevel->sendSetTitle(appID);
    window.toplevel->sendSetAppId(appID);
    window.surface->sendCommit();
    return true;
}

static bool parseUint(const std::string& text, uint32_t& value) {
    const auto RESULT = std::from_chars(text.data(), text.data() + text.size(), value);
    return RESULT.ec == std::errc{} && RESULT.ptr == text.data() + text.size();
}

static void parseRequest(SState& state, const std::string& line) {
    std::istringstream stream{line};
    std::string        command, id, mode, serial, extra;
    if (!(stream >> command))
        return;

    if (command == "exit" && !(stream >> extra)) {
        state.shouldExit = true;
        return;
    }

    SRequest request;
    if (command != "activate" || !(stream >> id >> mode) || !parseUint(id, request.id)) {
        fail(state, "invalid command");
        return;
    }

    if (mode == "serial") {
        uint32_t value = 0;
        if (!(stream >> serial) || !parseUint(serial, value)) {
            fail(state, "invalid serial");
            return;
        }
        request.serial = value;
    } else if (mode != "missing") {
        fail(state, "invalid activation mode");
        return;
    }

    if (stream >> extra) {
        fail(state, "unexpected command arguments");
        return;
    }

    state.requests.push_back(request);
}

static bool readInput(SState& state) {
    std::array<char, 4096> buffer{};
    const auto             SIZE = read(STDIN_FILENO, buffer.data(), buffer.size());
    if (SIZE < 0)
        return errno == EINTR || errno == EAGAIN;
    if (SIZE == 0) {
        state.inputClosed = true;
        if (!state.input.empty()) {
            parseRequest(state, state.input);
            state.input.clear();
        }
        return true;
    }

    state.input.append(buffer.data(), sc<size_t>(SIZE));
    size_t consumed = 0;
    while (!state.shouldExit && !state.failed) {
        const auto END = state.input.find('\n', consumed);
        if (END == std::string::npos)
            break;
        parseRequest(state, state.input.substr(consumed, END - consumed));
        consumed = END + 1;
    }
    state.input.erase(0, consumed);
    return true;
}

static void advanceRequests(SState& state) {
    // Only called outside dispatch: a callback must outlive its own invocation.
    if (state.requestFinished) {
        state.sync.reset();
        state.token.reset();
        state.requestFinished = false;
    }

    if (!state.ready || state.token || state.requests.empty())
        return;

    const auto REQUEST = state.requests.front();
    state.requests.pop_front();
    state.token = wrapResource<CCXdgActivationTokenV1>(state.activation->sendGetActivationToken());
    if (!state.token) {
        fail(state, "failed to create activation token");
        return;
    }

    state.token->setDone([&state, id = REQUEST.id](CCXdgActivationTokenV1*, const char* token) {
        const bool EMPTY = token[0] == '\0';
        state.activation->sendActivate(token, state.target.surface->resource());
        state.sync = wrapResource<CCWlCallback>(rc<wl_proxy*>(wl_display_sync(state.display)));
        if (!state.sync) {
            fail(state, "failed to create activation sync");
            return;
        }
        state.sync->setDone([&state, id, EMPTY](CCWlCallback*, uint32_t) {
            std::cout << "activated " << id << " empty=" << (EMPTY ? 1 : 0) << '\n' << std::flush;
            state.requestFinished = true;
        });
    });
    if (REQUEST.serial)
        state.token->sendSetSerial(*REQUEST.serial, state.seat->resource());
    state.token->sendCommit();
}

static bool run(SState& state) {
    while (!state.shouldExit && !state.failed) {
        if (wl_display_dispatch_pending(state.display) < 0)
            return false;
        if (state.shouldExit || state.failed)
            break;

        advanceRequests(state);
        if (state.failed)
            break;
        if (state.inputClosed && state.requests.empty() && !state.token)
            return true;

        if (wl_display_prepare_read(state.display) != 0)
            continue;

        std::array<pollfd, 2> fds = {{
            {.fd = wl_display_get_fd(state.display), .events = POLLIN, .revents = 0},
            {.fd = state.inputClosed ? -1 : STDIN_FILENO, .events = POLLIN, .revents = 0},
        }};
        if (wl_display_flush(state.display) < 0) {
            if (errno != EAGAIN) {
                wl_display_cancel_read(state.display);
                return false;
            }
            fds[0].events |= POLLOUT;
        }

        // The harness owns the timeout, including startup and stalled requests.
        const int RESULT = poll(fds.data(), fds.size(), -1);
        if (RESULT < 0) {
            wl_display_cancel_read(state.display);
            if (errno == EINTR)
                continue;
            return false;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_read_events(state.display) < 0)
                return false;
        } else
            wl_display_cancel_read(state.display);

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            return false;
        if (fds[1].revents & (POLLERR | POLLNVAL))
            return false;
        // Drain pipe data even when POLLHUP accompanies POLLIN.
        if ((fds[1].revents & (POLLIN | POLLHUP)) && !readInput(state))
            return false;
    }
    return !state.failed;
}

int main() {
    // Declared first so all protocol wrappers are destroyed before disconnect.
    const SConnection CONNECTION;
    if (!CONNECTION.display) {
        std::cerr << "xdg-activation: failed to connect to Wayland\n";
        return 1;
    }

    SState state;
    state.display = CONNECTION.display;
    if (!bindRegistry(state) || !setupWindow(state, state.source, "xdg-activation-source") || !setupWindow(state, state.target, "xdg-activation-target")) {
        std::cerr << "xdg-activation: failed to initialize protocols or windows\n";
        return 1;
    }

    if (!run(state)) {
        std::cerr << "xdg-activation: client failed\n";
        return 1;
    }
    return 0;
}
