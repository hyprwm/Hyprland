#include <algorithm>
#include <array>
#include <cerrno>
#include <format>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <utility>
#include <fcntl.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;
using Hyprutils::OS::CFileDescriptor;

struct SShmState {
    CFileDescriptor             fd;
    CSharedPointer<CCWlShmPool> pool;
    CSharedPointer<CCWlBuffer>  buffer;
    size_t                      size = 0;
};

struct SChildState {
    SShmState                     shm;
    CSharedPointer<CCWlSurface>   surf;
    CSharedPointer<CCXdgSurface>  xdgSurf;
    CSharedPointer<CCXdgToplevel> xdgToplevel;
    Vector2D                      geom    = {240, 160};
    bool                          started = false;
};

struct SWlState {
    wl_display*                    display = nullptr;
    CSharedPointer<CCWlRegistry>   registry;

    CSharedPointer<CCWlCompositor> wlCompositor;
    CSharedPointer<CCWlSeat>       wlSeat;
    CSharedPointer<CCWlShm>        wlShm;
    CSharedPointer<CCXdgWmBase>    xdgShell;

    SShmState                      shm;
    bool                           xrgb8888_support = false;

    CSharedPointer<CCWlSurface>    surf;
    CSharedPointer<CCXdgSurface>   xdgSurf;
    CSharedPointer<CCXdgToplevel>  xdgToplevel;
    Vector2D                       geom = {500, 400};

    CSharedPointer<CCWlPointer>    pointer;
    uint32_t                       enterSerial        = 0;
    uint32_t                       lastButtonSerial   = 0;
    bool                           requestActive      = false;
    bool                           resizing           = false;
    bool                           fullscreen         = false;
    uint32_t                       leaveAfterRequest  = 0;
    uint32_t                       motionAfterRequest = 0;
    uint32_t                       buttonPresses      = 0;

    std::optional<SChildState>     child;
};

static bool debug, started, shouldExit;

template <typename... Args>
//NOLINTNEXTLINE
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

template <typename... Args>
//NOLINTNEXTLINE
static void debugLog(std::format_string<Args...> fmt, Args&&... args) {
    if (!debug)
        return;

    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

static bool stateArrayContains(wl_array* arr, uint32_t state) {
    if (!arr || !arr->data)
        return false;

    const auto* states = static_cast<const uint32_t*>(arr->data);
    for (size_t i = 0; i < arr->size / sizeof(uint32_t); ++i) {
        if (states[i] == state)
            return true;
    }

    return false;
}

static std::optional<xdgToplevelResizeEdge> edgeFromString(const std::string& edge) {
    if (edge == "none")
        return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
    if (edge == "top")
        return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    if (edge == "bottom")
        return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    if (edge == "left")
        return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    if (edge == "right")
        return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    if (edge == "top_left")
        return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
    if (edge == "top_right")
        return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
    if (edge == "bottom_left")
        return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
    if (edge == "bottom_right")
        return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;

    return std::nullopt;
}

static bool bindRegistry(SWlState& state) {
    state.registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(state.display));

    state.registry->setGlobal([&](CCWlRegistry* r, uint32_t id, const char* name, uint32_t version) {
        const std::string NAME = name;
        if (NAME == "wl_compositor") {
            debugLog("  > binding to global: {} (version {}) with id {}", name, version, id);
            state.wlCompositor = makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_compositor_interface, 6));
        } else if (NAME == "wl_shm") {
            debugLog("  > binding to global: {} (version {}) with id {}", name, version, id);
            state.wlShm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_shm_interface, 1));
        } else if (NAME == "wl_seat") {
            debugLog("  > binding to global: {} (version {}) with id {}", name, version, id);
            state.wlSeat = makeShared<CCWlSeat>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_seat_interface, 9));
        } else if (NAME == "xdg_wm_base") {
            debugLog("  > binding to global: {} (version {}) with id {}", name, version, id);
            state.xdgShell = makeShared<CCXdgWmBase>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &xdg_wm_base_interface, 1));
        }
    });
    state.registry->setGlobalRemove([](CCWlRegistry* r, uint32_t id) { debugLog("Global {} removed", id); });

    wl_display_roundtrip(state.display);

    if (!state.wlCompositor || !state.wlShm || !state.wlSeat || !state.xdgShell) {
        clientLog("Failed to get protocols from Hyprland");
        return false;
    }

    return true;
}

