#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <iostream>
#include <print>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <viewporter.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

struct SState {
    CSharedPointer<CCWlRegistry>      registry;
    CSharedPointer<CCWlCompositor>    compositor;
    CSharedPointer<CCWlSubcompositor> subcompositor;
    CSharedPointer<CCWlShm>           shm;
    CSharedPointer<CCXdgWmBase>       wm;
    CSharedPointer<CCWpViewporter>    viewporter;
    CSharedPointer<CCWlSurface>       root, child;
    CSharedPointer<CCWlSubsurface>    subsurface;
    CSharedPointer<CCWpViewport>      viewport;
    CSharedPointer<CCXdgSurface>      xdg;
    CSharedPointer<CCXdgToplevel>     toplevel;
    int                               width = 320, height = 240;
    bool                              edges = false, configured = false, done = false;
};

static bool attach(SState& state, bool child, bool partial) {
    // A fractional-scale-style buffer with an integer logical viewport, like Firefox.
    const int             WIDTH  = child ? std::ceil(state.width * 4.0 / 3.0) : state.width;
    const int             HEIGHT = child ? std::ceil(state.height * 4.0 / 3.0) : state.height;
    const int             BYTES  = WIDTH * HEIGHT * 4;
    const CFileDescriptor FD{memfd_create("fractional-subsurface", MFD_CLOEXEC)};
    if (FD.get() < 0 || ftruncate(FD.get(), BYTES) < 0)
        return false;
    auto memory = mmap(nullptr, BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, FD.get(), 0);
    if (memory == MAP_FAILED)
        return false;
    const CScopeGuard unmap([&] { munmap(memory, BYTES); });
    auto              pixels = sc<uint32_t*>(memory);
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            pixels[y * WIDTH + x] = !child ? 0 : state.edges && (x >= WIDTH - 8 || y >= HEIGHT - 8) ? 0xFFFF0000 : 0xFF00FF00;
        }
    }
    const auto POOL    = makeShared<CCWlShmPool>(state.shm->sendCreatePool(FD.get(), BYTES));
    const auto BUFFER  = makeShared<CCWlBuffer>(POOL->sendCreateBuffer(0, WIDTH, HEIGHT, WIDTH * 4, WL_SHM_FORMAT_ARGB8888));
    const auto SURFACE = child ? state.child : state.root;
    if (child)
        state.viewport->sendSetDestination(state.width, state.height);
    SURFACE->sendAttach(BUFFER.get(), 0, 0);
    if (partial) {
        SURFACE->sendDamageBuffer(WIDTH - 8, 0, 8, HEIGHT);
        SURFACE->sendDamageBuffer(0, HEIGHT - 8, WIDTH, 8);
    } else
        SURFACE->sendDamageBuffer(0, 0, WIDTH, HEIGHT);
    SURFACE->sendCommit();
    BUFFER->sendDestroy();
    POOL->sendDestroy();
    return true;
}

int main() {
    const auto DISPLAY = wl_display_connect(nullptr);
    if (!DISPLAY)
        return 1;
    const CScopeGuard disconnect([DISPLAY] { wl_display_disconnect(DISPLAY); });
    SState            state;
    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(DISPLAY)));
    state.registry->setGlobal([&](CCWlRegistry* registry, uint32_t id, const char* interface, uint32_t version) {
        const std::string IFACE = interface;
        const auto        bind  = [&](const wl_interface& iface, uint32_t ver) {
            return rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &iface, std::min(version, ver)));
        };
        if (IFACE == "wl_compositor")
            state.compositor = makeShared<CCWlCompositor>(bind(wl_compositor_interface, 6));
        else if (IFACE == "wl_subcompositor")
            state.subcompositor = makeShared<CCWlSubcompositor>(bind(wl_subcompositor_interface, 1));
        else if (IFACE == "wl_shm")
            state.shm = makeShared<CCWlShm>(bind(wl_shm_interface, 1));
        else if (IFACE == "xdg_wm_base")
            state.wm = makeShared<CCXdgWmBase>(bind(xdg_wm_base_interface, 1));
        else if (IFACE == "wp_viewporter")
            state.viewporter = makeShared<CCWpViewporter>(bind(wp_viewporter_interface, 1));
    });
    if (wl_display_roundtrip(DISPLAY) < 0 || !state.compositor || !state.subcompositor || !state.shm || !state.wm || !state.viewporter)
        return 1;
    state.wm->setPing([&](CCXdgWmBase*, uint32_t serial) { state.wm->sendPong(serial); });
    state.root       = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    state.child      = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    state.subsurface = makeShared<CCWlSubsurface>(state.subcompositor->sendGetSubsurface(state.child.get(), state.root.get()));
    state.subsurface->sendSetDesync();
    state.viewport = makeShared<CCWpViewport>(state.viewporter->sendGetViewport(state.child->resource()));
    state.xdg      = makeShared<CCXdgSurface>(state.wm->sendGetXdgSurface(state.root->resource()));
    state.toplevel = makeShared<CCXdgToplevel>(state.xdg->sendGetToplevel());
    state.toplevel->sendSetAppId("fractional-subsurface");
    state.toplevel->setConfigure([&](CCXdgToplevel*, int32_t width, int32_t height, wl_array*) {
        if (width > 0 && height > 0) {
            state.width  = width;
            state.height = height;
        }
    });
    state.toplevel->setClose([&](CCXdgToplevel*) { state.done = true; });
    state.xdg->setConfigure([&](CCXdgSurface*, uint32_t serial) {
        state.xdg->sendAckConfigure(serial);
        state.xdg->sendSetWindowGeometry(0, 0, state.width, state.height);
        state.done       = !attach(state, true, false) || !attach(state, false, false);
        state.configured = true;
    });
    state.root->sendCommit();
    while (!state.configured && !state.done) {
        if (wl_display_roundtrip(DISPLAY) < 0)
            return 1;
    }
    std::println("ready");
    std::cout.flush();
    while (!state.done) {
        while (wl_display_prepare_read(DISPLAY) != 0) {
            if (wl_display_dispatch_pending(DISPLAY) < 0)
                return 1;
        }
        wl_display_flush(DISPLAY);
        std::array<pollfd, 2> fds    = {{{wl_display_get_fd(DISPLAY), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}}};
        const auto            RESULT = poll(fds.data(), fds.size(), -1);
        if (RESULT < 0) {
            wl_display_cancel_read(DISPLAY);
            if (errno == EINTR)
                continue;
            return 1;
        }
        if (fds[0].revents & POLLIN) {
            if (wl_display_read_events(DISPLAY) < 0 || wl_display_dispatch_pending(DISPLAY) < 0)
                return 1;
        } else
            wl_display_cancel_read(DISPLAY);
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            std::string command;
            if (!std::getline(std::cin, command) || command == "exit")
                break;
            if (command == "edges") {
                state.edges = !state.edges;
                if (!attach(state, true, true))
                    return 1;
            }
            if (wl_display_roundtrip(DISPLAY) < 0)
                return 1;
            std::println("ok");
            std::cout.flush();
        }
    }
    return 0;
}
