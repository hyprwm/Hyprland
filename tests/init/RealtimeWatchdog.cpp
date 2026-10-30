#include <init/RealtimeWatchdog.hpp>

#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <vector>

#ifdef HAS_RTKIT
#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
#endif

#include <gtest/gtest.h>

using namespace NInit;
using namespace std::chrono_literals;

static Time::steady_tp at(std::chrono::nanoseconds time) {
    return Time::steady_tp{} + time;
}

static SThreadSample sampleAt(std::chrono::nanoseconds time, std::chrono::nanoseconds cpu, uint64_t switches, bool blocked) {
    return {.time = at(time), .cpuBefore = cpu, .voluntarySwitches = switches, .blocked = blocked, .cpuAfter = cpu, .realtime = true};
}

TEST(RealtimeWatchdog, sleptSincePreviousSample) {
    // switched out after the previous sample, and ran for 10ms of the 20ms since
    EXPECT_EQ(lastSleptAt(at(0ms), sampleAt(20ms, 5ms, 1, false), sampleAt(40ms, 15ms, 2, false)), at(30ms));

    // ran the whole time: the sleep was right after the previous sample
    EXPECT_EQ(lastSleptAt(at(0ms), sampleAt(20ms, 5ms, 1, false), sampleAt(40ms, 25ms, 2, false)), at(20ms));
}

TEST(RealtimeWatchdog, lateSampleAfterFreeze) {
    // both threads were frozen for a suspend: barely any CPU time since, so it slept just now
    EXPECT_EQ(lastSleptAt(at(0ms), sampleAt(20ms, 5ms, 1, false), sampleAt(1020ms, 6ms, 2, false)), at(1019ms));
}

TEST(RealtimeWatchdog, lateSampleKeepsLongRun) {
    // the watchdog was held up while the thread slept once, then ran for 500ms
    EXPECT_EQ(lastSleptAt(at(0ms), sampleAt(20ms, 5ms, 1, false), sampleAt(520ms, 505ms, 2, false)), at(20ms));
}

TEST(RealtimeWatchdog, delayedReadKeepsRun) {
    // the watchdog was held up for 60ms between reading the status and the CPU clock. The thread slept right after the
    // status read, then ran through the delay and on: that run counts, though it shows in the previous sample's CPU time
    const SThreadSample PREV = {.time = at(0ms), .cpuBefore = 0ms, .voluntarySwitches = 0, .blocked = false, .cpuAfter = 60ms, .realtime = true};
    EXPECT_EQ(lastSleptAt(at(0ms), PREV, sampleAt(120ms, 120ms, 1, false)), at(0ms));
}

TEST(RealtimeWatchdog, asleepWithoutRunning) {
    const auto NOW = sampleAt(40ms, 5ms, 1, true);
    EXPECT_EQ(lastSleptAt(at(0ms), sampleAt(20ms, 5ms, 1, true), NOW), NOW.time);
}

TEST(RealtimeWatchdog, runningKeepsLastSleep) {
    EXPECT_EQ(lastSleptAt(at(10ms), sampleAt(20ms, 5ms, 1, false), sampleAt(40ms, 25ms, 1, false)), at(10ms));

    // shown asleep, but it ran since the previous sample: it is about to switch out, which hasn't happened yet
    EXPECT_EQ(lastSleptAt(at(10ms), sampleAt(20ms, 5ms, 1, false), sampleAt(40ms, 25ms, 1, true)), at(10ms));
}

// Simulates random runs and sleeps in 1ms steps, including long runs like a stuck syscall, sampled every 19ms or, with
// late samples, sometimes after up to 600ms. Since its last wakeup, the thread never ran for longer than the time since
// lastSleptAt. With on time samples, it's also never more than two intervals behind.
static void checkLastSleep(bool lateSamples) {
    constexpr auto POLL = 19ms;
    std::mt19937   rng(16648);
    for (int round = 0; round < 200; ++round) {
        std::vector<bool> running;
        while (running.size() < 4000) {
            const bool   RUN    = rng() % 2;
            const size_t LENGTH = 1 + (rng() % (RUN ? 300 : 40));
            running.insert(running.end(), LENGTH, RUN);
        }

        std::chrono::nanoseconds     cpu{0};
        std::chrono::nanoseconds     run{0};
        uint64_t                     switches = 0;
        std::optional<SThreadSample> prev;
        Time::steady_tp              lastSlept;
        size_t                       nextSample = POLL.count();
        for (size_t step = 0; step < running.size(); ++step) {
            if (running[step]) {
                cpu += 1ms;
                run += 1ms;
            } else {
                switches += step > 0 && running[step - 1];
                run = 0ms;
            }

            if (step + 1 != nextSample)
                continue;

            nextSample += lateSamples && rng() % 20 == 0 ? 100 + (rng() % 500) : POLL.count();

            const auto SAMPLE = sampleAt(std::chrono::milliseconds{step + 1}, cpu, switches, !running[step]);
            lastSlept         = prev ? lastSleptAt(lastSlept, *prev, SAMPLE) : at(0ms);
            prev              = SAMPLE;
            ASSERT_LE(run, SAMPLE.time - lastSlept) << "round " << round << ", step " << step;
            ASSERT_TRUE(lateSamples || SAMPLE.time - lastSlept <= run + (2 * POLL)) << "round " << round << ", step " << step;
        }
    }
}

