#if !defined(__OpenBSD__)
#include <linux/capability.h>
#include <sys/prctl.h>
#endif

#include "initHelpers.hpp"

#ifdef HAS_RTKIT
#include <sys/resource.h>
#include <sys/syscall.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>

#include <sdbus-c++/sdbus-c++.h>
#include <hyprutils/memory/Casts.hpp>

#include "RealtimeWatchdog.hpp"
#endif

bool NInit::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

#if !defined(__OpenBSD__)
// NixOS-specific fix to prevent all children from inheriting
// CAP_SYS_NICE due to how the security wrapper works.
void NInit::lowerAmbientCaps() {
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER, CAP_SYS_NICE, 0, 0);
}
#endif

#ifdef HAS_RTKIT
// Puts a thread of this process on SCHED_RR. SCHED_RESET_ON_FORK keeps children from inheriting
// realtime, the same kernel mechanism rtkit's grants use. Raw syscall because not every libc
// exposes the per-thread Linux semantics of sched_setscheduler (musl returns ENOSYS).
static bool setThreadRealtime(pid_t tid, int prio) {
    const struct sched_param param = {.sched_priority = prio};
    return syscall(SYS_sched_setscheduler, tid, SCHED_RR | SCHED_RESET_ON_FORK, &param) == 0;
}

// Gives a thread of this process the highest SCHED_OTHER priority it may set, for the realtime
// watchdog falling back from realtime: nice -20 with CAP_SYS_NICE, otherwise as far as RLIMIT_NICE
// allows, like rtkit's MinNiceLevel is as far as rtkit allows. A RLIMIT_RTPRIO allowing realtime
// doesn't allow a negative nice by itself. Never raises the nice value.
static bool setThreadHighPriority(pid_t tid) {
    errno             = 0;
    const int CURRENT = getpriority(PRIO_PROCESS, tid);
    if (errno)
        return false;

    if (CURRENT <= -20 || setpriority(PRIO_PROCESS, tid, -20) == 0)
        return true;

    // RLIMIT_NICE lets the nice value go down to 20 - limit
    rlimit limit = {};
    if (getrlimit(RLIMIT_NICE, &limit) || limit.rlim_cur <= 20)
        return false;

    const int LOWEST = 20 - sc<int>(std::min<rlim_t>(limit.rlim_cur, 40));
    return CURRENT <= LOWEST || setpriority(PRIO_PROCESS, tid, LOWEST) == 0;
}
#endif

// Asks the kernel directly to put the calling thread on SCHED_RR. Only succeeds if
// the process is privileged to go realtime on its own: CAP_SYS_NICE (e.g. granted via
// setcap or a security wrapper) or a nonzero RLIMIT_RTPRIO (e.g. a "realtime" group
// entry in limits.conf).
static bool trySchedDirect(int prio) {
#ifdef HAS_RTKIT
    return setThreadRealtime(gettid(), prio);
#else
    int                oldPolicy = 0;
    struct sched_param param     = {};

    if (pthread_getschedparam(pthread_self(), &oldPolicy, &param)) {
        LOG(Log::WARN, "Failed to get old pthread scheduling priority");
        return false;
    }

    param.sched_priority = prio;

    return pthread_setschedparam(pthread_self(), SCHED_RR, &param) == 0;
#endif
}

#ifdef HAS_RTKIT

constexpr const char* RTKIT_SERVICE      = "org.freedesktop.RealtimeKit1";
constexpr const char* RTKIT_OBJECT       = "/org/freedesktop/RealtimeKit1";
constexpr const char* RTKIT_INTERFACE    = "org.freedesktop.RealtimeKit1";
constexpr auto        RTKIT_DBUS_TIMEOUT = std::chrono::seconds(2); // D-Bus defaults to 25s, a wedged rtkit must not stall startup

// An rtkit proxy on a system bus connection of its own. Throws sdbus::Error if rtkit can't be
// reached.
struct SRtkit {
    std::unique_ptr<sdbus::IConnection> connection = sdbus::createSystemBusConnection();
    std::unique_ptr<sdbus::IProxy>      proxy      = sdbus::createProxy(*connection, sdbus::ServiceName{RTKIT_SERVICE}, sdbus::ObjectPath{RTKIT_OBJECT});
};

// Reads one of rtkit's properties. Throws sdbus::Error on failure.
static sdbus::Variant rtkitProperty(sdbus::IProxy& proxy, const char* name) {
    sdbus::Variant value;
    proxy.callMethod("Get").onInterface("org.freedesktop.DBus.Properties").withTimeout(RTKIT_DBUS_TIMEOUT).withArguments(RTKIT_INTERFACE, name).storeResultsTo(value);
    return value;
}

// Asks rtkit to put a thread of this process on SCHED_RR. rtkit addresses threads by kernel tid
// (gettid), not pthread_t, and grants SCHED_RR with SCHED_RESET_ON_FORK set, so fork children
// reset to SCHED_OTHER kernel-side. Throws sdbus::Error on failure.
static void rtkitMakeThreadRealtime(sdbus::IProxy& proxy, pid_t tid, int prio) {
    proxy.callMethod("MakeThreadRealtime").onInterface(RTKIT_INTERFACE).withTimeout(RTKIT_DBUS_TIMEOUT).withArguments(sc<uint64_t>(tid), sc<uint32_t>(prio));
}

