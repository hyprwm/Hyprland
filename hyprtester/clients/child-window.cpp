#include <print>
#include <poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/math/Vector2D.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;

struct SWlState {
    wl_display*                  display;
    CSharedPointer<CCWlRegistry> registry;

    // protocols
    CSharedPointer<CCWlCompositor> wlCompositor;
    CSharedPointer<CCWlSeat>       wlSeat;
    CSharedPointer<CCWlShm>        wlShm;
    CSharedPointer<CCXdgWmBase>    xdgShell;

    // shm/buffer stuff
    CSharedPointer<CCWlShmPool> shmPool;
    CSharedPointer<CCWlBuffer>  shmBuf;
    CSharedPointer<CCWlBuffer>  shmBuf2;
    int                         shmFd            = 0;
    size_t                      shmBufSize       = 0;
    bool                        xrgb8888_support = false;

    // surface/toplevel stuff
    CSharedPointer<CCWlSurface>   surf;
    CSharedPointer<CCXdgSurface>  xdgSurf;
    CSharedPointer<CCXdgToplevel> xdgToplevel;
    Vector2D                      geom;

    // pointer
    CSharedPointer<CCWlPointer> pointer;
    uint32_t                    enterSerial = 0;
};

bool debug, shouldExit, started;

template <typename... Args>
//NOLINTNEXTLINE
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::string text = std::vformat(fmt.get(), std::make_format_args(args...));
    std::println("{}", text);
    std::fflush(stdout);
}

template <typename... Args>
//NOLINTNEXTLINE
static void debugLog(std::format_string<Args...> fmt, Args&&... args) {
    std::string text = std::vformat(fmt.get(), std::make_format_args(args...));
    if (!debug)
        return;
    std::println("{}", text);
    std::fflush(stdout);
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

static bool createShm(SWlState& state, Vector2D geom) {
    if (!state.xrgb8888_support)
        return false;

    size_t stride = geom.x * 4;
    size_t size   = geom.y * stride;
    if (!state.shmPool) {
        const char* name = "/wl-shm-pointer-warp";
        state.shmFd      = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (state.shmFd < 0)
            return false;

        if (shm_unlink(name) < 0 || ftruncate(state.shmFd, size * 2) < 0) {
            close(state.shmFd);
            return false;
        }

        state.shmPool = makeShared<CCWlShmPool>(state.wlShm->sendCreatePool(state.shmFd, size * 2));
        if (!state.shmPool->resource()) {
            close(state.shmFd);
            state.shmFd = -1;
            state.shmPool.reset();
            return false;
        }
        state.shmBufSize = size;
    } else if (size > state.shmBufSize) {
        if (ftruncate(state.shmFd, size) < 0) {
            close(state.shmFd);
            state.shmFd = -1;
            state.shmPool.reset();
            return false;
        }

        state.shmPool->sendResize(size * 2);
        state.shmBufSize = size;
    }

    auto buf = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(0, geom.x, geom.y, stride, WL_SHM_FORMAT_XRGB8888));
    if (!buf->resource())
        return false;

    if (state.shmBuf) {
        state.shmBuf->sendDestroy();
        state.shmBuf.reset();
    }
    state.shmBuf = buf;

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

    state.xdgToplevel->setClose([&](CCXdgToplevel* p) { exit(0); });

    state.xdgToplevel->setConfigure([&](CCXdgToplevel* p, int32_t w, int32_t h, wl_array* arr) {
        state.geom = {1280, 720};

        if (!createShm(state, state.geom))
            exit(-1);
    });

    state.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        if (!state.shmBuf)
            debugLog("xdgSurf configure but no buf made yet?");

        state.xdgSurf->sendSetWindowGeometry(0, 0, state.geom.x, state.geom.y);
        state.surf->sendAttach(state.shmBuf.get(), 0, 0);
        state.surf->sendCommit();

        state.xdgSurf->sendAckConfigure(serial);

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    state.xdgToplevel->sendSetTitle("child-test parent");
    state.xdgToplevel->sendSetAppId("child-test-parent");

    state.surf->sendAttach(nullptr, 0, 0);
    state.surf->sendCommit();

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

    state.pointer->setLeave([&](CCWlPointer* p, uint32_t serial, wl_proxy* surf) { debugLog("Got pointer leave event, serial {}", serial); });

    state.pointer->setMotion([&](CCWlPointer* p, uint32_t serial, wl_fixed_t x, wl_fixed_t y) { debugLog("Got pointer motion event, serial {}, x {}, y {}", serial, x, y); });

    return true;
}

