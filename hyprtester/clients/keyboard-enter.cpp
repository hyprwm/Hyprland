#include <cstring>
#include <sys/poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <print>
#include <format>
#include <string>
#include <vector>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/math/Vector2D.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;

struct SWlState {
    wl_display*                    display;
    CSharedPointer<CCWlRegistry>   registry;

    CSharedPointer<CCWlCompositor> wlCompositor;
    CSharedPointer<CCWlSeat>       wlSeat;
    CSharedPointer<CCWlShm>        wlShm;
    CSharedPointer<CCXdgWmBase>    xdgShell;

    CSharedPointer<CCWlShmPool>    shmPool;
    CSharedPointer<CCWlBuffer>     shmBuf;
    int                            shmFd           = -1;
    size_t                         shmBufSize      = 0;
    bool                           xrgb8888Support = false;

    CSharedPointer<CCWlSurface>    surf;
    CSharedPointer<CCXdgSurface>   xdgSurf;
    CSharedPointer<CCXdgToplevel>  xdgToplevel;
    Vector2D                       geom;

    CSharedPointer<CCWlKeyboard>   keyboard;
};

static bool debug, started, shouldExit;

template <typename... Args>
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::println("{}", std::vformat(fmt.get(), std::make_format_args(args...)));
    std::fflush(stdout);
}

template <typename... Args>
static void debugLog(std::format_string<Args...> fmt, Args&&... args) {
    if (!debug)
        return;

    std::println("{}", std::vformat(fmt.get(), std::make_format_args(args...)));
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
    if (!state.xrgb8888Support)
        return false;

    const size_t stride = geom.x * 4;
    const size_t size   = geom.y * stride;

    if (!state.shmPool) {
        const char* name = "/wl-shm-keyboard-enter";
        state.shmFd      = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (state.shmFd < 0)
            return false;

        if (shm_unlink(name) < 0 || ftruncate(state.shmFd, size) < 0) {
            close(state.shmFd);
            return false;
        }

        state.shmPool = makeShared<CCWlShmPool>(state.wlShm->sendCreatePool(state.shmFd, size));
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

        state.shmPool->sendResize(size);
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

static bool setupToplevel(SWlState& state, const std::string& appId) {
    state.wlShm->setFormat([&](CCWlShm* p, uint32_t format) {
        if (format == WL_SHM_FORMAT_XRGB8888)
            state.xrgb8888Support = true;
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

    state.xdgToplevel->setClose([&](CCXdgToplevel* p) { std::exit(0); });
    state.xdgToplevel->setConfigure([&](CCXdgToplevel* p, int32_t w, int32_t h, wl_array* arr) {
        state.geom = {640, 360};

        if (!createShm(state, state.geom))
            std::exit(-1);
    });

    state.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        state.xdgSurf->sendSetWindowGeometry(0, 0, state.geom.x, state.geom.y);
        state.surf->sendAttach(state.shmBuf.get(), 0, 0);
        state.surf->sendCommit();
        state.xdgSurf->sendAckConfigure(serial);

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    state.xdgToplevel->sendSetTitle((appId + " test client").c_str());
    state.xdgToplevel->sendSetAppId(appId.c_str());
    state.surf->sendAttach(nullptr, 0, 0);
    state.surf->sendCommit();

    return true;
}

static std::string formatEnterKeys(wl_array* keys) {
    std::vector<uint32_t> pressed;
    pressed.reserve(keys ? keys->size / sizeof(uint32_t) : 0);

    if (keys && keys->data) {
        const auto* begin = static_cast<const uint32_t*>(keys->data);
        const auto  count = keys->size / sizeof(uint32_t);
        pressed.assign(begin, begin + count);
    }

    std::string out = std::format("enter {}", pressed.size());
    for (const auto key : pressed) {
        out += std::format(" {}", key);
    }

    return out;
}

static bool setupSeat(SWlState& state) {
    state.keyboard = makeShared<CCWlKeyboard>(state.wlSeat->sendGetKeyboard());
    if (!state.keyboard->resource())
        return false;

    state.keyboard->setKeymap([](CCWlKeyboard* p, enum wl_keyboard_keymap_format format, int32_t fd, uint32_t size) {
        if (fd >= 0)
            close(fd);
    });
    state.keyboard->setEnter([](CCWlKeyboard* p, uint32_t serial, wl_proxy* surf, wl_array* keys) { clientLog("{}", formatEnterKeys(keys)); });
    state.keyboard->setLeave([](CCWlKeyboard* p, uint32_t serial, wl_proxy* surf) { debugLog("leave {}", serial); });
    state.keyboard->setKey(
        [](CCWlKeyboard* p, uint32_t serial, uint32_t time, uint32_t key, enum wl_keyboard_key_state state) { debugLog("key {} {}", key, static_cast<uint32_t>(state)); });
    state.keyboard->setModifiers([](CCWlKeyboard* p, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        debugLog("mods dep={} lat={} lock={} grp={}", depressed, latched, locked, group);
    });
    state.keyboard->setRepeatInfo([](CCWlKeyboard* p, int32_t rate, int32_t delay) { debugLog("repeat rate={} delay={}", rate, delay); });

    return true;
}

static void parseRequest(std::string req) {
    if (req.contains("exit"))
        shouldExit = true;
}

int main(int argc, char** argv) {
    std::string appId = "keyboard-enter";
    for (int i = 1; i < argc; ++i) {
        if (std::string{argv[i]} == "--debug")
            debug = true;
        else
            appId = argv[i];
    }

    SWlState state = {
        .display = wl_display_connect(nullptr),
    };

    if (!state.display) {
        clientLog("Wayland connection failed");
        return 1;
    }

    if (!bindRegistry(state))
        return 1;

    if (!setupToplevel(state, appId))
        return 1;

    if (!setupSeat(state))
        return 1;

    while (!shouldExit) {
        wl_display_dispatch_pending(state.display);
        wl_display_flush(state.display);

        struct pollfd fds[2] = {
            {.fd = wl_display_get_fd(state.display), .events = POLLIN},
            {.fd = STDIN_FILENO, .events = POLLIN},
        };

        if (poll(fds, 2, -1) < 0)
            return 1;

        if (fds[0].revents & POLLIN)
            wl_display_dispatch(state.display);

        if (fds[1].revents & POLLIN) {
            std::array<char, 256> buf       = {};
            const auto            bytesRead = read(STDIN_FILENO, buf.data(), buf.size() - 1);
            if (bytesRead > 0)
                parseRequest(std::string{buf.data(), static_cast<size_t>(bytesRead)});
        }
    }

    return 0;
}
