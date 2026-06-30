#include <cerrno>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <fractional-scale-v1.hpp>

#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

struct SSurfaceStats {
    int rootScaleCount      = 0;
    int rootTransformCount  = 0;
    int rootFractionCount   = 0;
    int childScaleCount     = 0;
    int childTransformCount = 0;
    int childFractionCount  = 0;

    int rootScale      = -1;
    int rootTransform  = -1;
    int rootFraction   = -1;
    int childScale     = -1;
    int childTransform = -1;
    int childFraction  = -1;
};

struct SWlState {
    wl_display*                                  display = nullptr;

    CSharedPointer<CCWlRegistry>                 registry;
    CSharedPointer<CCWlCompositor>               compositor;
    CSharedPointer<CCWlSubcompositor>            subcompositor;
    CSharedPointer<CCWlShm>                      shm;
    CSharedPointer<CCXdgWmBase>                  xdgShell;
    CSharedPointer<CCWpFractionalScaleManagerV1> fractional;

    CSharedPointer<CCWlSurface>                  surface;
    CSharedPointer<CCXdgSurface>                 xdgSurface;
    CSharedPointer<CCXdgToplevel>                xdgToplevel;
    CSharedPointer<CCWpFractionalScaleV1>        rootFractional;
    CSharedPointer<CCWlBuffer>                   rootBuffer;

    CSharedPointer<CCWlSurface>                  childSurface;
    CSharedPointer<CCWlSubsurface>               childSubsurface;
    CSharedPointer<CCWpFractionalScaleV1>        childFractional;
    CSharedPointer<CCWlBuffer>                   childBuffer;

    SSurfaceStats                                stats;
    bool                                         xrgb8888   = false;
    bool                                         configured = false;
    std::string                                  setupError;
};

static void sendLine(const std::string& line) {
    std::cout << line << std::endl;
}

static bool pollDisplay(SWlState& state, int timeoutMs) {
    while (wl_display_prepare_read(state.display) != 0) {
        if (wl_display_dispatch_pending(state.display) == -1)
            return false;
    }

    if (wl_display_flush(state.display) == -1 && errno != EAGAIN) {
        wl_display_cancel_read(state.display);
        return false;
    }

    pollfd fd = {.fd = wl_display_get_fd(state.display), .events = POLLIN, .revents = 0};

    int    ret = 0;
    do {
        ret = poll(&fd, 1, timeoutMs);
    } while (ret == -1 && errno == EINTR);

    if (ret == -1) {
        wl_display_cancel_read(state.display);
        return false;
    }

    if (ret == 0) {
        wl_display_cancel_read(state.display);
        return true;
    }

    if (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        wl_display_cancel_read(state.display);
        return false;
    }

    if (!(fd.revents & POLLIN)) {
        wl_display_cancel_read(state.display);
        return true;
    }

    if (wl_display_read_events(state.display) == -1)
        return false;

    return wl_display_dispatch_pending(state.display) != -1;
}

template <typename F>
static bool waitForDisplayState(SWlState& state, F&& predicate, int attempts, int timeoutMs) {
    for (int i = 0; i < attempts && !predicate(); ++i) {
        if (!pollDisplay(state, timeoutMs))
            return false;
    }

    return predicate();
}

static std::string bindError(const SWlState& state) {
    std::string ret;

    if (!state.compositor)
        ret += " wl_compositor";
    if (!state.subcompositor)
        ret += " wl_subcompositor";
    if (!state.shm)
        ret += " wl_shm";
    if (!state.xdgShell)
        ret += " xdg_wm_base";
    if (!state.fractional)
        ret += " wp_fractional_scale_manager_v1";
    if (state.shm && !state.xrgb8888)
        ret += " xrgb8888";

    return ret.empty() ? "dispatch" : ret.substr(1);
}

