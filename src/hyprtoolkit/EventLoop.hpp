#pragma once

#include <hyprtoolkit/core/BackendServices.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../helpers/memory/Memory.hpp"

struct wl_event_loop;
struct wl_event_source;

namespace Toolkit {
    // Runs hyprtoolkit's work on the compositor's wayland loop.
    // addIdle() is thread-safe, hyprtoolkit posts idles from its resource threads.
    class CEventLoop : public Hyprtoolkit::IEventLoop {
      public:
        CEventLoop(wl_event_loop* loop);
        virtual ~CEventLoop();

        CEventLoop(const CEventLoop&)                                 = delete;
        CEventLoop&                      operator=(const CEventLoop&) = delete;

        virtual void                     addFd(int fd, std::function<void()>&& callback) override;
        virtual void                     removeFd(int fd) override;
        virtual ASP<Hyprtoolkit::CTimer> addTimer(const Hyprtoolkit::TimerDuration& timeout, std::function<void(ASP<Hyprtoolkit::CTimer> self, void* data)> callback, void* data,
                                                  bool force = false) override;
        virtual void                     addIdle(const std::function<void()>& callback) override;
        virtual void                     cancelPending() override;
        virtual void                     enterLoop() override;

      private:
        struct SFdSource {
            ~SFdSource();

            wl_event_source*      source = nullptr;
            std::function<void()> fn;
        };

        struct STimerSource {
            ~STimerSource();

            CEventLoop*              self   = nullptr;
            wl_event_source*         source = nullptr;
            ASP<Hyprtoolkit::CTimer> timer;
        };

        static int                             onFd(int fd, uint32_t mask, void* data);
        static int                             onTimer(void* data);
        static int                             onIdleWake(int fd, uint32_t mask, void* data);

        void                                   dispatchIdles();

        wl_event_loop*                         m_loop = nullptr;
        std::unordered_map<int, UP<SFdSource>> m_fds;
        std::vector<UP<STimerSource>>          m_timers;

        Hyprutils::OS::CFileDescriptor         m_idleFd;
        wl_event_source*                       m_idleSource = nullptr;
        std::mutex                             m_idleMutex;
        std::vector<std::function<void()>>     m_idles;
    };
}
