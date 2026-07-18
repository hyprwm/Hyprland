#include <array>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <print>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <presentation-time.hpp>

#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using Hyprutils::Math::Vector2D;
using namespace Hyprutils::Memory;

struct SBuffer {
    CSharedPointer<CCWlBuffer> buffer;
    uint32_t*                  data = nullptr;
};

struct SWlState {
    wl_display*                                           display = nullptr;
    CSharedPointer<CCWlRegistry>                          registry;
    CSharedPointer<CCWlCompositor>                        wlCompositor;
    CSharedPointer<CCWlShm>                               wlShm;
    CSharedPointer<CCXdgWmBase>                           xdgShell;
    CSharedPointer<CCWpPresentation>                      presentation;

    CSharedPointer<CCWlShmPool>                           shmPool;
    int                                                   shmFd      = -1;
    size_t                                                shmBufSize = 0;
    bool                                                  xrgb8888   = false;
    std::array<SBuffer, 2>                                buffers;

    CSharedPointer<CCWlSurface>                           surf;
    CSharedPointer<CCXdgSurface>                          xdgSurf;
    CSharedPointer<CCXdgToplevel>                         xdgToplevel;
    Vector2D                                              geom = {1280, 720};
    std::string                                           appId;

    std::vector<CSharedPointer<CCWlCallback>>             frameCallbacks;
    std::vector<CSharedPointer<CCWpPresentationFeedback>> feedbacks;

    uint64_t                                              commits           = 0;
    uint64_t                                              frameRequested    = 0;
    uint64_t                                              frameDone         = 0;
    uint64_t                                              feedbackRequested = 0;
    uint64_t                                              feedbackDone      = 0;
    uint64_t                                              presented         = 0;
    uint64_t                                              discarded         = 0;
    uint64_t                                              configureCount    = 0;
};

static bool debug = false, started = false, shouldExit = false;

template <typename... Args>
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

template <typename... Args>
static void debugLog(std::format_string<Args...> fmt, Args&&... args) {
    if (!debug)
        return;

    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

static bool createShm(SWlState& state) {
    if (!state.xrgb8888)
        return false;

    const size_t stride = state.geom.x * 4;
    const size_t size   = state.geom.y * stride;

    if (!state.shmPool) {
        const std::string name = std::format("/hyprtester-fs-stress-{}", getpid());
        state.shmFd            = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (state.shmFd < 0)
            return false;

        shm_unlink(name.c_str());

        if (ftruncate(state.shmFd, size * state.buffers.size()) < 0)
            return false;

        state.shmPool = makeShared<CCWlShmPool>(state.wlShm->sendCreatePool(state.shmFd, size * state.buffers.size()));
        if (!state.shmPool->resource())
            return false;

        state.shmBufSize = size * state.buffers.size();
    }

    auto* mapping = mmap(nullptr, state.shmBufSize, PROT_READ | PROT_WRITE, MAP_SHARED, state.shmFd, 0);
    if (mapping == MAP_FAILED)
        return false;

    for (size_t i = 0; i < state.buffers.size(); ++i) {
        const size_t offset     = i * size;
        state.buffers[i].data   = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mapping) + offset);
        state.buffers[i].buffer = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(offset, state.geom.x, state.geom.y, stride, WL_SHM_FORMAT_XRGB8888));

        if (!state.buffers[i].buffer->resource())
            return false;
    }

    return true;
}

static void paint(SWlState& state, SBuffer& buffer) {
    const uint32_t color  = 0xFF000000u | ((state.commits * 37u) & 0x00FFFFFFu);
    const size_t   pixels = state.geom.x * state.geom.y;

    for (size_t i = 0; i < pixels; i += 97)
        buffer.data[i] = color;
}

static void commitFrame(SWlState& state, bool invalidDamage) {
    if (!state.surf || !state.xdgToplevel)
        return;

    auto& buffer = state.buffers[state.commits % state.buffers.size()];
    paint(state, buffer);

    auto callback = makeShared<CCWlCallback>(state.surf->sendFrame());
    callback->setDone([&state](CCWlCallback* cb, uint32_t time) { state.frameDone++; });
    state.frameCallbacks.emplace_back(callback);
    state.frameRequested++;

    if (state.presentation) {
        auto feedback = makeShared<CCWpPresentationFeedback>(state.presentation->sendFeedback(state.surf->resource()));
        feedback->setPresented([&state](CCWpPresentationFeedback* fb, uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvNsec, uint32_t refresh, uint32_t seqHi, uint32_t seqLo,
                                        wpPresentationFeedbackKind flags) {
            state.presented++;
            state.feedbackDone++;
        });
        feedback->setDiscarded([&state](CCWpPresentationFeedback* fb) {
            state.discarded++;
            state.feedbackDone++;
        });
        state.feedbacks.emplace_back(feedback);
        state.feedbackRequested++;
    }

    state.surf->sendAttach(buffer.buffer.get(), 0, 0);

    if (invalidDamage) {
        state.surf->sendDamage(-100000, -100000, 300000, 300000);
        state.surf->sendDamageBuffer(-100000, -100000, 300000, 300000);
    } else {
        state.surf->sendDamageBuffer(0, 0, state.geom.x, state.geom.y);
    }

    state.surf->sendCommit();
    state.commits++;
    wl_display_flush(state.display);
}

