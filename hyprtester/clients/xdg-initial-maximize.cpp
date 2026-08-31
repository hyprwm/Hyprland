#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;

enum class eMaximizeTiming : uint8_t {
    BEFORE_INITIAL_COMMIT,
    BEFORE_MAP,
};

struct SState {
    wl_display*                    display = nullptr;
    CSharedPointer<CCWlRegistry>   registry;
    CSharedPointer<CCWlCompositor> compositor;
    CSharedPointer<CCWlShm>        shm;
    CSharedPointer<CCXdgWmBase>    xdgWm;

    CSharedPointer<CCWlShmPool>    shmPool;
    CSharedPointer<CCWlBuffer>     shmBuffer;
    int                            shmFD   = -1;
    size_t                         shmSize = 0;
    bool                           hasXRGB = false;

    CSharedPointer<CCWlSurface>    surface;
    CSharedPointer<CCXdgSurface>   xdgSurface;
    CSharedPointer<CCXdgToplevel>  toplevel;
    Vector2D                       geometry = {640, 480};

    eMaximizeTiming                timing     = eMaximizeTiming::BEFORE_INITIAL_COMMIT;
    bool                           requested  = false;
    bool                           shouldExit = false;
};

static void clientLog(const std::string& message) {
    std::println("{}", message);
    std::fflush(stdout);
}

static std::optional<eMaximizeTiming> parseTiming(int argc, char** argv) {
    if (argc != 2)
        return std::nullopt;

    const std::string ARG = argv[1];
    if (ARG == "before-initial-commit")
        return eMaximizeTiming::BEFORE_INITIAL_COMMIT;
    if (ARG == "before-map")
        return eMaximizeTiming::BEFORE_MAP;

    return std::nullopt;
}

static bool bindGlobals(SState& state) {
    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(state.display)));

    state.registry->setGlobal([&state](CCWlRegistry* registry, uint32_t id, const char* interface, uint32_t version) {
        const std::string INTERFACE = interface;
        if (INTERFACE == "wl_compositor")
            state.compositor =
                makeShared<CCWlCompositor>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &wl_compositor_interface, std::min(version, 6U))));
        else if (INTERFACE == "wl_shm")
            state.shm = makeShared<CCWlShm>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &wl_shm_interface, 1)));
        else if (INTERFACE == "xdg_wm_base")
            state.xdgWm = makeShared<CCXdgWmBase>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &xdg_wm_base_interface, 1)));
    });
    state.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});

    wl_display_roundtrip(state.display);
    return state.compositor && state.shm && state.xdgWm;
}

static bool createBuffer(SState& state) {
    if (!state.hasXRGB)
        return false;

    const int    WIDTH  = std::max(1, sc<int>(state.geometry.x));
    const int    HEIGHT = std::max(1, sc<int>(state.geometry.y));
    const size_t STRIDE = WIDTH * 4;
    const size_t SIZE   = HEIGHT * STRIDE;

    if (!state.shmPool) {
        const std::string NAME = std::format("/wl-shm-xdg-initial-maximize-{}", getpid());
        state.shmFD            = shm_open(NAME.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (state.shmFD < 0)
            return false;

        if (shm_unlink(NAME.c_str()) < 0) {
            close(state.shmFD);
            state.shmFD = -1;
            return false;
        }

        if (ftruncate(state.shmFD, SIZE) < 0)
            return false;

        state.shmPool = makeShared<CCWlShmPool>(state.shm->sendCreatePool(state.shmFD, SIZE));
        state.shmSize = SIZE;
    } else if (SIZE > state.shmSize) {
        if (ftruncate(state.shmFD, SIZE) < 0)
            return false;

        state.shmPool->sendResize(SIZE);
        state.shmSize = SIZE;
    }

    if (!state.shmPool || !state.shmPool->resource())
        return false;

    if (state.shmBuffer) {
        state.shmBuffer->sendDestroy();
        state.shmBuffer.reset();
    }

    state.shmBuffer = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(0, WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888));
    return state.shmBuffer && state.shmBuffer->resource();
}

static void requestMaximize(SState& state) {
    state.toplevel->sendSetMaximized();
    state.requested = true;
}

static bool setupSurface(SState& state) {
    state.shm->setFormat([&state](CCWlShm*, uint32_t format) {
        if (format == WL_SHM_FORMAT_XRGB8888)
            state.hasXRGB = true;
    });
    state.xdgWm->setPing([&state](CCXdgWmBase*, uint32_t serial) { state.xdgWm->sendPong(serial); });

    state.surface    = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    state.xdgSurface = makeShared<CCXdgSurface>(state.xdgWm->sendGetXdgSurface(state.surface->resource()));
    state.toplevel   = makeShared<CCXdgToplevel>(state.xdgSurface->sendGetToplevel());
    if (!state.surface->resource() || !state.xdgSurface->resource() || !state.toplevel->resource())
        return false;

    state.toplevel->setClose([&state](CCXdgToplevel*) { state.shouldExit = true; });
    state.toplevel->setConfigure([&state](CCXdgToplevel*, int32_t width, int32_t height, wl_array*) {
        if (width > 0)
            state.geometry.x = width;
        if (height > 0)
            state.geometry.y = height;

        if (!createBuffer(state))
            state.shouldExit = true;
    });
    state.xdgSurface->setConfigure([&state](CCXdgSurface*, uint32_t serial) {
        if (!state.shmBuffer) {
            state.shouldExit = true;
            return;
        }

        state.xdgSurface->sendAckConfigure(serial);
        if (state.timing == eMaximizeTiming::BEFORE_MAP && !state.requested)
            requestMaximize(state);

        state.xdgSurface->sendSetWindowGeometry(0, 0, state.geometry.x, state.geometry.y);
        state.surface->sendAttach(state.shmBuffer.get(), 0, 0);
        state.surface->sendCommit();
    });

    state.toplevel->sendSetTitle("XDG initial maximize test client");
    state.toplevel->sendSetAppId("xdg-initial-maximize");

    if (state.timing == eMaximizeTiming::BEFORE_INITIAL_COMMIT)
        requestMaximize(state);

    state.surface->sendAttach(nullptr, 0, 0);
    state.surface->sendCommit();
    return true;
}

static void disconnect(SState& state) {
    const auto DISPLAY = state.display;
    if (!DISPLAY)
        return;

    state.toplevel.reset();
    state.xdgSurface.reset();
    state.surface.reset();
    state.shmBuffer.reset();
    state.shmPool.reset();
    state.xdgWm.reset();
    state.shm.reset();
    state.compositor.reset();
    state.registry.reset();

    if (state.shmFD >= 0)
        close(state.shmFD);

    state.display = nullptr;
    wl_display_flush(DISPLAY);
    wl_display_disconnect(DISPLAY);
}

int main(int argc, char** argv) {
    const auto TIMING = parseTiming(argc, argv);
    if (!TIMING) {
        clientLog("usage: xdg-initial-maximize <before-initial-commit|before-map>");
        return 1;
    }

    SState state{.timing = *TIMING};
    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        clientLog("failed to connect to Wayland display");
        return 1;
    }

    if (!bindGlobals(state) || !setupSurface(state)) {
        clientLog("failed to set up test surface");
        disconnect(state);
        return 1;
    }

    wl_display_flush(state.display);
    while (!state.shouldExit && wl_display_dispatch(state.display) != -1) {
        ;
    }

    disconnect(state);
    return 0;
}