static CSharedPointer<CCWlBuffer> createBuffer(SWlState& state, int width, int height) {
    if (!state.xrgb8888)
        return nullptr;

    const int stride = width * 4;
    const int size   = stride * height;
    const int fd     = memfd_create("hyprtester-surface-scale", MFD_CLOEXEC);
    if (fd < 0)
        return nullptr;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return nullptr;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return nullptr;
    }

    std::memset(data, 0x33, size);
    munmap(data, size);

    auto pool   = makeShared<CCWlShmPool>(state.shm->sendCreatePool(fd, size));
    auto buffer = makeShared<CCWlBuffer>(pool->sendCreateBuffer(0, width, height, stride, WL_SHM_FORMAT_XRGB8888));
    pool->sendDestroy();
    close(fd);

    return buffer;
}

static void setSurfaceListeners(CSharedPointer<CCWlSurface> surface, SSurfaceStats& stats, bool child) {
    surface->setPreferredBufferScale([&stats, child](CCWlSurface*, int32_t scale) {
        if (child) {
            stats.childScale = scale;
            stats.childScaleCount++;
        } else {
            stats.rootScale = scale;
            stats.rootScaleCount++;
        }
    });

    surface->setPreferredBufferTransform([&stats, child](CCWlSurface*, uint32_t transform) {
        if (child) {
            stats.childTransform = transform;
            stats.childTransformCount++;
        } else {
            stats.rootTransform = transform;
            stats.rootTransformCount++;
        }
    });
}

static void setFractionalListener(CSharedPointer<CCWpFractionalScaleV1> fractional, SSurfaceStats& stats, bool child) {
    fractional->setPreferredScale([&stats, child](CCWpFractionalScaleV1*, uint32_t scale) {
        if (child) {
            stats.childFraction = scale;
            stats.childFractionCount++;
        } else {
            stats.rootFraction = scale;
            stats.rootFractionCount++;
        }
    });
}

static bool bindRegistry(SWlState& state) {
    state.registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(state.display));
    if (!state.registry)
        return false;

    state.registry->setGlobal([&](CCWlRegistry*, uint32_t id, const char* name, uint32_t version) {
        const std::string NAME = name;
        if (NAME == "wl_compositor")
            state.compositor =
                makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_compositor_interface, std::min<uint32_t>(version, 6)));
        else if (NAME == "wl_subcompositor")
            state.subcompositor = makeShared<CCWlSubcompositor>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_subcompositor_interface, 1));
        else if (NAME == "wl_shm") {
            state.shm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_shm_interface, 1));
            state.shm->setFormat([&](CCWlShm*, uint32_t format) {
                if (format == WL_SHM_FORMAT_XRGB8888)
                    state.xrgb8888 = true;
            });
        } else if (NAME == "xdg_wm_base")
            state.xdgShell = makeShared<CCXdgWmBase>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &xdg_wm_base_interface, 1));
        else if (NAME == "wp_fractional_scale_manager_v1")
            state.fractional =
                makeShared<CCWpFractionalScaleManagerV1>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wp_fractional_scale_manager_v1_interface, 1));
    });

    state.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});
    return waitForDisplayState(
        state, [&state]() { return state.compositor && state.subcompositor && state.shm && state.xdgShell && state.fractional && state.xrgb8888; }, 100, 100);
}