static bool bindRegistry(SWlState& state) {
    state.registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(state.display));

    state.registry->setGlobal([&](CCWlRegistry* r, uint32_t id, const char* name, uint32_t version) {
        const std::string NAME = name;
        if (NAME == "wl_compositor")
            state.wlCompositor = makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_compositor_interface, 6));
        else if (NAME == "wl_shm")
            state.wlShm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_shm_interface, 1));
        else if (NAME == "xdg_wm_base")
            state.xdgShell = makeShared<CCXdgWmBase>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &xdg_wm_base_interface, 1));
        else if (NAME == "wp_presentation")
            state.presentation = makeShared<CCWpPresentation>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wp_presentation_interface, 2));
    });

    state.registry->setGlobalRemove([](CCWlRegistry* r, uint32_t id) { debugLog("Global {} removed", id); });
    wl_display_roundtrip(state.display);

    return state.wlCompositor && state.wlShm && state.xdgShell;
}

static bool setupToplevel(SWlState& state) {
    state.wlShm->setFormat([&](CCWlShm* p, uint32_t format) {
        if (format == WL_SHM_FORMAT_XRGB8888)
            state.xrgb8888 = true;
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

    state.xdgToplevel->setClose([](CCXdgToplevel* p) { shouldExit = true; });
    state.xdgToplevel->setConfigure([&](CCXdgToplevel* p, int32_t w, int32_t h, wl_array* arr) {
        if (!state.shmPool && w > 0 && h > 0)
            state.geom = {w, h};

        state.configureCount++;
        if (!state.shmPool && !createShm(state))
            exit(-1);
    });

    state.xdgSurf->setConfigure([&](CCXdgSurface* p, uint32_t serial) {
        state.xdgSurf->sendAckConfigure(serial);
        commitFrame(state, false);

        if (!started) {
            started = true;
            clientLog("started");
        }
    });

    state.xdgToplevel->sendSetTitle(state.appId.c_str());
    state.xdgToplevel->sendSetAppId(state.appId.c_str());
    state.surf->sendAttach(nullptr, 0, 0);
    state.surf->sendCommit();

    return true;
}

static void destroySurface(SWlState& state) {
    if (state.xdgToplevel)
        state.xdgToplevel->sendDestroy();
    if (state.xdgSurf)
        state.xdgSurf->sendDestroy();
    if (state.surf)
        state.surf->sendDestroy();

    state.xdgToplevel.reset();
    state.xdgSurf.reset();
    state.surf.reset();
    wl_display_flush(state.display);
}

static void printStats(const SWlState& state) {
    clientLog("stats commits={} frame_requested={} frame_done={} feedback_requested={} feedback_done={} presented={} discarded={} configures={}", state.commits,
              state.frameRequested, state.frameDone, state.feedbackRequested, state.feedbackDone, state.presented, state.discarded, state.configureCount);
}

static void parseLine(SWlState& state, const std::string& line) {
    std::stringstream ss(line);
    std::string       cmd;
    ss >> cmd;

    if (cmd == "exit")
        shouldExit = true;
    else if (cmd == "fullscreen" && state.xdgToplevel)
        state.xdgToplevel->sendSetFullscreen(nullptr);
    else if (cmd == "unfullscreen" && state.xdgToplevel)
        state.xdgToplevel->sendUnsetFullscreen();
    else if (cmd == "destroy")
        destroySurface(state);
    else if (cmd == "stats")
        printStats(state);
    else if (cmd == "burst") {
        int         frames = 1;
        std::string damage;
        ss >> frames >> damage;

        for (int i = 0; i < frames; ++i)
            commitFrame(state, damage == "invalid");

        clientLog("burst_done {}", frames);
    }
}

static void parseRequest(SWlState& state, std::string_view request) {
    static std::string pending;
    pending += request;

    for (auto pos = pending.find('\n'); pos != std::string::npos; pos = pending.find('\n')) {
        parseLine(state, pending.substr(0, pos));
        pending.erase(0, pos + 1);
    }
}

template <size_t N>
static ssize_t readChunk(int fd, std::array<char, N>& buffer) {
    // The destination capacity is exactly buffer.size(), and callers use only the returned byte count.
    // Flawfinder: ignore
    return read(fd, buffer.data(), buffer.size());
}

int main(int argc, char** argv) {
    SWlState state;
    state.appId = argc > 1 ? argv[1] : "fullscreen-scanout-stress";

    if (argc > 2 && std::string{argv[2]} == "--debug")
        debug = true;

    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        clientLog("failed to connect");
        return -1;
    }

    if (!bindRegistry(state) || !setupToplevel(state)) {
        clientLog("failed to setup");
        return -1;
    }

    wl_display_flush(state.display);

    std::array<char, 1024> readBuf;
    struct pollfd          fds[2] = {{.fd = wl_display_get_fd(state.display), .events = POLLIN | POLLOUT}, {.fd = STDIN_FILENO, .events = POLLIN}};

    while (!shouldExit && poll(fds, 2, 10) != -1) {
        if (fds[0].revents & POLLIN) {
            wl_display_flush(state.display);

            if (wl_display_prepare_read(state.display) == 0) {
                wl_display_read_events(state.display);
                wl_display_dispatch_pending(state.display);
            } else
                wl_display_dispatch(state.display);

            while (wl_display_dispatch_pending(state.display) > 0)
                wl_display_flush(state.display);
        }

        if (fds[1].revents & POLLIN) {
            const ssize_t bytesRead = readChunk(fds[1].fd, readBuf);
            if (bytesRead <= 0)
                continue;

            parseRequest(state, std::string_view{readBuf.data(), static_cast<size_t>(bytesRead)});
        }
    }

    const auto display = state.display;
    state.display      = nullptr;
    wl_display_disconnect(display);
    return 0;
}
