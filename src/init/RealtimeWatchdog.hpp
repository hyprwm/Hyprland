#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <sys/types.h>

#include "../helpers/time/Time.hpp"

namespace NInit {
    // A snapshot of a thread's scheduling state, read in field order.
    struct SThreadSample {
        Time::steady_tp          time;                      // taken before anything else is read
        std::chrono::nanoseconds cpuBefore{0};              // CPU time used so far, read before the status
        uint64_t                 voluntarySwitches = 0;     // how many times it blocked
        bool                     blocked           = false; // off the run queue when sampled
        std::chrono::nanoseconds cpuAfter{0};               // CPU time used so far, read after the status
        bool                     realtime = false;          // scheduled as SCHED_RR or SCHED_FIFO
    };

    // Fills voluntarySwitches and blocked from a /proc/<pid>/task/<tid>/status file. Returns false if either is missing.
    bool parseThreadStatus(std::string_view status, SThreadSample& sample);

    // Returns when the thread last slept as far as two consecutive samples tell, given when that was so far.
    Time::steady_tp lastSleptAt(const Time::steady_tp& lastSlept, const SThreadSample& prev, const SThreadSample& now);

    using FMakeRealtime     = std::function<bool(pid_t tid, int prio)>;
    using FMakeHighPriority = std::function<bool(pid_t tid)>;

#ifdef HAS_RTKIT
    // The shortest RLIMIT_RTTIME hard limit the realtime watchdog can protect. Below it, the kernel counting in whole
    // ticks (up to 10ms each) and the watchdog's own wakeup latency eat the margin left after its deadline and a poll.
    inline constexpr uint64_t REALTIME_LIMIT_MIN_US = 100'000;

    // Samples a thread of this process, given its CPU clock and an open /proc/self/task/<tid>/status.
    std::optional<SThreadSample> sampleThread(clockid_t cpuClock, pid_t tid, int statusFd);

    bool                         installSigxcpuHandler();
    void                         startRealtimeWatchdog(int prio, const FMakeRealtime& makeRealtime, const FMakeHighPriority& makeHighPriority);
#endif
    void stopRealtimeWatchdog();
}
