#include <hyprtoolkit/EventLoop.hpp>
#include <hyprtoolkit/core/Timer.hpp>

#include <wayland-server-core.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace Toolkit;

namespace {
    class CWaylandLoop {
      public:
        CWaylandLoop() : m_loop(wl_event_loop_create()) {
            ;
        }

        ~CWaylandLoop() {
            wl_event_loop_destroy(m_loop);
        }

        wl_event_loop* get() const {
            return m_loop;
        }

        void dispatchAfter(std::chrono::milliseconds delay) const {
            std::this_thread::sleep_for(delay);
            wl_event_loop_dispatch(m_loop, 0);
        }

      private:
        wl_event_loop* m_loop = nullptr;
    };
}

TEST(ToolkitEventLoop, idlesFromOtherThreadsRunOnTheLoop) {
    CWaylandLoop    loop;
    CEventLoop      eventLoop(loop.get());

    std::thread::id ranOn;
    std::thread([&] { eventLoop.addIdle([&] { ranOn = std::this_thread::get_id(); }); }).join();

    EXPECT_EQ(ranOn, std::thread::id{});

    loop.dispatchAfter(std::chrono::milliseconds(0));
    EXPECT_EQ(ranOn, std::this_thread::get_id());
}

TEST(ToolkitEventLoop, cancelledTimersDoNotFire) {
    CWaylandLoop loop;
    CEventLoop   eventLoop(loop.get());

    int          fired = 0;
    const auto   KEPT  = eventLoop.addTimer(std::chrono::milliseconds(1), [&](auto, void*) { fired += 1; }, nullptr);
    const auto   GONE  = eventLoop.addTimer(std::chrono::milliseconds(1), [&](auto, void*) { fired += 10; }, nullptr);
    GONE->cancel();

    loop.dispatchAfter(std::chrono::milliseconds(10));
    EXPECT_EQ(fired, 1);
}

TEST(ToolkitEventLoop, cancelPendingDropsIdlesAndTimers) {
    CWaylandLoop loop;
    CEventLoop   eventLoop(loop.get());

    int          ran = 0;
    eventLoop.addIdle([&] { ran++; });
    eventLoop.addTimer(std::chrono::milliseconds(1), [&](auto, void*) { ran++; }, nullptr);
    eventLoop.cancelPending();

    loop.dispatchAfter(std::chrono::milliseconds(10));
    EXPECT_EQ(ran, 0);
}