static bool createShm(SWlState& state, SShmState& shm, Vector2D geom) {
    if (!state.xrgb8888_support)
        return false;

    const int width  = std::max(1, sc<int>(geom.x));
    const int height = std::max(1, sc<int>(geom.y));
    size_t    stride = width * 4;
    size_t    size   = height * stride;

    if (!shm.pool) {
        const std::string name = std::format("/wl-shm-xdg-interactive-{}", getpid());
        shm.fd                 = CFileDescriptor{shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
        if (!shm.fd.isValid())
            return false;

        if (shm_unlink(name.c_str()) < 0 || ftruncate(shm.fd.get(), size) < 0) {
            shm.fd.reset();
            return false;
        }

        shm.pool = makeShared<CCWlShmPool>(state.wlShm->sendCreatePool(shm.fd.get(), size));
        if (!shm.pool->resource()) {
            shm.fd.reset();
            shm.pool.reset();
            return false;
        }

        shm.size = size;
    } else if (size > shm.size) {
        if (ftruncate(shm.fd.get(), size) < 0) {
            shm.fd.reset();
            shm.pool.reset();
            return false;
        }

        shm.pool->sendResize(size);
        shm.size = size;
    }

    auto buf = makeShared<CCWlBuffer>(shm.pool->sendCreateBuffer(0, width, height, stride, WL_SHM_FORMAT_XRGB8888));
    if (!buf->resource())
        return false;

    if (shm.buffer) {
        shm.buffer->sendDestroy();
        shm.buffer.reset();
    }

    shm.buffer = buf;

    return true;
}

static bool setupToplevel(SWlState& state) {
    state.wlShm->setFormat([&](CCWlShm* p, uint32_t format) {
        if (format == WL_SHM_FORMAT_XRGB8888)
            state.xrgb8888_support = true;
    });

    state.xdgShell->setPing([&](CCXdgWmBase* p, uint32_t serial) { state.xdgShell->sendPong(serial); });

    state.surf = makeShared<CCWlSurface>(state.wlCompositor->sendCreateSurface());
    if (!state.surf->resource())
        return false;

    state.xdgSurf = makeShared<CCXdgSurface>(state.xdgShell->sendGetXdgSurface(state.surf->resource()));
    if (!state.xdgSurf->resource())
        return false;

    state.xdgToplevel = makeShared<CCXdgToplevel>(state.xdgSurf->sendGetToplevel());
    if (!state.xdgToplevel->resource())
        return false;

    state.xdgToplevel->setClose([&](CCXdgToplevel* p) { shouldExit = true; });

    state.xdgToplevel->setConfigure([&](CCXdgToplevel* p, int32_t w, int32_t h, wl_array* arr) {
        if (w > 0)
            state.geom.x = w;
        if (h > 0)
            state.geom.y = h;

        if (!createShm(state, state.shm, state.geom))
            exit(-1);

        const bool RESIZING = stateArrayContains(arr, XDG_TOPLEVEL_STATE_RESIZING);
        if (started && RESIZING != state.resizing)
            clientLog("configure resizing={}", RESIZING ? 1 : 0);

        state.resizing   = RESIZING;
        state.fullscreen = stateArrayContains(arr, XDG_TOPLEVEL_STATE_FULLSCREEN);
    });

    state.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        if (!state.shm.buffer)
            debugLog("xdgSurf configure but no buf made yet?");

        state.xdgSurf->sendSetWindowGeometry(0, 0, state.geom.x, state.geom.y);
        state.xdgSurf->sendAckConfigure(serial);
        state.surf->sendAttach(state.shm.buffer.get(), 0, 0);
        state.surf->sendCommit();

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    state.xdgToplevel->sendSetTitle("xdg interactive test client");
    state.xdgToplevel->sendSetAppId("xdg-interactive");

    state.surf->sendAttach(nullptr, 0, 0);
    state.surf->sendCommit();

    return true;
}

static bool setupChild(SWlState& state) {
    auto& child = state.child.emplace();

    child.surf = makeShared<CCWlSurface>(state.wlCompositor->sendCreateSurface());
    if (!child.surf->resource())
        return false;

    child.xdgSurf = makeShared<CCXdgSurface>(state.xdgShell->sendGetXdgSurface(child.surf->resource()));
    if (!child.xdgSurf->resource())
        return false;

    child.xdgToplevel = makeShared<CCXdgToplevel>(child.xdgSurf->sendGetToplevel());
    if (!child.xdgToplevel->resource())
        return false;

    child.xdgToplevel->setClose([](CCXdgToplevel*) { shouldExit = true; });
    child.xdgToplevel->setConfigure([&child](CCXdgToplevel*, int32_t w, int32_t h, wl_array*) {
        if (w > 0)
            child.geom.x = w;
        if (h > 0)
            child.geom.y = h;
    });

    child.xdgSurf->setConfigure([&state, &child](CCXdgSurface*, uint32_t serial) {
        if (!createShm(state, child.shm, child.geom)) {
            clientLog("Failed to create child buffer");
            shouldExit = true;
            return;
        }

        child.xdgSurf->sendSetWindowGeometry(0, 0, sc<int32_t>(child.geom.x), sc<int32_t>(child.geom.y));
        child.xdgSurf->sendAckConfigure(serial);
        child.surf->sendAttach(child.shm.buffer.get(), 0, 0);
        child.surf->sendCommit();

        if (!child.started) {
            child.started = true;
            clientLog("child started");
        }
    });

    child.xdgToplevel->sendSetTitle("xdg interactive child test client");
    child.xdgToplevel->sendSetAppId("xdg-interactive-child");
    child.xdgToplevel->sendSetParent(state.xdgToplevel.get());
    child.surf->sendCommit();

    return true;
}

static bool setupSeat(SWlState& state) {
    state.pointer = makeShared<CCWlPointer>(state.wlSeat->sendGetPointer());
    if (!state.pointer->resource())
        return false;

    state.pointer->setEnter([&](CCWlPointer* p, uint32_t serial, wl_proxy* surf, wl_fixed_t x, wl_fixed_t y) {
        debugLog("Got pointer enter event, serial {}, x {}, y {}", serial, x, y);
        state.enterSerial = serial;
    });

    state.pointer->setLeave([&](CCWlPointer* p, uint32_t serial, wl_proxy* surf) {
        debugLog("Got pointer leave event, serial {}", serial);
        if (state.requestActive)
            ++state.leaveAfterRequest;
    });

    state.pointer->setMotion([&](CCWlPointer* p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
        debugLog("Got pointer motion event, time {}, x {}, y {}", time, x, y);
        if (state.requestActive)
            ++state.motionAfterRequest;
    });

    state.pointer->setButton([&](CCWlPointer* p, uint32_t serial, uint32_t time, uint32_t button, wl_pointer_button_state buttonState) {
        debugLog("Got pointer button event, serial {}, button {}, state {}", serial, button, static_cast<uint32_t>(buttonState));
        if (buttonState != WL_POINTER_BUTTON_STATE_PRESSED)
            return;

        state.lastButtonSerial = serial;
        ++state.buttonPresses;
        clientLog("button {}", serial);
    });

    return true;
}

static void resetRequestCounters(SWlState& state) {
    state.requestActive      = true;
    state.leaveAfterRequest  = 0;
    state.motionAfterRequest = 0;
}

static void requestMove(SWlState& state, uint32_t serial) {
    resetRequestCounters(state);
    state.xdgToplevel->sendMove(state.wlSeat->resource(), serial);
    wl_display_flush(state.display);
    clientLog("requested move {}", serial);
}

static void requestResize(SWlState& state, uint32_t serial, xdgToplevelResizeEdge edge) {
    resetRequestCounters(state);
    state.xdgToplevel->sendResize(state.wlSeat->resource(), serial, edge);
    wl_display_flush(state.display);
    clientLog("requested resize {} {}", serial, static_cast<uint32_t>(edge));
}

static void parseRequest(SWlState& state, std::string req) {
    const auto nl = req.find('\n');
    if (nl != std::string::npos)
        req = req.substr(0, nl);

    std::istringstream iss(req);
    std::string        command;
    iss >> command;

    if (command == "exit") {
        shouldExit = true;
    } else if (command == "create-child") {
        if (state.child)
            clientLog("child already exists");
        else if (!setupChild(state)) {
            clientLog("Failed to create child");
            shouldExit = true;
        } else
            wl_display_flush(state.display);
    } else if (command == "move") {
        requestMove(state, state.lastButtonSerial);
    } else if (command == "move-serial") {
        uint32_t serial = 0;
        iss >> serial;
        requestMove(state, serial);
    } else if (command == "resize") {
        std::string edgeName;
        iss >> edgeName;

        const auto EDGE = edgeFromString(edgeName);
        if (!EDGE) {
            clientLog("unknown edge {}", edgeName);
            return;
        }

        requestResize(state, state.lastButtonSerial, *EDGE);
    } else if (command == "resize-serial") {
        uint32_t    serial = 0;
        std::string edgeName;
        iss >> serial >> edgeName;

        const auto EDGE = edgeFromString(edgeName);
        if (!EDGE) {
            clientLog("unknown edge {}", edgeName);
            return;
        }

        requestResize(state, serial, *EDGE);
    } else if (command == "resize-raw") {
        uint32_t edge = 0;
        iss >> edge;
        requestResize(state, state.lastButtonSerial, static_cast<xdgToplevelResizeEdge>(edge));
    } else if (command == "set-maximized" || command == "unset-maximized") {
        if (command == "set-maximized")
            state.xdgToplevel->sendSetMaximized();
        else
            state.xdgToplevel->sendUnsetMaximized();

        // An unchanged-state assertion must wait until the compositor has handled the request.
        if (wl_display_roundtrip(state.display) < 0) {
            shouldExit = true;
            return;
        }

        clientLog("completed {}", command);
    } else if (command == "status") {
        if (wl_display_roundtrip(state.display) < 0) {
            shouldExit = true;
            return;
        }

        clientLog("status leave={} motion_after_request={} last_button={} button_presses={} resizing={} fullscreen={}", state.leaveAfterRequest, state.motionAfterRequest,
                  state.lastButtonSerial, state.buttonPresses, state.resizing ? 1 : 0, state.fullscreen ? 1 : 0);
    }
}

static bool dispatchDisplay(SWlState& state) {
    wl_display_flush(state.display);

    if (wl_display_prepare_read(state.display) == 0) {
        if (wl_display_read_events(state.display) == -1)
            return false;
        if (wl_display_dispatch_pending(state.display) == -1)
            return false;
    } else if (wl_display_dispatch_pending(state.display) == -1)
        return false;

    int ret = 0;
    do {
        ret = wl_display_dispatch_pending(state.display);
        wl_display_flush(state.display);
    } while (ret > 0);

    return ret != -1;
}

int main(int argc, char** argv) {
    if (argc != 1 && argc != 2)
        clientLog("Only the \"--debug\" switch is allowed, it turns on debug logs.");

    if (argc == 2 && std::string{argv[1]} == "--debug")
        debug = true;

    SWlState state;

    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        clientLog("Failed to connect to wayland display");
        return -1;
    }

    if (!bindRegistry(state) || !setupSeat(state) || !setupToplevel(state))
        return -1;

    std::array<char, 1024> readBuf;
    readBuf.fill(0);

    wl_display_flush(state.display);

    struct pollfd fds[2] = {{.fd = wl_display_get_fd(state.display), .events = POLLIN}, {.fd = STDIN_FILENO, .events = POLLIN}};
    while (!shouldExit) {
        const int RET = poll(fds, 2, 50);
        if (RET == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            break;

        if (fds[0].revents & POLLIN && !dispatchDisplay(state))
            break;

        if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
            break;

        if (fds[1].revents & POLLIN) {
            ssize_t bytesRead = read(fds[1].fd, readBuf.data(), readBuf.size() - 1);
            if (bytesRead <= 0)
                continue;
            readBuf[bytesRead] = 0;

            parseRequest(state, std::string{readBuf.data()});
        }
    }

    wl_display* display = state.display;
    state.child.reset();
    state = {};

    wl_display_disconnect(display);
    return 0;
}