static bool setupToplevel(SWlState& state) {
    state.xdgShell->setPing([&](CCXdgWmBase*, uint32_t serial) { state.xdgShell->sendPong(serial); });

    state.surface = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    if (!state.surface) {
        state.setupError = "surface";
        return false;
    }

    setSurfaceListeners(state.surface, state.stats, false);

    state.rootFractional = makeShared<CCWpFractionalScaleV1>(state.fractional->sendGetFractionalScale(state.surface->resource()));
    if (!state.rootFractional) {
        state.setupError = "fractional";
        return false;
    }

    setFractionalListener(state.rootFractional, state.stats, false);

    state.xdgSurface = makeShared<CCXdgSurface>(state.xdgShell->sendGetXdgSurface(state.surface->resource()));
    if (!state.xdgSurface) {
        state.setupError = "xdg-surface";
        return false;
    }

    state.xdgToplevel = makeShared<CCXdgToplevel>(state.xdgSurface->sendGetToplevel());
    if (!state.xdgToplevel) {
        state.setupError = "xdg";
        return false;
    }

    state.xdgToplevel->setClose([](CCXdgToplevel*) { exit(0); });
    state.xdgToplevel->setConfigure([](CCXdgToplevel*, int32_t, int32_t, wl_array*) {});
    state.xdgSurface->setConfigure([&](CCXdgSurface*, uint32_t serial) {
        state.xdgSurface->sendAckConfigure(serial);
        state.configured = true;
        if (!state.rootBuffer)
            state.rootBuffer = createBuffer(state, 320, 240);
        state.surface->sendAttach(state.rootBuffer.get(), 0, 0);
        state.surface->sendCommit();
    });

    state.xdgToplevel->sendSetTitle("surface scale transform test");
    state.xdgToplevel->sendSetAppId("surface-scale-transform");
    state.surface->sendCommit();

    if (!waitForDisplayState(state, [&state]() { return state.configured; }, 100, 100)) {
        state.setupError = "timeout";
        return false;
    }

    if (!waitForDisplayState(state, [&state]() { return state.stats.rootScale != -1 && state.stats.rootTransform != -1 && state.stats.rootFraction != -1; }, 100, 100)) {
        state.setupError = "surface-events";
        return false;
    }

    if (!state.rootBuffer)
        state.setupError = "buffer";

    return state.rootBuffer != nullptr;
}

static bool createChild(SWlState& state) {
    if (state.childSurface)
        return true;

    state.childSurface = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    setSurfaceListeners(state.childSurface, state.stats, true);

    state.childFractional = makeShared<CCWpFractionalScaleV1>(state.fractional->sendGetFractionalScale(state.childSurface->resource()));
    setFractionalListener(state.childFractional, state.stats, true);

    state.childSubsurface = makeShared<CCWlSubsurface>(state.subcompositor->sendGetSubsurface(state.childSurface.get(), state.surface.get()));
    state.childSubsurface->sendSetPosition(10, 10);

    state.childBuffer = createBuffer(state, 64, 64);
    if (!state.childBuffer)
        return false;

    state.childSurface->sendAttach(state.childBuffer.get(), 0, 0);
    state.childSurface->sendCommit();
    state.surface->sendCommit();

    return waitForDisplayState(state, [&state]() { return state.stats.childScale != -1 && state.stats.childTransform != -1 && state.stats.childFraction != -1; }, 100, 100);
}

static bool remapRoot(SWlState& state) {
    state.surface->sendAttach(state.rootBuffer.get(), 0, 0);
    state.surface->sendCommit();

    return pollDisplay(state, 100);
}

static std::string report(const SSurfaceStats& stats) {
    return std::format("root_scale={} root_scale_count={} root_transform={} root_transform_count={} root_fraction={} root_fraction_count={} child_scale={} child_scale_count={} "
                       "child_transform={} child_transform_count={} child_fraction={} child_fraction_count={}",
                       stats.rootScale, stats.rootScaleCount, stats.rootTransform, stats.rootTransformCount, stats.rootFraction, stats.rootFractionCount, stats.childScale,
                       stats.childScaleCount, stats.childTransform, stats.childTransformCount, stats.childFraction, stats.childFractionCount);
}

static void handleCommand(SWlState& state, const std::string& command) {
    pollDisplay(state, 100);

    if (command == "report")
        sendLine(report(state.stats));
    else if (command == "unmap") {
        state.surface->sendAttach(nullptr, 0, 0);
        state.surface->sendCommit();
        state.configured = false;
        pollDisplay(state, 100);
        sendLine("ok");
    } else if (command == "remap") {
        sendLine(remapRoot(state) ? "ok" : "error");
    } else if (command == "subsurface")
        sendLine(createChild(state) ? "ok" : "error");
    else if (command == "exit")
        exit(0);
    else
        sendLine("unknown");
}

int main() {
    SWlState state;
    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        sendLine("error connect");
        return 1;
    }

    if (!bindRegistry(state)) {
        sendLine(std::format("error bind {}", bindError(state)));
        return 1;
    }

    if (!setupToplevel(state)) {
        sendLine(std::format("error setup {}", state.setupError));
        return 1;
    }

    sendLine("started");

    std::string command;
    while (std::getline(std::cin, command))
        handleCommand(state, command);

    return 0;
}