TEST(RealtimeWatchdog, lastSleepBoundsRunSinceWakeup) {
    checkLastSleep(false);
}

TEST(RealtimeWatchdog, lastSleepBoundsRunSinceWakeupWithLateSamples) {
    checkLastSleep(true);
}

TEST(RealtimeWatchdog, parsesThreadStatus) {
    constexpr std::string_view STATUS = "Name:\tHyprland\n"
                                        "Umask:\t0022\n"
                                        "State:\tS (sleeping)\n"
                                        "Tgid:\t1234\n"
                                        "Cpus_allowed_list:\t0-31\n"
                                        "voluntary_ctxt_switches:\t5678\n"
                                        "nonvoluntary_ctxt_switches:\t90\n";

    SThreadSample              sample;
    ASSERT_TRUE(parseThreadStatus(STATUS, sample));
    EXPECT_EQ(sample.voluntarySwitches, 5678U);
    EXPECT_TRUE(sample.blocked);
}

static bool blockedIn(std::string_view state) {
    SThreadSample sample;
    EXPECT_TRUE(parseThreadStatus(std::format("State:\t{}\nvoluntary_ctxt_switches:\t1\n", state), sample));
    return sample.blocked;
}

TEST(RealtimeWatchdog, parsesThreadState) {
    for (const auto STATE : {
             "S (sleeping)",
             "D (disk sleep)",
             "I (idle)",
             "T (stopped)",
             "t (tracing stop)",
         }) {
        EXPECT_TRUE(blockedIn(STATE)) << STATE;
    }

    for (const auto STATE : {
             "R (running)",
             "Z (zombie)",
             "X (dead)",
         }) {
        EXPECT_FALSE(blockedIn(STATE)) << STATE;
    }
}

TEST(RealtimeWatchdog, matchesWholeStatusKeys) {
    // "voluntary_ctxt_switches" is also the tail of "nonvoluntary_ctxt_switches"
    SThreadSample sample;
    ASSERT_TRUE(parseThreadStatus("State:\tS (sleeping)\nnonvoluntary_ctxt_switches:\t90\nvoluntary_ctxt_switches:\t12\n", sample));
    EXPECT_EQ(sample.voluntarySwitches, 12U);
    EXPECT_FALSE(parseThreadStatus("State:\tS (sleeping)\nnonvoluntary_ctxt_switches:\t90\n", sample));
}

TEST(RealtimeWatchdog, parsesLastLineWithoutNewline) {
    SThreadSample sample;
    ASSERT_TRUE(parseThreadStatus("State:\tR (running)\nvoluntary_ctxt_switches:\t8", sample));
    EXPECT_EQ(sample.voluntarySwitches, 8U);
}

TEST(RealtimeWatchdog, rejectsBadThreadStatus) {
    SThreadSample sample;
    EXPECT_FALSE(parseThreadStatus("", sample));
    EXPECT_FALSE(parseThreadStatus("voluntary_ctxt_switches:\t1\n", sample));
    EXPECT_FALSE(parseThreadStatus("State:\tS (sleeping)\nvoluntary_ctxt_switches:\tgarbage\n", sample));
    EXPECT_FALSE(parseThreadStatus("State:\tS (sleeping)\nvoluntary_ctxt_switches:\t12abc\n", sample));
}

#ifdef __linux__
TEST(RealtimeWatchdog, parsesOwnThreadStatus) {
    std::ifstream file("/proc/thread-self/status");
    ASSERT_TRUE(file.is_open());

    std::stringstream status;
    status << file.rdbuf();

    // reading its own status, so it is running
    SThreadSample sample;
    ASSERT_TRUE(parseThreadStatus(status.str(), sample));
    EXPECT_FALSE(sample.blocked);
}
#endif

