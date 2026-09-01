#include "EmbeddedToolkitManager.hpp"

#include "../../Compositor.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../render/OpenGL.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprtoolkit/core/EmbeddedBackend.hpp>
#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/window/EmbeddedSurface.hpp>
#include <sys/eventfd.h>
#include <unistd.h>
#include <wayland-server-core.h>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

namespace EmbeddedToolkit {
    class CEventLoop final : public Hyprtoolkit::IEventLoop {
      public:
        CEventLoop() {
            if (!g_pCompositor || !g_pCompositor->m_wlEventLoop)
                return;

            m_idleFD     = CFileDescriptor{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
            m_idleSource = wl_event_loop_add_fd(
                g_pCompositor->m_wlEventLoop, m_idleFD.get(), WL_EVENT_READABLE,
                [](int fd, uint32_t mask, void* data) {
                    if (mask & (WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR))
                        sc<CEventLoop*>(data)->dispatchIdles(fd);
                    return 0;
                },
                this);
        }

        virtual ~CEventLoop() override {
            cancelPending();
            if (m_idleSource)
                wl_event_source_remove(m_idleSource);
        }

        virtual void addFd(int fd, std::function<void()>&& callback) override {
            removeFd(fd);
            if (!g_pCompositor || !g_pCompositor->m_wlEventLoop)
                return;

            auto source      = makeUnique<SFdSource>();
            source->callback = std::move(callback);
            source->source   = wl_event_loop_add_fd(
                g_pCompositor->m_wlEventLoop, fd, WL_EVENT_READABLE,
                [](int, uint32_t mask, void* data) {
                    const auto SOURCE = sc<SFdSource*>(data);
                    if (SOURCE && SOURCE->callback && (mask & (WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR)))
                        SOURCE->callback();
                    return 0;
                },
                source.get());

            if (!source->source)
                return;

            m_fds.emplace(fd, std::move(source));
        }

        virtual void removeFd(int fd) override {
            const auto IT = m_fds.find(fd);
            if (IT == m_fds.end())
                return;

            if (IT->second->source)
                wl_event_source_remove(IT->second->source);
            m_fds.erase(IT);
        }

        virtual CAtomicSharedPointer<Hyprtoolkit::CTimer> addTimer(const Hyprtoolkit::TimerDuration&                                               timeout,
                                                                   std::function<void(CAtomicSharedPointer<Hyprtoolkit::CTimer> self, void* data)> callback, void* data,
                                                                   bool force) override {
            auto timer = makeAtomicShared<Hyprtoolkit::CTimer>(timeout, std::move(callback), data, force);
            if (!g_pEventLoopManager)
                return timer;

            auto hostTimer = makeShared<CEventLoopTimer>(
                timeout,
                [this, timer](SP<CEventLoopTimer> self, void*) {
                    if (timer->cancelled()) {
                        removeTimer(timer);
                        return;
                    }

                    if (!timer->passed()) {
                        self->updateTimeout(std::chrono::duration_cast<Time::steady_dur>(std::chrono::duration<float, std::milli>(std::max(timer->leftMs(), 1.F))));
                        return;
                    }

                    timer->call(timer);
                    removeTimer(timer);
                },
                nullptr);

            m_timers.emplace_back(STimer{.timer = timer, .host = hostTimer});
            g_pEventLoopManager->addTimer(hostTimer);
            return timer;
        }

        virtual void addIdle(const std::function<void()>& callback) override {
            if (m_stopping || !m_idleFD.isValid() || !m_idleSource)
                return;

            {
                std::lock_guard<std::mutex> lock(m_idleMutex);
                if (m_stopping)
                    return;
                m_idles.emplace_back(callback);
            }

            constexpr uint64_t WAKE   = 1;
            ssize_t            result = 0;
            do {
                result = write(m_idleFD.get(), &WAKE, sizeof(WAKE));
            } while (result < 0 && errno == EINTR);
            if (result < 0 && errno != EAGAIN)
                Log::logger->log(Log::ERR, "Failed to wake the embedded hyprtoolkit event loop: {}", strerror(errno));
        }

        virtual void cancelPending() override {
            m_stopping = true;

            for (const auto& [_, source] : m_fds) {
                if (source->source)
                    wl_event_source_remove(source->source);
            }
            m_fds.clear();

            for (const auto& timer : m_timers)
                timer.host->cancel();
            m_timers.clear();

            std::lock_guard<std::mutex> lock(m_idleMutex);
            m_idles.clear();
        }

        virtual void enterLoop() override {
            ;
        }

      private:
        struct SFdSource {
            wl_event_source*      source = nullptr;
            std::function<void()> callback;
        };

        struct STimer {
            CAtomicSharedPointer<Hyprtoolkit::CTimer> timer;
            SP<CEventLoopTimer>                       host;
        };

        void removeTimer(const CAtomicSharedPointer<Hyprtoolkit::CTimer>& timer) {
            std::erase_if(m_timers, [&timer](const auto& candidate) { return candidate.timer == timer; });
        }

        void dispatchIdles(int fd) {
            uint64_t wakeCount = 0;
            while (read(fd, &wakeCount, sizeof(wakeCount)) > 0) {
                ;
            }

            std::vector<std::function<void()>> idles;
            {
                std::lock_guard<std::mutex> lock(m_idleMutex);
                idles.swap(m_idles);
            }

            for (const auto& idle : idles) {
                if (!m_stopping && idle)
                    idle();
            }
        }

        std::map<int, UP<SFdSource>>       m_fds;
        std::vector<STimer>                m_timers;
        CFileDescriptor                    m_idleFD;
        wl_event_source*                   m_idleSource = nullptr;
        std::mutex                         m_idleMutex;
        std::vector<std::function<void()>> m_idles;
        std::atomic<bool>                  m_stopping = false;
    };

    CManager::CManager() {
        if (!Render::GL::g_pHyprOpenGL || !g_pEventLoopManager)
            return;

        Render::GL::g_pHyprOpenGL->makeEGLCurrent();
        m_eventLoop = makeShared<CEventLoop>();

        Hyprtoolkit::IEmbeddedBackend::SCreationData creationData;
        creationData.eventLoop = m_eventLoop;
        m_backend              = Hyprtoolkit::IEmbeddedBackend::create(creationData);
        if (!m_backend)
            Log::logger->log(Log::ERR, "Failed to initialize the embedded hyprtoolkit backend");
    }

    CManager::~CManager() {
        if (!m_backend)
            return;

        if (Render::GL::g_pHyprOpenGL)
            Render::GL::g_pHyprOpenGL->makeEGLCurrent();
        m_backend->destroy();
        m_backend.reset();
        m_eventLoop.reset();
    }

    SP<Hyprtoolkit::IEmbeddedSurface> CManager::createSurface() const {
        return m_backend ? m_backend->createSurface() : nullptr;
    }

    bool CManager::available() const {
        return !!m_backend;
    }

    UP<CManager>& manager() {
        static UP<CManager> mgr;
        return mgr;
    }
}
