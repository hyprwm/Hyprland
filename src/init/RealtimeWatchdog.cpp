#include "RealtimeWatchdog.hpp"

#include <algorithm>
#include <charconv>

#ifdef HAS_RTKIT
#include <array>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <mutex>
#include <new>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "../debug/log/Logger.hpp"
#include "../helpers/memory/Memory.hpp"

using namespace Hyprutils::OS;
#endif

using namespace NInit;

// Returns the value of a "key:\tvalue" line from a /proc status file.
static std::optional<std::string_view> statusValue(std::string_view status, std::string_view key) {
    while (!status.empty()) {
        const auto END  = std::min(status.find('\n'), status.size());
        auto       line = status.substr(0, END);
        status.remove_prefix(std::min(END + 1, status.size()));

        // whole keys only, "voluntary_ctxt_switches" is also the tail of "nonvoluntary_ctxt_switches"
        if (!line.starts_with(key) || !line.substr(key.size()).starts_with(':'))
            continue;

        line.remove_prefix(key.size() + 1);
        line.remove_prefix(std::min(line.find_first_not_of(" \t"), line.size()));
        return line;
    }

    return std::nullopt;
}

bool NInit::parseThreadStatus(std::string_view status, SThreadSample& sample) {
    const auto STATE    = statusValue(status, "State");
    const auto SWITCHES = statusValue(status, "voluntary_ctxt_switches");
    if (!STATE || STATE->empty() || !SWITCHES)
        return false;

    uint64_t   switches = 0;
    const auto RESULT   = std::from_chars(SWITCHES->data(), SWITCHES->data() + SWITCHES->size(), switches);
    if (RESULT.ec != std::errc{} || RESULT.ptr != SWITCHES->data() + SWITCHES->size())
        return false;

    // off the run queue: S and D are sleeps, I an idle wait, T and t stopped or traced. Each wakes up like a sleep
    sample.voluntarySwitches = switches;
    sample.blocked           = std::string_view{"SDITt"}.contains(STATE->front());
    return true;
}

Time::steady_tp NInit::lastSleptAt(const Time::steady_tp& lastSlept, const SThreadSample& prev, const SThreadSample& now) {
    // A voluntary switch means it slept after prev's status was read. It can't have run for longer since than the CPU
    // time it used from just before that read, which places the sleep when this sample comes late, e.g. after both
    // threads were frozen for a suspend. The CPU time leaves out interrupts, which can place the sleep later than it
    // was, by at most the interrupt time between the two samples.
    if (now.voluntarySwitches != prev.voluntarySwitches)
        return std::max(prev.time, now.time - (now.cpuAfter - prev.cpuBefore));

    // off the run queue and didn't run since prev: it can only run again after a wakeup. /proc can show a thread as
    // asleep for an instant before it switches out, hence the CPU time check, around the status read
    if (now.blocked && now.cpuAfter == prev.cpuAfter)
        return now.time;

    return lastSlept;
}

#ifdef HAS_RTKIT

// Above this hard limit, the deadline is clamped: nothing realistic runs that long without sleeping.
constexpr rlim_t LIMIT_MAX_US = 60'000'000;

// Puts a thread of this process back on SCHED_OTHER, keeping its nice value. Keeps SCHED_RESET_ON_FORK, as realtime
// was granted with it set (on both paths) and clearing it needs CAP_SYS_NICE: the kernel rejects the whole call with
// EPERM otherwise. Async-signal-safe.
static bool setThreadNormal(pid_t tid) {
    const struct sched_param param = {.sched_priority = 0};
    return syscall(SYS_sched_setscheduler, tid, SCHED_OTHER | SCHED_RESET_ON_FORK, &param) == 0;
}

// Cached for the SIGXCPU handler: it must demote the RT thread specifically, but a process-directed signal can be
// delivered to any thread.
static pid_t mainThreadTid = 0;

// Demotes the main thread back to SCHED_OTHER instead of letting the process die. With a finite RLIMIT_RTTIME, an RT
// thread that overruns the soft limit without making a blocking syscall gets SIGXCPU (fatal by default), then SIGKILL
// at the hard limit. This only catches overruns that come back to userspace before the hard limit, see
// CRealtimeWatchdog for the others.
// Only async-signal-safe calls in here: raw syscalls and write().
static void handleSigxcpu(int /* signo */) {
    const int savedErrno = errno;

    if (setThreadNormal(mainThreadTid)) {
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), dropping realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    } else {
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), failed to drop realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    }

    errno = savedErrno;
}