struct SChildWindow {
    CSharedPointer<CCWlSurface>   surface;
    CSharedPointer<CCXdgSurface>  xSurface;
    CSharedPointer<CCXdgToplevel> toplevel;
};

static void parseRequest(SWlState& state, std::string str, SChildWindow& window) {
    if (str.starts_with("exit")) {
        shouldExit = true;
        return;
    }

    size_t index = str.find_first_of('\n');
    str          = str.substr(0, index);

    if (str == "toplevel") {
        window.surface  = makeShared<CCWlSurface>(state.wlCompositor->sendCreateSurface());
        window.xSurface = makeShared<CCXdgSurface>(state.xdgShell->sendGetXdgSurface(window.surface->resource()));

        window.xSurface->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
            if (!state.shmBuf)
                debugLog("xdgSurf configure but no buf made yet?");

            window.xSurface->sendSetWindowGeometry(0, 0, state.geom.x, state.geom.y);
            window.surface->sendAttach(state.shmBuf2.get(), 0, 0);
            window.surface->sendCommit();

            window.xSurface->sendAckConfigure(serial);
        });

        window.toplevel = makeShared<CCXdgToplevel>(window.xSurface->sendGetToplevel());

        window.toplevel->setConfigure([&](CCXdgToplevel* p, int32_t w, int32_t h, wl_array* arr) {
            size_t stride = 1280 * 4;
            size_t size   = 720 * stride;

            auto   buf = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(size, state.geom.x, state.geom.y, stride, WL_SHM_FORMAT_XRGB8888));
            if (!buf->resource())
                clientLog("Failed to create child buffer");

            if (state.shmBuf2) {
                state.shmBuf2->sendDestroy();
                state.shmBuf2.reset();
            }
            state.shmBuf2 = buf;
        });

        window.toplevel->sendSetTitle("child-test child");
        window.toplevel->sendSetAppId("child-test-child");
        window.toplevel->sendSetParent(state.xdgToplevel.get());

        window.surface->sendAttach(nullptr, 0, 0);
        window.surface->sendCommit();
        clientLog("child started");
        return;
    }
}

int main(int argc, char** argv) {
    if (argc != 1 && argc != 2)
        clientLog("Only the \"--debug\" switch is allowed, it turns on debug logs.");

    if (argc == 2 && std::string{argv[1]} == "--debug")
        debug = true;

    SWlState     state;
    SChildWindow window;

    // WAYLAND_DISPLAY env should be set to the correct one
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

    struct pollfd fds[2] = {{.fd = wl_display_get_fd(state.display), .events = POLLIN | POLLOUT}, {.fd = STDIN_FILENO, .events = POLLIN}};
    while (!shouldExit && poll(fds, 2, 0) != -1) {
        if (fds[0].revents & POLLIN) {
            wl_display_flush(state.display);

            if (wl_display_prepare_read(state.display) == 0) {
                wl_display_read_events(state.display);
                wl_display_dispatch_pending(state.display);
            } else
                wl_display_dispatch(state.display);

            int ret = 0;
            do {
                ret = wl_display_dispatch_pending(state.display);
                wl_display_flush(state.display);
            } while (ret > 0);
        }

        if (fds[1].revents & POLLIN) {
            ssize_t bytesRead = read(fds[1].fd, readBuf.data(), 1023);
            if (bytesRead == -1)
                continue;
            readBuf[bytesRead] = 0;

            parseRequest(state, std::string{readBuf.data()}, window);
        }
    }

    wl_display* display = state.display;
    state               = {};
    window              = {};

    wl_display_disconnect(display);
    return 0;
}
