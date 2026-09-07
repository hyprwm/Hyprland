#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>
#include <vector>

#include <wayland-client.h>
#include <wayland.hpp>
#include <wlr-foreign-toplevel-management-unstable-v1.hpp>
#include <hyprland-toplevel-export-v1.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

// ── State ─────────────────────────────────────────────────────────────────────

struct SHandle {
    CSharedPointer<CCZwlrForeignToplevelHandleV1> resource;
    std::string                                   appID;
};

struct SState {
    wl_display*                                       display = nullptr;
    CSharedPointer<CCWlRegistry>                      registry;
    CSharedPointer<CCWlShm>                           shm;
    CSharedPointer<CCZwlrForeignToplevelManagerV1>    toplevelMgr;
    CSharedPointer<CCHyprlandToplevelExportManagerV1> exportMgr;

    std::vector<CSharedPointer<SHandle>>              handles;
    CSharedPointer<CCZwlrForeignToplevelHandleV1>     target;
    std::string                                       targetAppID;

    CSharedPointer<CCHyprlandToplevelExportFrameV1>   frame;
    CSharedPointer<CCWlShmPool>                       pool;
    CSharedPointer<CCWlBuffer>                        buffer;
    int                                               bufferFD = -1;

    uint32_t                                          bufferFormat = 0;
    uint32_t                                          bufferWidth  = 0;
    uint32_t                                          bufferHeight = 0;
    uint32_t                                          bufferStride = 0;

    bool                                              frameReceived = false;
    bool                                              shouldExit    = false;
};

static bool createCaptureBuffer(SState&);

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool bindGlobals(SState& state) {
    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(state.display)));

    state.registry->setGlobal([&state](CCWlRegistry* r, uint32_t name, const char* iface, uint32_t ver) {
        const std::string_view IFACE = iface;

        if (IFACE == "wl_shm") {
            state.shm = makeShared<CCWlShm>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(r->resource()), name, &wl_shm_interface, 1)));

        } else if (IFACE == "zwlr_foreign_toplevel_manager_v1") {
            state.toplevelMgr = makeShared<CCZwlrForeignToplevelManagerV1>(
                rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(r->resource()), name, &zwlr_foreign_toplevel_manager_v1_interface, std::min(ver, 3U))));

            state.toplevelMgr->setToplevel([&state](CCZwlrForeignToplevelManagerV1*, wl_proxy* res) {
                const auto H = makeShared<SHandle>();
                H->resource  = makeShared<CCZwlrForeignToplevelHandleV1>(res);
                auto* h      = H.get();
                H->resource->setAppId([h](CCZwlrForeignToplevelHandleV1*, const char* id) { h->appID = id; });
                H->resource->setDone([&state, h](CCZwlrForeignToplevelHandleV1*) {
                    if (h->appID == state.targetAppID)
                        state.target = h->resource;
                });
                H->resource->setClosed([&state, h](CCZwlrForeignToplevelHandleV1*) {
                    if (state.target == h->resource)
                        state.target.reset();
                });
                state.handles.emplace_back(H);
            });
            state.toplevelMgr->setFinished([](CCZwlrForeignToplevelManagerV1*) {});

        } else if (IFACE == "hyprland_toplevel_export_manager_v1") {
            state.exportMgr = makeShared<CCHyprlandToplevelExportManagerV1>(
                rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(r->resource()), name, &hyprland_toplevel_export_manager_v1_interface, std::min(ver, 2U))));
        }
    });
    state.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});

    if (wl_display_roundtrip(state.display) < 0)
        return false;

    return state.shm && state.toplevelMgr && state.exportMgr;
}

static bool findTarget(SState& state) {
    for (size_t i = 0; i < 3 && !state.target; ++i) {
        if (wl_display_roundtrip(state.display) < 0)
            return false;
    }
    return !!state.target;
}