// Installs handleSigxcpu, set up to demote the calling thread. Must happen before the thread can go realtime, and stays
// installed for the lifetime of the process: SIGXCPU is not delivered to well-behaved non-realtime processes.
bool NInit::installSigxcpuHandler() {
    mainThreadTid = gettid();

    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_flags   = SA_RESTART;
    action.sa_handler = handleSigxcpu;

    if (sigaction(SIGXCPU, &action, nullptr)) {
        LOG(Log::WARN, "Failed to install the SIGXCPU handler: {}, not attempting realtime", strerror(errno));
        return false;
    }

    return true;
}

// Blocks the asynchronous signals on the calling thread and returns the previous mask. Threads started meanwhile inherit
// the mask, which leaves those signals to the main thread: Hyprland's handlers expect to run there (SIGUSR1's even
// throws to unwind a timed out event). SIGXCPU stays, its handler works from any thread, and so do the signals a fault
// raises.
static sigset_t blockAsyncSignals() {
    sigset_t set = {};
    sigfillset(&set);
    for (const int SIG : {
             SIGXCPU,
             SIGSEGV,
             SIGBUS,
             SIGFPE,
             SIGILL,
             SIGTRAP,
             SIGSYS,
             SIGABRT,
         }) {
        sigdelset(&set, SIG);
    }

    sigset_t previous = {};
    pthread_sigmask(SIG_BLOCK, &set, &previous);
    return previous;
}

std::optional<SThreadSample> NInit::sampleThread(clockid_t cpuClock, pid_t tid, int statusFd) {
    SThreadSample sample = {.time = Time::steadyNow()};

    timespec      cpu = {};
    if (clock_gettime(cpuClock, &cpu))
        return std::nullopt;

    sample.cpuBefore = std::chrono::seconds{cpu.tv_sec} + std::chrono::nanoseconds{cpu.tv_nsec};

    std::array<char, 16384> buf = {};
    const auto              LEN = pread(statusFd, buf.data(), buf.size(), 0);
    if (LEN <= 0 || !parseThreadStatus({buf.data(), sc<size_t>(LEN)}, sample))
        return std::nullopt;

    if (clock_gettime(cpuClock, &cpu))
        return std::nullopt;

    sample.cpuAfter = std::chrono::seconds{cpu.tv_sec} + std::chrono::nanoseconds{cpu.tv_nsec};

    const auto POLICY = syscall(SYS_sched_getscheduler, tid);
    if (POLICY < 0)
        return std::nullopt;

    sample.realtime = (POLICY & ~SCHED_RESET_ON_FORK) == SCHED_RR || (POLICY & ~SCHED_RESET_ON_FORK) == SCHED_FIFO;
    return sample;
}

namespace NInit {
    // Watches the realtime main thread from a second thread. The kernel counts the time a realtime thread runs without
    // sleeping, and on most architectures only checks that count when the thread returns to userspace, hard limit first:
    // a single syscall that runs past the RLIMIT_RTTIME hard limit (e.g. a connector probe or a modeset on some drivers)
    // gets the whole process SIGKILLed, and a SIGXCPU handler never runs. If the main thread goes too long without being
    // seen sleeping, or loses realtime some other way, the watchdog demotes it to SCHED_OTHER, which stops the count
    // wherever the thread is, asks for a high SCHED_OTHER priority instead, and stops. Realtime isn't restored.
    //
    // Known gaps: under proxy execution a thread blocked on a kernel mutex keeps being counted while it looks asleep, a
    // sleep cut short by a pending signal looks like a real one, a thread preempted on its way to sleep for a whole poll
    // and woken meanwhile looks like it slept, and a freezer (e.g. around a suspend) can stop the watchdog while the main
    // thread is still stuck in a syscall.
    class CRealtimeWatchdog {
      public:
        // Starts watching the calling thread, which must have just become realtime at priority prio. hardLimitUs is the
        // RLIMIT_RTTIME hard limit. Returns nullptr if the thread can't be watched.
        static UP<CRealtimeWatchdog> create(rlim_t hardLimitUs, int prio, const FMakeRealtime& makeRealtime, const FMakeHighPriority& makeHighPriority);

        CRealtimeWatchdog(pid_t watchedTid, clockid_t cpuClock, CFileDescriptor&& statusFd, const SThreadSample& initial, std::chrono::nanoseconds limit,
                          const FMakeHighPriority& makeHighPriority);

        // Stops watching and demotes the watched thread, as nothing protects it anymore.
        ~CRealtimeWatchdog();

      private:
        void                     run(std::stop_token stop);
        bool                     demote();
        void                     raisePriority();

