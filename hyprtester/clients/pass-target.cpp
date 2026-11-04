#include <array>
#include <cstring>
#include <sys/poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <print>
#include <format>
#include <string>
#include <vector>
#include <utility>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

// A plain toplevel that records the wl_pointer and wl_keyboard events it receives, so tests can
// check what a window that `pass` targets sees.
//
// stdin commands:
//   events      prints the recorded events, separated by spaces, and clears them
//   appid <id>  sets the app id
//   exit        exits

namespace {
    struct SWlState {
        wl_display*                    display = nullptr;
        CSharedPointer<CCWlRegistry>   registry;

        CSharedPointer<CCWlCompositor> wlCompositor;
        CSharedPointer<CCWlSeat>       wlSeat;
        CSharedPointer<CCWlShm>        wlShm;
        CSharedPointer<CCXdgWmBase>    xdgShell;

        CSharedPointer<CCWlShmPool>    shmPool;
        CSharedPointer<CCWlBuffer>     shmBuf;
        CFileDescriptor                shmFd;
        bool                           xrgb8888_support = false;

        CSharedPointer<CCWlSurface>    surf;
        CSharedPointer<CCXdgSurface>   xdgSurf;
        CSharedPointer<CCXdgToplevel>  xdgToplevel;
        Vector2D                       geom = {640, 480};

        CSharedPointer<CCWlPointer>    pointer;
        CSharedPointer<CCWlKeyboard>   keyboard;
    };
}

static bool                     started    = false;
static bool                     shouldExit = false;
static std::vector<std::string> events;

template <typename... Args>
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

static bool bindRegistry(SWlState& state) {
    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(state.display)));

    state.registry->setGlobal([&](CCWlRegistry* r, uint32_t id, const char* name, uint32_t version) {
        const std::string NAME = name;
        if (NAME == "wl_compositor")
            state.wlCompositor = makeShared<CCWlCompositor>(sc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(state.registry->resource()), id, &wl_compositor_interface, 6)));
        else if (NAME == "wl_shm")
            state.wlShm = makeShared<CCWlShm>(sc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(state.registry->resource()), id, &wl_shm_interface, 1)));
        else if (NAME == "wl_seat")
            state.wlSeat = makeShared<CCWlSeat>(sc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(state.registry->resource()), id, &wl_seat_interface, 9)));
        else if (NAME == "xdg_wm_base")
            state.xdgShell = makeShared<CCXdgWmBase>(sc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(state.registry->resource()), id, &xdg_wm_base_interface, 1)));
    });

    wl_display_roundtrip(state.display);

    if (!state.wlCompositor || !state.wlShm || !state.wlSeat || !state.xdgShell) {
        clientLog("Failed to get protocols from Hyprland");
        return false;
    }

    return true;
}

static bool createShm(SWlState& state) {
    if (!state.xrgb8888_support)
        return false;

    const size_t STRIDE = state.geom.x * 4;
    const size_t SIZE   = state.geom.y * STRIDE;

    if (!state.shmPool) {
        state.shmFd = CFileDescriptor{memfd_create("pass-target", MFD_CLOEXEC)};
        if (!state.shmFd.isValid() || ftruncate(state.shmFd.get(), SIZE) < 0)
            return false;

        state.shmPool = makeShared<CCWlShmPool>(state.wlShm->sendCreatePool(state.shmFd.get(), SIZE));
        if (!state.shmPool->resource())
            return false;
    }

    state.shmBuf = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(0, state.geom.x, state.geom.y, STRIDE, WL_SHM_FORMAT_XRGB8888));
    return state.shmBuf->resource();
}

