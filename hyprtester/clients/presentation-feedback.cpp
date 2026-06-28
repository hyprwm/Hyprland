#include <array>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>
#include <vector>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <presentation-time.hpp>

#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

namespace {
    constexpr int WIDTH       = 64;
    constexpr int HEIGHT      = 64;
    constexpr int STRIDE      = WIDTH * 4;
    constexpr int BUFFER_SIZE = STRIDE * HEIGHT;

    struct SFeedbackResult {
        char        label     = '?';
        std::string status    = "";
        bool        completed = false;
    };

    struct SWlState {
        wl_display*                                           display = nullptr;

        CSharedPointer<CCWlRegistry>                          registry;
        CSharedPointer<CCWlCompositor>                        compositor;
        CSharedPointer<CCWlShm>                               shm;
        CSharedPointer<CCXdgWmBase>                           xdgShell;
        CSharedPointer<CCWpPresentation>                      presentation;
        std::vector<CSharedPointer<CCWlOutput>>               outputs;

        CSharedPointer<CCWlSurface>                           surface;
        CSharedPointer<CCXdgSurface>                          xdgSurface;
        CSharedPointer<CCXdgToplevel>                         xdgToplevel;
        CSharedPointer<CCWlShmPool>                           shmPool;
        std::array<CSharedPointer<CCWlBuffer>, 4>             buffers;
        std::vector<CSharedPointer<CCWpPresentationFeedback>> feedbacks;
        std::array<SFeedbackResult, 3>                        results = {SFeedbackResult{'A'}, SFeedbackResult{'B'}, SFeedbackResult{'C'}};

        int                                                   shmFd       = -1;
        void*                                                 shmData     = nullptr;
        bool                                                  hasXrgb8888 = false;
        bool                                                  configured  = false;
        bool                                                  submitted   = false;
        bool                                                  summarized  = false;
        bool                                                  destroyMode = false;
        bool                                                  destroyed   = false;
        std::chrono::steady_clock::time_point                 submittedAt = {};
    };

    void logLine(const std::string& text) {
        std::println("{}", text);
        std::fflush(stdout);
    }

    bool allFeedbacksCompleted(const SWlState& state) {
        return std::ranges::all_of(state.results, [](const auto& result) { return result.completed; });
    }

    int resultIndex(char label) {
        return label - 'A';
    }

    void recordResult(SWlState& state, char label, const std::string& status) {
        const auto INDEX = resultIndex(label);
        if (INDEX < 0 || INDEX >= static_cast<int>(state.results.size()) || state.results[INDEX].completed)
            return;

        state.results[INDEX].status    = status;
        state.results[INDEX].completed = true;
        logLine(std::format("{} {}", label, status));
    }

    void summarizeIfComplete(SWlState& state) {
        if (state.summarized || !allFeedbacksCompleted(state))
            return;

        int presented = 0;
        int discarded = 0;
        for (const auto& result : state.results) {
            if (result.status == "presented")
                presented++;
            else if (result.status == "discarded")
                discarded++;
        }

        state.summarized = true;
        logLine(std::format("summary discarded={} presented={}", discarded, presented));
    }

    bool createBuffers(SWlState& state) {
        if (!state.hasXrgb8888)
            return false;

        const auto TOTAL_SIZE = BUFFER_SIZE * static_cast<int>(state.buffers.size());
        const auto SHM_NAME   = std::format("/hypr-presentation-feedback-{}", getpid());

        state.shmFd = shm_open(SHM_NAME.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (state.shmFd < 0)
            return false;

        shm_unlink(SHM_NAME.c_str());

        if (ftruncate(state.shmFd, TOTAL_SIZE) < 0)
            return false;

        state.shmData = mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, state.shmFd, 0);
        if (state.shmData == MAP_FAILED) {
            state.shmData = nullptr;
            return false;
        }

        constexpr std::array<uint32_t, 4> COLORS = {0xff202020, 0xffff0000, 0xff00ff00, 0xff0000ff};
        for (size_t i = 0; i < COLORS.size(); ++i) {
            auto* pixels = static_cast<uint32_t*>(state.shmData) + (BUFFER_SIZE / 4) * i;
            std::fill_n(pixels, BUFFER_SIZE / 4, COLORS[i]);
        }

        state.shmPool = makeShared<CCWlShmPool>(state.shm->sendCreatePool(state.shmFd, TOTAL_SIZE));
        if (!state.shmPool || !state.shmPool->resource())
            return false;

        for (size_t i = 0; i < state.buffers.size(); ++i) {
            state.buffers[i] = makeShared<CCWlBuffer>(state.shmPool->sendCreateBuffer(BUFFER_SIZE * i, WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888));
            if (!state.buffers[i] || !state.buffers[i]->resource())
                return false;
        }

        return true;
    }