        pid_t                    m_pid        = getpid();
        pid_t                    m_watchedTid = 0;
        clockid_t                m_cpuClock   = 0;
        CFileDescriptor          m_statusFd;
        SThreadSample            m_initial;
        std::chrono::nanoseconds m_deadline{0};
        std::chrono::nanoseconds m_poll{0};
        FMakeHighPriority        m_makeHighPriority;
        std::atomic<pid_t>       m_watchdogTid = 0;

        // last, so it stops and joins before the state it uses is destroyed
        std::jthread m_thread;
    };
}

UP<CRealtimeWatchdog> CRealtimeWatchdog::create(rlim_t hardLimitUs, int prio, const FMakeRealtime& makeRealtime, const FMakeHighPriority& makeHighPriority) {
    clockid_t cpuClock = 0;
    if (pthread_getcpuclockid(pthread_self(), &cpuClock))
        return nullptr;

    CFileDescriptor statusFd{open("/proc/thread-self/status", O_RDONLY | O_CLOEXEC)};
    if (!statusFd.isValid())
        return nullptr;

    // the thread became realtime moments ago, only a few syscalls ran since
    const pid_t TID     = gettid();
    const auto  INITIAL = sampleThread(cpuClock, TID, statusFd.get());
    if (!INITIAL)
        return nullptr;

    const auto            LIMIT = std::chrono::microseconds{std::min(hardLimitUs, LIMIT_MAX_US)};
    UP<CRealtimeWatchdog> watchdog;
    {
        // the mask comes back on the way out, exceptions included
        const auto                    PREVIOUS = blockAsyncSignals();
        Hyprutils::Utils::CScopeGuard restore([&PREVIOUS] { pthread_sigmask(SIG_SETMASK, &PREVIOUS, nullptr); });
        try {
            watchdog = makeUnique<CRealtimeWatchdog>(TID, cpuClock, std::move(statusFd), *INITIAL, LIMIT, makeHighPriority);
        } catch (const std::exception& e) { LOG(Log::WARN, "realtime watchdog: failed to start: {}", e.what()); }
    }

    if (!watchdog)
        return nullptr;

    // Realtime at the watched thread's priority, so a busy system can't delay the watchdog. Not higher, which makes the
    // kernel keep it waiting on the watched thread's CPU rather than place it on another one, except with a single CPU
    // allowed: there only a higher priority gets it to run, and without realtime it can't run in time at all.
    watchdog->m_watchdogTid.wait(0);
    cpu_set_t  cpus       = {};
    const bool SINGLE_CPU = sched_getaffinity(0, sizeof(cpus), &cpus) == 0 && CPU_COUNT(&cpus) == 1;
    if (makeRealtime(watchdog->m_watchdogTid, SINGLE_CPU ? prio + 1 : prio))
        return watchdog;

    if (SINGLE_CPU) {
        LOG(Log::WARN, "realtime watchdog: failed to go realtime, which it needs on a single CPU");
        return nullptr;
    }

    LOG(Log::WARN, "realtime watchdog: failed to go realtime, it may react late under load");
    return watchdog;
}

// Sets the deadline and the poll interval as fractions of the hard limit. The kernel's count at the fallback is at most
// the time since the thread was last seen sleeping, so under deadline + poll, 7/10 of the limit. The rest absorbs the
// watchdog's own wakeup latency and the kernel counting in whole ticks.
CRealtimeWatchdog::CRealtimeWatchdog(pid_t watchedTid, clockid_t cpuClock, CFileDescriptor&& statusFd, const SThreadSample& initial, std::chrono::nanoseconds limit,
                                     const FMakeHighPriority& makeHighPriority) :
    m_watchedTid(watchedTid), m_cpuClock(cpuClock), m_statusFd(std::move(statusFd)), m_initial(initial), m_deadline(limit * 3 / 5), m_poll(limit / 10),
    m_makeHighPriority(makeHighPriority), m_thread([this](std::stop_token stop) { run(stop); }) {
    ;
}

CRealtimeWatchdog::~CRealtimeWatchdog() {
    // In a forked child the thread doesn't exist, and the watched thread is the parent's. Leak what's left of the
    // thread: joining it never returns, and even detaching it can wait on its leftovers.
    if (getpid() != m_pid) {
        std::ignore = new (std::nothrow) std::jthread(std::move(m_thread));
        return;
    }

    // stop first, so the watchdog can't act on the thread after this
    m_thread.request_stop();
    if (m_thread.joinable())
        m_thread.join();

    setThreadNormal(m_watchedTid);
}