static bool setupToplevel(SWlState& state, const std::string& appId) {
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

    state.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        if (!state.shmBuf && !createShm(state))
            exit(-1);

        state.xdgSurf->sendSetWindowGeometry(0, 0, state.geom.x, state.geom.y);
        state.surf->sendAttach(state.shmBuf.get(), 0, 0);
        state.surf->sendCommit();

        state.xdgSurf->sendAckConfigure(serial);

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    state.xdgToplevel->sendSetTitle("pass-target test client");
    state.xdgToplevel->sendSetAppId(appId.c_str());

    state.surf->sendAttach(nullptr, 0, 0);
    state.surf->sendCommit();

    return true;
}

static bool setupSeat(SWlState& state) {
    state.pointer = makeShared<CCWlPointer>(state.wlSeat->sendGetPointer());
    if (!state.pointer->resource())
        return false;

    state.pointer->setEnter([](CCWlPointer* p, uint32_t serial, wl_proxy* surf, wl_fixed_t x, wl_fixed_t y) { events.emplace_back("penter"); });
    state.pointer->setLeave([](CCWlPointer* p, uint32_t serial, wl_proxy* surf) { events.emplace_back("pleave"); });
    state.pointer->setButton([](CCWlPointer* p, uint32_t serial, uint32_t time, uint32_t button, wl_pointer_button_state state) {
        events.emplace_back(std::format("button:{}:{}", button, sc<int>(state)));
    });

    state.keyboard = makeShared<CCWlKeyboard>(state.wlSeat->sendGetKeyboard());
    if (!state.keyboard->resource())
        return false;

    state.keyboard->setEnter([](CCWlKeyboard* p, uint32_t serial, wl_proxy* surf, wl_array* keys) { events.emplace_back("kenter"); });
    state.keyboard->setLeave([](CCWlKeyboard* p, uint32_t serial, wl_proxy* surf) { events.emplace_back("kleave"); });
    state.keyboard->setKey(
        [](CCWlKeyboard* p, uint32_t serial, uint32_t time, uint32_t key, wl_keyboard_key_state state) { events.emplace_back(std::format("key:{}:{}", key, sc<int>(state))); });

    return true;
}

static void parseRequest(SWlState& state, const std::string& req) {
    if (req.starts_with("events")) {
        std::string out;
        for (const auto& e : events) {
            out += e;
            out += ' ';
        }
        events.clear();
        clientLog("events {}", out);
    } else if (req.starts_with("appid ")) {
        auto id = req.substr(6);
        while (!id.empty() && (id.back() == '\n' || id.back() == ' '))
            id.pop_back();
        state.xdgToplevel->sendSetAppId(id.c_str());
        state.surf->sendCommit();
        wl_display_flush(state.display);
        clientLog("ok");
    } else if (req.starts_with("exit"))
        shouldExit = true;
}

int main(int argc, char** argv) {
    const std::string APPID = argc > 1 ? argv[1] : "pass-target";

    SWlState          state;

    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        clientLog("Failed to connect to wayland display");
        return -1;
    }

    if (!bindRegistry(state) || !setupSeat(state) || !setupToplevel(state, APPID)) {
        wl_display* display = state.display;
        state               = {};

        wl_display_disconnect(display);
        return -1;
    }

    std::array<char, 1024> readBuf = {};

    wl_display_flush(state.display);

    struct pollfd fds[2] = {{.fd = wl_display_get_fd(state.display), .events = POLLIN}, {.fd = STDIN_FILENO, .events = POLLIN}};
    while (!shouldExit && poll(fds, 2, -1) != -1) {
        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(state.display) == -1)
                break;
            wl_display_flush(state.display);
        }

        if (fds[1].revents & POLLIN) {
            const ssize_t BYTESREAD = read(fds[1].fd, readBuf.data(), readBuf.size() - 1);
            if (BYTESREAD <= 0)
                break;
            readBuf[BYTESREAD] = 0;

            parseRequest(state, std::string{readBuf.data()});
        }

        if (fds[0].revents & (POLLERR | POLLHUP) || fds[1].revents & (POLLERR | POLLHUP))
            break;
    }

    wl_display* display = state.display;
    state               = {};

    wl_display_disconnect(display);
    return 0;
}