static bool createCaptureBuffer(SState& state) {
    const size_t SIZE = sc<size_t>(state.bufferStride) * state.bufferHeight;
    if (SIZE == 0)
        return false;

    const std::string NAME = std::format("/wl-shm-toplevel-capture-{}", getpid());
    const int         FD   = shm_open(NAME.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (FD < 0)
        return false;
    if (shm_unlink(NAME.c_str()) < 0) {
        close(FD);
        return false;
    }
    if (ftruncate(FD, sc<off_t>(SIZE)) < 0) {
        close(FD);
        return false;
    }

    state.bufferFD = FD;
    state.pool     = makeShared<CCWlShmPool>(state.shm->sendCreatePool(FD, sc<int32_t>(SIZE)));
    if (!state.pool || !state.pool->resource())
        return false;

    state.buffer = makeShared<CCWlBuffer>(
        state.pool->sendCreateBuffer(0, sc<int32_t>(state.bufferWidth), sc<int32_t>(state.bufferHeight), sc<int32_t>(state.bufferStride), state.bufferFormat));
    return state.buffer && state.buffer->resource();
}

static bool requestCapture(SState& state) {
    state.frame =
        makeShared<CCHyprlandToplevelExportFrameV1>(state.exportMgr->sendCaptureToplevelWithWlrToplevelHandle(0 /*overlay_cursor*/, rc<wl_proxy*>(state.target->resource())));

    if (!state.frame || !state.frame->resource())
        return false;

    state.frame->setBuffer([&state](CCHyprlandToplevelExportFrameV1*, uint32_t fmt, uint32_t w, uint32_t h, uint32_t stride) {
        state.bufferFormat = fmt;
        state.bufferWidth  = w;
        state.bufferHeight = h;
        state.bufferStride = stride;
    });
    state.frame->setDamage([](CCHyprlandToplevelExportFrameV1*, uint32_t, uint32_t, uint32_t, uint32_t) {});
    state.frame->setFlags([](CCHyprlandToplevelExportFrameV1*, uint32_t) {});
    state.frame->setReady([&state](CCHyprlandToplevelExportFrameV1*, uint32_t, uint32_t, uint32_t) {
        state.frameReceived = true;
        state.shouldExit    = true;
    });
    state.frame->setFailed([&state](CCHyprlandToplevelExportFrameV1*) {
        std::println(stderr, "capture failed (server sent 'failed')");
        state.shouldExit = true;
    });
    state.frame->setLinuxDmabuf([](CCHyprlandToplevelExportFrameV1*, uint32_t, uint32_t, uint32_t) {});
    state.frame->setBufferDone([&state](CCHyprlandToplevelExportFrameV1*) {
        if (!createCaptureBuffer(state)) {
            std::println(stderr, "failed to allocate capture buffer");
            state.shouldExit = true;
            return;
        }
        state.frame->sendCopy(rc<wl_proxy*>(state.buffer->resource()), 1 /*ignore_damage*/);
    });

    return true;
}

static bool dispatchUntilDone(SState& state, std::chrono::steady_clock::time_point deadline) {
    while (!state.shouldExit) {
        while (wl_display_prepare_read(state.display) != 0) {
            if (wl_display_dispatch_pending(state.display) < 0)
                return false;
            if (state.shouldExit)
                return true;
        }

        if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
            wl_display_cancel_read(state.display);
            return false;
        }

        const auto NOW = std::chrono::steady_clock::now();
        if (NOW >= deadline) {
            wl_display_cancel_read(state.display);
            return false;
        }

        const auto REMAINING_MS = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - NOW).count();
        pollfd     pfd          = {.fd = wl_display_get_fd(state.display), .events = POLLIN, .revents = 0};
        const int  POLL_RET     = poll(&pfd, 1, sc<int>(REMAINING_MS));

        if (POLL_RET <= 0) {
            wl_display_cancel_read(state.display);
            if (POLL_RET < 0 && errno == EINTR)
                continue;
            return false;
        }

        if (wl_display_read_events(state.display) < 0)
            return false;
        if (wl_display_dispatch_pending(state.display) < 0)
            return false;
    }
    return true;
}

static void disconnect(SState& state) {
    const auto DISPLAY = state.display;
    if (!DISPLAY)
        return;

    state.frame.reset();
    state.buffer.reset();
    state.pool.reset();
    state.target.reset();
    state.handles.clear();
    state.toplevelMgr.reset();
    state.exportMgr.reset();
    state.shm.reset();
    state.registry.reset();

    if (state.bufferFD >= 0)
        close(state.bufferFD);

    state.display = nullptr;
    wl_display_flush(DISPLAY);
    wl_display_disconnect(DISPLAY);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::println(stderr, "usage: toplevel-capture <app-id>");
        return 1;
    }

    SState state;
    state.targetAppID = argv[1];
    state.display     = wl_display_connect(nullptr);
    if (!state.display) {
        std::println(stderr, "failed to connect to Wayland display");
        return 1;
    }

    if (!bindGlobals(state)) {
        std::println(stderr, "wl_shm, zwlr_foreign_toplevel_manager_v1, or hyprland_toplevel_export_manager_v1 unavailable");
        disconnect(state);
        return 1;
    }

    if (!findTarget(state)) {
        std::println(stderr, "no foreign toplevel found for app ID '{}'", state.targetAppID);
        disconnect(state);
        return 1;
    }

    if (!requestCapture(state)) {
        std::println(stderr, "failed to request toplevel capture");
        disconnect(state);
        return 1;
    }

    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    dispatchUntilDone(state, DEADLINE);

    if (!state.frameReceived)
        std::println(stderr, "did not receive a captured frame");

    const bool RESULT = state.frameReceived;
    disconnect(state);
    return RESULT ? 0 : 1;
}