    void submitFeedbackCommit(SWlState& state, char label, size_t bufferIndex) {
        auto feedback = makeShared<CCWpPresentationFeedback>(state.presentation->sendFeedback(state.surface->resource()));
        feedback->setPresented([&state, label](auto*, auto...) {
            recordResult(state, label, "presented");
            summarizeIfComplete(state);
        });
        feedback->setDiscarded([&state, label](auto*) {
            recordResult(state, label, "discarded");
            summarizeIfComplete(state);
        });
        feedback->setSyncOutput([](auto*, auto*) {});
        state.feedbacks.emplace_back(feedback);

        state.surface->sendAttach(state.buffers[bufferIndex].get(), 0, 0);
        state.surface->sendDamageBuffer(0, 0, WIDTH, HEIGHT);
        state.surface->sendCommit();
    }

    void submitBurst(SWlState& state) {
        if (state.submitted)
            return;

        state.submitted   = true;
        state.submittedAt = std::chrono::steady_clock::now();
        submitFeedbackCommit(state, 'A', 1);
        submitFeedbackCommit(state, 'B', 2);
        submitFeedbackCommit(state, 'C', 3);
        wl_display_flush(state.display);
    }

    void submitDestroy(SWlState& state) {
        if (state.submitted)
            return;

        state.submitted   = true;
        state.destroyMode = true;
        state.submittedAt = std::chrono::steady_clock::now();

        auto feedback = makeShared<CCWpPresentationFeedback>(state.presentation->sendFeedback(state.surface->resource()));
        feedback->setPresented([&state](auto*, auto...) {
            state.destroyed = true;
            logLine("D presented");
        });
        feedback->setDiscarded([&state](auto*) {
            state.destroyed = true;
            logLine("D discarded");
        });
        feedback->setSyncOutput([](auto*, auto*) {});
        state.feedbacks.emplace_back(feedback);

        state.surface->sendAttach(state.buffers[1].get(), 0, 0);
        state.surface->sendDamageBuffer(0, 0, WIDTH, HEIGHT);
        state.surface->sendCommit();
        state.xdgToplevel->sendDestroy();
        state.xdgSurface->sendDestroy();
        state.surface->sendDestroy();
        wl_display_flush(state.display);
    }