#ifdef HAS_RTKIT
// What the death tests' children exit with when they pass. Not 0, so an exit(0) along some other path can't pass.
constexpr int PASSED = 42;

// Returns the calling thread's scheduling policy. Raw syscall, as musl's sched_getscheduler returns ENOSYS.
static int currentPolicy() {
    return sc<int>(syscall(SYS_sched_getscheduler, 0)) & ~SCHED_RESET_ON_FORK;
}

static bool isRealtime(int policy) {
    return policy == SCHED_RR || policy == SCHED_FIFO;
}

// Puts a thread of this process on SCHED_RR like Hyprland does without rtkit. Needs CAP_SYS_NICE or a RLIMIT_RTPRIO.
static bool setThreadRealtime(pid_t tid, int prio) {
    const sched_param param = {.sched_priority = prio};
    return syscall(SYS_sched_setscheduler, tid, SCHED_RR | SCHED_RESET_ON_FORK, &param) == 0;
}

// Returns whether this process may go realtime, trying on a short lived thread so the test thread stays as it is.
static bool canGoRealtime() {
    bool can = false;
    std::thread([&] { can = setThreadRealtime(gettid(), 1); }).join();
    return can;
}

// Returns a hard RLIMIT_RTTIME the watchdog runs under, at most 200ms and no higher than the current one, which can't
// be raised. Zero if that's too short to watch.
static rlim_t watchableHardLimit() {
    rlimit limit = {};
    if (getrlimit(RLIMIT_RTTIME, &limit))
        return 0;

    const rlim_t HARD = std::min<rlim_t>(limit.rlim_max, 200000);
    return HARD >= REALTIME_LIMIT_MIN_US ? HARD : 0;
}

// Runs the watchdog on a thread that isn't realtime, which it takes as having lost realtime, and exits with PASSED if it
// fell back right away. Meant for a child process: a lowered hard limit can't be raised back, and the watchdog changes
// the thread's scheduling flags.
[[noreturn]] static void fallBackOnNonRealtimeThread() {
    const rlim_t HARD  = watchableHardLimit();
    const rlimit LIMIT = {.rlim_cur = HARD / 4 * 3, .rlim_max = HARD};
    if (setrlimit(RLIMIT_RTTIME, &LIMIT))
        _exit(2);

    std::mutex              mutex;
    std::condition_variable fellBack;
    pid_t                   madeRealtime     = 0;
    pid_t                   madeHighPriority = 0;
    startRealtimeWatchdog(
        1,
        [&](pid_t tid, int) {
            std::scoped_lock lock(mutex);
            madeRealtime = tid;
            return true;
        },
        [&](pid_t tid) {
            std::scoped_lock lock(mutex);
            madeHighPriority = tid;
            fellBack.notify_all();
            return true;
        });

    {
        std::unique_lock lock(mutex);
        fellBack.wait_for(lock, 5s, [&] { return madeHighPriority != 0; });
    }

    stopRealtimeWatchdog();
    _exit(madeHighPriority == gettid() && madeRealtime != 0 && madeRealtime != gettid() ? PASSED : 1);
}

// Starts the watchdog under a hard limit too short to watch, and exits with PASSED if it fell back without starting.
[[noreturn]] static void fallBackOnShortLimit() {
    const rlimit LIMIT = {.rlim_cur = 5000, .rlim_max = 5000};
    if (setrlimit(RLIMIT_RTTIME, &LIMIT))
        _exit(2);

    bool  madeRealtime     = false;
    pid_t madeHighPriority = 0;
    startRealtimeWatchdog(
        1, [&](pid_t, int) { return madeRealtime = true; },
        [&](pid_t tid) {
            madeHighPriority = tid;
            return true;
        });

    stopRealtimeWatchdog();
    _exit(!madeRealtime && madeHighPriority == gettid() ? PASSED : 1);
}