void CRealtimeWatchdog::run(std::stop_token stop) {
    m_watchdogTid = gettid();
    m_watchdogTid.notify_one();
    pthread_setname_np(pthread_self(), "hypr-rtwatchdog");

    try {
        // only this thread takes the lock: the watchdog must never wait on another thread
        std::mutex                  mutex;
        std::condition_variable_any wakeup;
        std::unique_lock            lock(mutex);

        SThreadSample               prev      = m_initial;
        Time::steady_tp             lastSlept = m_initial.time;
        while (true) {
            // a predicate that never holds: a sleep that a stop request cuts short
            std::ignore = wakeup.wait_for(lock, stop, m_poll, [] { return false; });
            if (stop.stop_requested())
                return;

            // log after demoting: logging can block (it allocates, and allocating can wait on the address space lock
            // that the stuck thread may hold), and nothing may delay the demotion
            const auto SAMPLE = sampleThread(m_cpuClock, m_watchedTid, m_statusFd.get());
            if (!SAMPLE) {
                if (demote())
                    LOG(Log::ERR, "realtime watchdog: failed to sample the main thread, dropped realtime scheduling for the rest of the session");
                raisePriority();
                return;
            }

            lastSlept = lastSleptAt(lastSlept, prev, *SAMPLE);
            prev      = *SAMPLE;
            if (SAMPLE->realtime && SAMPLE->time - lastSlept < m_deadline)
                continue;

            if (!SAMPLE->realtime)
                LOG(Log::WARN, "realtime watchdog: main thread lost realtime scheduling");
            else if (demote())
                LOG(Log::WARN, "realtime watchdog: main thread not seen sleeping for {}ms, dropped realtime scheduling for the rest of the session",
                    std::chrono::duration_cast<std::chrono::milliseconds>(SAMPLE->time - lastSlept).count());

            raisePriority();
            return;
        }
    } catch (...) {
        // nothing in here may take the compositor down, but the main thread mustn't stay realtime unwatched either
        setThreadNormal(m_watchedTid);
    }
}

// Demotes the main thread. Returns false if that failed.
bool CRealtimeWatchdog::demote() {
    if (setThreadNormal(m_watchedTid))
        return true;

    LOG(Log::ERR, "realtime watchdog: failed to drop realtime scheduling: {}", strerror(errno));
    return false;
}

// Asks for a high SCHED_OTHER priority for the main thread, once it isn't realtime anymore.
void CRealtimeWatchdog::raisePriority() {
    if (!m_makeHighPriority(m_watchedTid))
        LOG(Log::WARN, "realtime watchdog: failed to get a high priority instead");
}

// Holds the realtime watchdog. Never destroyed: CCompositor's teardown can also run from static destructors (exit()
// from a Lua config), and stops the watchdog at its end.
static UP<CRealtimeWatchdog>& realtimeWatchdog() {
    static auto* const INSTANCE = new UP<CRealtimeWatchdog>();
    return *INSTANCE;
}

// Starts the realtime watchdog for the calling thread, which just became realtime at priority prio. Only needed when
// the kernel can SIGKILL the thread, i.e. when the RLIMIT_RTTIME hard limit is finite: always the case with rtkit,
// rarely with direct realtime. Drops realtime right away if the watchdog can't start, as without it a single long
// syscall gets Hyprland killed.
void NInit::startRealtimeWatchdog(int prio, const FMakeRealtime& makeRealtime, const FMakeHighPriority& makeHighPriority) {
    rlimit limit = {};
    if (getrlimit(RLIMIT_RTTIME, &limit) == 0 && limit.rlim_max == RLIM_INFINITY)
        return;

    if (limit.rlim_max >= REALTIME_LIMIT_MIN_US)
        realtimeWatchdog() = CRealtimeWatchdog::create(limit.rlim_max, prio, makeRealtime, makeHighPriority);

    if (realtimeWatchdog())
        return;

    if (!setThreadNormal(gettid())) {
        LOG(Log::ERR, "Failed to start the realtime watchdog, and to drop realtime scheduling: {}", strerror(errno));
        return;
    }

    LOG(Log::WARN, "Failed to start the realtime watchdog (RLIMIT_RTTIME hard limit {}us), dropped realtime scheduling", limit.rlim_max);
    if (!makeHighPriority(gettid()))
        LOG(Log::WARN, "Failed to get a high priority instead");
}

#endif

// Stops the realtime watchdog, if one runs, which drops the watched thread's realtime scheduling.
void NInit::stopRealtimeWatchdog() {
#ifdef HAS_RTKIT
    realtimeWatchdog().reset();
#endif
}