    bool bindRegistry(SWlState& state) {
        state.registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(state.display));
        state.registry->setGlobal([&](CCWlRegistry*, uint32_t id, const char* name, uint32_t version) {
            const std::string NAME = name;
            if (NAME == "wl_compositor")
                state.compositor =
                    makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_compositor_interface, std::min(version, 6u)));
            else if (NAME == "wl_shm") {
                state.shm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_shm_interface, 1));
                state.shm->setFormat([&](CCWlShm*, uint32_t format) {
                    if (format == WL_SHM_FORMAT_XRGB8888)
                        state.hasXrgb8888 = true;
                });
            } else if (NAME == "xdg_wm_base")
                state.xdgShell = makeShared<CCXdgWmBase>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &xdg_wm_base_interface, 1));
            else if (NAME == "wp_presentation")
                state.presentation = makeShared<CCWpPresentation>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wp_presentation_interface, 1));
            else if (NAME == "wl_output") {
                auto output = makeShared<CCWlOutput>((wl_proxy*)wl_registry_bind((wl_registry*)state.registry->resource(), id, &wl_output_interface, std::min(version, 4u)));
                output->setGeometry([](CCWlOutput*, auto...) {});
                output->setMode([](CCWlOutput*, auto...) {});
                output->setDone([](CCWlOutput*) {});
                output->setScale([](CCWlOutput*, auto...) {});
                output->setName([](CCWlOutput*, auto...) {});
                output->setDescription([](CCWlOutput*, auto...) {});
                state.outputs.emplace_back(std::move(output));
            }
        });
        state.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});

        wl_display_roundtrip(state.display);

        return state.compositor && state.shm && state.xdgShell && state.presentation;
    }

    bool setupSurface(SWlState& state) {
        state.xdgShell->setPing([&](CCXdgWmBase*, uint32_t serial) { state.xdgShell->sendPong(serial); });

        state.surface = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
        if (!state.surface || !state.surface->resource())
            return false;

        state.xdgSurface = makeShared<CCXdgSurface>(state.xdgShell->sendGetXdgSurface(state.surface->resource()));
        if (!state.xdgSurface || !state.xdgSurface->resource())
            return false;

        state.xdgToplevel = makeShared<CCXdgToplevel>(state.xdgSurface->sendGetToplevel());
        if (!state.xdgToplevel || !state.xdgToplevel->resource())
            return false;

        state.xdgToplevel->setClose([](CCXdgToplevel*) { exit(0); });
        state.xdgSurface->setConfigure([&](CCXdgSurface*, uint32_t serial) {
            state.xdgSurface->sendAckConfigure(serial);

            if (!state.buffers[0] && !createBuffers(state)) {
                logLine("failed to create buffers");
                exit(1);
            }

            state.xdgSurface->sendSetWindowGeometry(0, 0, WIDTH, HEIGHT);
            state.surface->sendAttach(state.buffers[0].get(), 0, 0);
            state.surface->sendDamageBuffer(0, 0, WIDTH, HEIGHT);
            state.surface->sendCommit();

            if (!state.configured) {
                state.configured = true;
                logLine("started");
            }
        });

        state.xdgToplevel->sendSetTitle("presentation-feedback test client");
        state.xdgToplevel->sendSetAppId("presentation-feedback");

        state.surface->sendAttach(nullptr, 0, 0);
        state.surface->sendCommit();
        return true;
    }

    void destroyState(SWlState& state) {
        if (state.shmData)
            munmap(state.shmData, BUFFER_SIZE * state.buffers.size());

        if (state.shmFd >= 0)
            close(state.shmFd);

        if (state.display)
            wl_display_disconnect(state.display);
    }
}

int main() {
    SWlState state;
    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        logLine("failed to connect");
        return 1;
    }

    if (!bindRegistry(state) || !setupSurface(state)) {
        logLine("failed to setup");
        destroyState(state);
        return 1;
    }

    struct pollfd fds[2] = {{.fd = wl_display_get_fd(state.display), .events = POLLIN | POLLOUT}, {.fd = STDIN_FILENO, .events = POLLIN}};

    while (poll(fds, 2, 50) >= 0) {
        if (fds[0].revents & POLLIN)
            wl_display_dispatch(state.display);

        if ((fds[0].revents & POLLIN) || (fds[0].revents & POLLOUT)) {
            wl_display_dispatch_pending(state.display);
            wl_display_flush(state.display);
        }

        if (fds[1].revents & POLLIN) {
            std::string REQUEST;
            if (std::getline(std::cin, REQUEST)) {
                if (REQUEST.contains("run"))
                    submitBurst(state);
                else if (REQUEST.contains("destroy"))
                    submitDestroy(state);
                else if (REQUEST.contains("exit"))
                    break;
            }
        }

        if (state.summarized || state.destroyed)
            break;

        if (state.submitted && std::chrono::steady_clock::now() - state.submittedAt > std::chrono::seconds(5)) {
            logLine("summary timeout");
            break;
        }
    }

    destroyState(state);
    return state.summarized || state.destroyed ? 0 : 1;
}