// Goes realtime with the soft limit at the hard one, so the kernel SIGKILLs at the limit without a SIGXCPU first, and
// spins in userspace for twice the limit. Exits with PASSED if the watchdog itself demoted the thread, past half the
// limit and in time.
[[noreturn]] static void surviveOverrun() {
    const rlim_t HARD  = watchableHardLimit();
    const rlimit LIMIT = {.rlim_cur = HARD, .rlim_max = HARD};
    if (setrlimit(RLIMIT_RTTIME, &LIMIT) || !setThreadRealtime(gettid(), 1))
        _exit(2);

    std::atomic<pid_t>                                    watchdog    = 0;
    std::atomic<bool>                                     raised      = false;
    const auto                                            START       = std::chrono::steady_clock::now();
    std::atomic<std::chrono::steady_clock::duration::rep> raisedAfter = 0;
    startRealtimeWatchdog(
        1,
        [&](pid_t tid, int prio) {
            watchdog = tid;
            return setThreadRealtime(tid, prio);
        },
        [&](pid_t) {
            raisedAfter   = (std::chrono::steady_clock::now() - START).count();
            return raised = true;
        });

    // clock reads normally go through the vDSO, and a syscall instead wouldn't sleep either
    const auto END = START + std::chrono::microseconds{HARD * 2};
    while (std::chrono::steady_clock::now() < END) {
        ;
    }

    const bool DEMOTED = !isRealtime(currentPolicy());
    const auto AFTER   = std::chrono::steady_clock::duration{raisedAfter};
    stopRealtimeWatchdog();

    const bool BY_WATCHDOG = watchdog != 0 && watchdog != gettid();
    const bool IN_TIME     = AFTER >= std::chrono::microseconds{HARD / 2} && AFTER < std::chrono::microseconds{HARD};
    _exit(DEMOTED && raised && BY_WATCHDOG && IN_TIME ? PASSED : 1);
}

TEST(RealtimeWatchdog, samplesOwnThread) {
    clockid_t cpuClock = 0;
    ASSERT_EQ(pthread_getcpuclockid(pthread_self(), &cpuClock), 0);

    const Hyprutils::OS::CFileDescriptor STATUS{open("/proc/thread-self/status", O_RDONLY | O_CLOEXEC)};
    ASSERT_TRUE(STATUS.isValid());

    const auto SAMPLE = sampleThread(cpuClock, gettid(), STATUS.get());
    ASSERT_TRUE(SAMPLE.has_value());
    EXPECT_FALSE(SAMPLE.value().blocked);
    EXPECT_GT(SAMPLE.value().cpuBefore, 0ns);
    EXPECT_GE(SAMPLE.value().cpuAfter, SAMPLE.value().cpuBefore);
    EXPECT_EQ(SAMPLE.value().realtime, isRealtime(currentPolicy()));
}

// gtest runs suites named *DeathTest first, so these fork before other tests leave threads running whose locks the
// children would inherit. Its threadsafe style doesn't fork but re-executes argv[0], which fails when the binary was
// found through PATH, as in CI.
TEST(RealtimeWatchdogDeathTest, fallsBackWhenThreadIsNotRealtime) {
    if (watchableHardLimit() == 0)
        GTEST_SKIP() << "hard RLIMIT_RTTIME below 100ms";

    EXPECT_EXIT(fallBackOnNonRealtimeThread(), ::testing::ExitedWithCode(PASSED), "");
}

TEST(RealtimeWatchdogDeathTest, fallsBackWhenLimitIsTooShort) {
    // the fallback demotes, which from SCHED_IDLE needs a RLIMIT_NICE allowing it
    if (currentPolicy() == SCHED_IDLE)
        GTEST_SKIP() << "running as SCHED_IDLE";

    EXPECT_EXIT(fallBackOnShortLimit(), ::testing::ExitedWithCode(PASSED), "");
}

TEST(RealtimeWatchdogDeathTest, survivesOverrun) {
    if (watchableHardLimit() == 0 || !canGoRealtime())
        GTEST_SKIP() << "can't go realtime (needs CAP_SYS_NICE or a RLIMIT_RTPRIO), or hard RLIMIT_RTTIME below 100ms";

    EXPECT_EXIT(surviveOverrun(), ::testing::ExitedWithCode(PASSED), "");
}

TEST(RealtimeWatchdog, notNeededWithoutHardLimit) {
    rlimit limit = {};
    ASSERT_EQ(getrlimit(RLIMIT_RTTIME, &limit), 0);
    if (limit.rlim_max != RLIM_INFINITY)
        GTEST_SKIP() << "hard RLIMIT_RTTIME is finite";

    bool called = false;
    startRealtimeWatchdog(1, [&](pid_t, int) { return called = true; }, [&](pid_t) { return called = true; });
    stopRealtimeWatchdog();
    EXPECT_FALSE(called);
}
#endif
