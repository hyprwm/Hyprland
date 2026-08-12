#include <cstring>
#include <sys/poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <print>
#include <format>
#include <string>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/math/Vector2D.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;

struct SState {
    wl_display*                    display;
    CSharedPointer<CCWlRegistry>   registry;
    CSharedPointer<CCWlCompositor> compositor;
    CSharedPointer<CCWlShm>        shm;
    CSharedPointer<CCXdgWmBase>    xdgWm;
    CSharedPointer<CCWlOutput>     output;

    CSharedPointer<CCWlShmPool>    shmPool;
    CSharedPointer<CCWlBuffer>     shmBuf;
    int                            shmFd    = -1;
    size_t                         shmSize  = 0;
    bool                           hasXrgb  = false;

    CSharedPointer<CCWlSurface>    surf;
    CSharedPointer<CCXdgSurface>   xdgSurf;
    CSharedPointer<CCXdgToplevel>  toplevel;
    Vector2D                       geom        = {1280, 720};
    bool                           fullscreen  = false;
};

static bool started    = false;
static bool shouldExit = false;

template <typename... Args>
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::string s = std::format(fmt, std::forward<Args>(args)...);
    std::println("{}", s);
    std::fflush(stdout);
}

static bool bindGlobals(SState& s) {
    s.registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(s.display));

    s.registry->setGlobal([&](CCWlRegistry* r, uint32_t id, const char* iface, uint32_t) {
        const std::string n = iface;
        if (n == "wl_compositor")
            s.compositor = makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), id, &wl_compositor_interface, 6));
        else if (n == "wl_shm")
            s.shm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), id, &wl_shm_interface, 1));
        else if (n == "xdg_wm_base")
            s.xdgWm = makeShared<CCXdgWmBase>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), id, &xdg_wm_base_interface, 1));
        else if (n == "wl_output" && !s.output)
            // only bind the first output; test env has one virtual monitor
            s.output = makeShared<CCWlOutput>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), id, &wl_output_interface, 4));
    });

    s.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});
    wl_display_roundtrip(s.display);

    return s.compositor && s.shm && s.xdgWm && s.output;
}

static bool makeShm(SState& s, Vector2D geom) {
    if (!s.hasXrgb)
        return false;

    size_t stride = (size_t)geom.x * 4;
    size_t size   = (size_t)geom.y * stride;

    if (!s.shmPool) {
        const char* name = "/wl-shm-fs-mon";
        s.shmFd          = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (s.shmFd < 0)
            return false;
        if (shm_unlink(name) < 0 || ftruncate(s.shmFd, size) < 0) {
            close(s.shmFd);
            return false;
        }
        s.shmPool = makeShared<CCWlShmPool>(s.shm->sendCreatePool(s.shmFd, size));
        s.shmSize = size;
    } else if (size > s.shmSize) {
        if (ftruncate(s.shmFd, size) < 0)
            return false;
        s.shmPool->sendResize(size);
        s.shmSize = size;
    }

    if (s.shmBuf) {
        s.shmBuf->sendDestroy();
        s.shmBuf.reset();
    }
    s.shmBuf = makeShared<CCWlBuffer>(s.shmPool->sendCreateBuffer(0, geom.x, geom.y, stride, WL_SHM_FORMAT_XRGB8888));
    return s.shmBuf && s.shmBuf->resource();
}

static bool setupSurface(SState& s) {
    s.shm->setFormat([&](CCWlShm*, uint32_t fmt) {
        if (fmt == WL_SHM_FORMAT_XRGB8888)
            s.hasXrgb = true;
    });

    s.xdgWm->setPing([&](CCXdgWmBase* p, uint32_t serial) { p->sendPong(serial); });

    s.surf     = makeShared<CCWlSurface>(s.compositor->sendCreateSurface());
    s.xdgSurf  = makeShared<CCXdgSurface>(s.xdgWm->sendGetXdgSurface(s.surf->resource()));
    s.toplevel = makeShared<CCXdgToplevel>(s.xdgSurf->sendGetToplevel());

    if (!s.surf->resource() || !s.xdgSurf->resource() || !s.toplevel->resource())
        return false;

    s.toplevel->setClose([](CCXdgToplevel*) { exit(0); });

    s.toplevel->setConfigure([&](CCXdgToplevel*, int32_t w, int32_t h, wl_array* states) {
        if (w > 0 && h > 0)
            s.geom = {(double)w, (double)h};

        // parse the state array to detect fullscreen
        s.fullscreen  = false;
        auto stateSpan = std::span<const uint32_t>(
            static_cast<const uint32_t*>(states->data),
            states->size / sizeof(uint32_t)
        );
        for (uint32_t st : stateSpan) {

            if (st == XDG_TOPLEVEL_STATE_FULLSCREEN) {
                s.fullscreen = true;
                break;
            }
        }   

        if (!makeShm(s, s.geom))
            exit(-1);
    });

    s.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        if (!s.shmBuf)
            return;

        p->sendSetWindowGeometry(0, 0, s.geom.x, s.geom.y);
        s.surf->sendAttach(s.shmBuf.get(), 0, 0);
        s.surf->sendCommit();
        p->sendAckConfigure(serial);

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    s.toplevel->sendSetTitle("fullscreen-with-monitor client");
    s.toplevel->sendSetAppId("fullscreen-with-monitor");

    s.surf->sendAttach(nullptr, 0, 0);
    s.surf->sendCommit();

    return true;
}

static void handleCmd(SState& s, std::string_view cmd) {
    if (cmd.starts_with("unfullscreen")) {
        s.toplevel->sendUnsetFullscreen();
        wl_display_flush(s.display);
    } else if (cmd.starts_with("fullscreen")) {
        s.toplevel->sendSetFullscreen(s.output->resource());
        wl_display_flush(s.display);
    } else if (cmd.starts_with("get")) {
        clientLog("{}", s.fullscreen ? "1" : "0");
    } else if (cmd.starts_with("exit")) {
        shouldExit = true;
    }
}

int main() {
    SState s;

    s.display = wl_display_connect(nullptr);
    if (!s.display) {
        clientLog("connect failed");
        return -1;
    }

    if (!bindGlobals(s)) {
        clientLog("failed to bind globals (no wl_output?)");
        return -1;
    }

    if (!setupSurface(s)) {
        clientLog("surface setup failed");
        return -1;
    }

    std::array<char, 1024> buf;
    buf.fill(0);
    wl_display_flush(s.display);

    struct pollfd fds[2] = {
        {.fd = wl_display_get_fd(s.display), .events = POLLIN | POLLOUT},
        {.fd = STDIN_FILENO,                 .events = POLLIN}
    };

    while (!shouldExit && poll(fds, 2, 0) != -1) {
        if (fds[0].revents & POLLIN) {
            wl_display_flush(s.display);
            if (wl_display_prepare_read(s.display) == 0) {
                wl_display_read_events(s.display);
                wl_display_dispatch_pending(s.display);
            } else
                wl_display_dispatch(s.display);

            int ret;
            do {
                ret = wl_display_dispatch_pending(s.display);
                wl_display_flush(s.display);
            } while (ret > 0);
        }

        if (fds[1].revents & POLLIN) {
            ssize_t n = read(fds[1].fd, buf.data(), buf.size() - 1);
            if (n > 0) {
                buf[n] = 0;
                handleCmd(s, buf.data());
            }
        }
    }

    wl_display* display = s.display;
    s                   = {};
    wl_display_disconnect(display);
    return 0;
}