// Asks rtkit (org.freedesktop.RealtimeKit1 on the system bus) to put the calling thread on
// SCHED_RR. rtkit hands out realtime scheduling to unprivileged processes, mediated by polkit
// (granted to active sessions only).
static bool tryRtkit(int prio) {
    try {
        SRtkit rtkit;

        // read RTTimeUSecMax, the largest RLIMIT_RTTIME rtkit grants under.
        // Also probes rtkit's liveness before touching RLIMIT_RTTIME: lowering
        // the hard limit must not happen unless rtkit is actually there to
        // grant us realtime in exchange
        const auto maxUs = rtkitProperty(*rtkit.proxy, "RTTimeUSecMax").get<int64_t>();
        if (maxUs <= 0) {
            LOG(Log::DEBUG, "rtkit: no usable RTTimeUSecMax, not requesting realtime");
            return false;
        }

        // rtkit refuses to grant realtime unless the caller has a finite RLIMIT_RTTIME at or
        // below its RTTimeUSecMax. Lowering the hard limit is irreversible without
        // CAP_SYS_RESOURCE and the new limit is inherited by forked children: apps launched from
        // Hyprland that gain realtime through their own privileges will live under this budget
        // too.
        rlimit current = {};
        if (getrlimit(RLIMIT_RTTIME, &current)) {
            LOG(Log::WARN, "rtkit: failed to get RLIMIT_RTTIME: {}", strerror(errno));
            return false;
        }

        // an unprivileged process cannot raise a finite hard limit, so stay under a pre-existing
        // one (RLIM_INFINITY compares greater than any finite limit). Keep soft below hard: an
        // overrun in userspace then raises a catchable SIGXCPU (which our handler turns into a
        // demotion) before the uncatchable SIGKILL at the hard limit.
        const rlim_t hard     = std::min(current.rlim_max, sc<rlim_t>(maxUs));
        const rlimit newLimit = {.rlim_cur = std::min(hard / 4 * 3, current.rlim_cur), .rlim_max = hard};

        // the realtime watchdog couldn't protect realtime under a shorter limit, and would drop it right away
        if (hard < NInit::REALTIME_LIMIT_MIN_US) {
            LOG(Log::DEBUG, "rtkit: a RLIMIT_RTTIME hard limit of {}us is too short to run realtime safely, not requesting it", hard);
            return false;
        }

        if (setrlimit(RLIMIT_RTTIME, &newLimit)) {
            LOG(Log::WARN, "rtkit: failed to set RLIMIT_RTTIME: {}", strerror(errno));
            return false;
        }

        // fails with a polkit denial for sessions that aren't active locally (e.g. ssh),
        // which is fine because realtime is best-effort. the armed RLIMIT_RTTIME must stay,
        // hard limits cannot be raised back.
        rtkitMakeThreadRealtime(*rtkit.proxy, gettid(), prio);

        return true;
    } catch (const sdbus::Error& e) {
        LOG(Log::DEBUG, "rtkit: {}", e.what());
        return false;
    }
}

// Asks rtkit to put a thread of this process on SCHED_RR, for the realtime watchdog's own thread.
static bool rtkitSetThreadRealtime(pid_t tid, int prio) {
    try {
        SRtkit rtkit;
        rtkitMakeThreadRealtime(*rtkit.proxy, tid, prio);
        return true;
    } catch (const sdbus::Error& e) {
        LOG(Log::DEBUG, "rtkit: {}", e.what());
        return false;
    }
}

// Asks rtkit to give a thread of this process the highest SCHED_OTHER priority it grants, its
// MinNiceLevel, for the realtime watchdog falling back from realtime. Never raises the nice value.
static bool rtkitSetThreadHighPriority(pid_t tid) {
    try {
        SRtkit     rtkit;
        const auto MIN_NICE = rtkitProperty(*rtkit.proxy, "MinNiceLevel").get<int32_t>();

        errno             = 0;
        const int CURRENT = getpriority(PRIO_PROCESS, tid);
        if (errno == 0 && CURRENT <= MIN_NICE)
            return true;

        rtkit.proxy->callMethod("MakeThreadHighPriority").onInterface(RTKIT_INTERFACE).withTimeout(RTKIT_DBUS_TIMEOUT).withArguments(sc<uint64_t>(tid), MIN_NICE);
        return true;
    } catch (const sdbus::Error& e) {
        LOG(Log::DEBUG, "rtkit: {}", e.what());
        return false;
    }
}

#endif

// Puts the main thread on SCHED_RR so a busy session cannot starve the compositor of CPU.
// Going realtime directly needs privilege and is tried first, rtkit is the standard
// unprivileged fallback on desktop systems. Best-effort: on failure we just run as SCHED_OTHER.
void NInit::gainRealTime() {
    const int minPrio = sched_get_priority_min(SCHED_RR);
    bool      gained  = false;

#ifdef HAS_RTKIT
    if (!installSigxcpuHandler())
        return;

    // how the watchdog gets realtime for itself and a high priority for the main thread when it
    // falls back, the same way the main thread got realtime
    FMakeRealtime     makeRealtime     = setThreadRealtime;
    FMakeHighPriority makeHighPriority = setThreadHighPriority;
#endif

    gained = trySchedDirect(minPrio);
    if (gained)
        LOG(Log::DEBUG, "Gained realtime scheduling directly");

#ifdef HAS_RTKIT
    if (!gained && (gained = tryRtkit(minPrio))) {
        LOG(Log::DEBUG, "Gained realtime scheduling via rtkit");
        makeRealtime     = rtkitSetThreadRealtime;
        makeHighPriority = rtkitSetThreadHighPriority;
    }
#endif

    if (!gained) {
        LOG(Log::WARN, "Failed to gain realtime scheduling");
        return;
    }

#ifdef HAS_RTKIT
    startRealtimeWatchdog(minPrio, makeRealtime, makeHighPriority);
#else
    // spawned children must not inherit RT, on Linux the kernel handles this via
    // SCHED_RESET_ON_FORK on both promotion paths
    pthread_atfork(nullptr, nullptr, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            LOG(Log::WARN, "Failed to reset process scheduling strategy");
    });
#endif
}
