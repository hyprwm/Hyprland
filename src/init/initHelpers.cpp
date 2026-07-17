#include <linux/capability.h>
#include <sys/prctl.h>

#include "initHelpers.hpp"

#ifdef HAS_RTKIT
#include <sys/resource.h>
#include <sys/syscall.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <sdbus-c++/sdbus-c++.h>
#include <hyprutils/memory/Casts.hpp>
#endif

bool NInit::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

// NixOS-specific fix to prevent all children from inheriting
// CAP_SYS_NICE due to how the security wrapper works.
void NInit::lowerAmbientCaps() {
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER, CAP_SYS_NICE, 0, 0);
}

// Asks the kernel directly to put the calling thread on SCHED_RR. Only succeeds if
// the process is privileged to go realtime on its own: CAP_SYS_NICE (e.g. granted via
// setcap or a security wrapper) or a nonzero RLIMIT_RTPRIO (e.g. a "realtime" group
// entry in limits.conf).
static bool trySchedDirect(int prio) {
#ifdef HAS_RTKIT
    // SCHED_RESET_ON_FORK keeps children from inheriting realtime, the same kernel mechanism
    // rtkit's grants use. Raw syscall because not every libc exposes the per-thread Linux
    // semantics of sched_setscheduler (musl returns ENOSYS).
    const struct sched_param param = {.sched_priority = prio};
    return syscall(SYS_sched_setscheduler, gettid(), SCHED_RR | SCHED_RESET_ON_FORK, &param) == 0;
#else
    int                oldPolicy = 0;
    struct sched_param param     = {};

    if (pthread_getschedparam(pthread_self(), &oldPolicy, &param)) {
        Log::logger->log(Log::WARN, "Failed to get old pthread scheduling priority");
        return false;
    }

    param.sched_priority = prio;

    return pthread_setschedparam(pthread_self(), SCHED_RR, &param) == 0;
#endif
}

#ifdef HAS_RTKIT

// Cached for the SIGXCPU handler: it must demote the RT thread specifically, but a
// process-directed signal can be delivered to any thread.
static pid_t mainThreadTid = 0;

// Demotes the main thread back to SCHED_OTHER instead of letting the process die. With a finite
// RLIMIT_RTTIME, an RT thread that overruns the soft limit without making a blocking syscall gets
// SIGXCPU (fatal by default), then SIGKILL at the hard limit. The counter resets every time the
// thread blocks (i.e. every frame), so this only fires on a genuinely runaway compositor.
// Only async-signal-safe calls in here: raw syscalls and write().
static void handleSigxcpu(int /* signo */) {
    const int savedErrno = errno;

    // realtime was granted with SCHED_RESET_ON_FORK set (on both paths), keep it as clearing it
    // needs CAP_SYS_NICE and the kernel rejects the whole call with EPERM otherwise.
    const struct sched_param param = {.sched_priority = 0};
    if (syscall(SYS_sched_setscheduler, mainThreadTid, SCHED_OTHER | SCHED_RESET_ON_FORK, &param) == 0) {
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), dropping realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    } else {
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), failed to drop realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    }

    errno = savedErrno;
}

// Installs handleSigxcpu. Must happen before the thread can go realtime, and stays installed for
// the lifetime of the process: SIGXCPU is not delivered to well-behaved non-realtime processes.
static bool installSigxcpuHandler() {
    mainThreadTid = gettid();

    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_flags   = SA_RESTART;
    action.sa_handler = handleSigxcpu;

    if (sigaction(SIGXCPU, &action, nullptr)) {
        Log::logger->log(Log::WARN, "Failed to install the SIGXCPU handler: {}, not attempting realtime", strerror(errno));
        return false;
    }

    return true;
}

constexpr const char* RTKIT_SERVICE      = "org.freedesktop.RealtimeKit1";
constexpr const char* RTKIT_OBJECT       = "/org/freedesktop/RealtimeKit1";
constexpr const char* RTKIT_INTERFACE    = "org.freedesktop.RealtimeKit1";
constexpr auto        RTKIT_DBUS_TIMEOUT = std::chrono::seconds(2); // D-Bus defaults to 25s, a wedged rtkit must not stall startup

// Asks rtkit (org.freedesktop.RealtimeKit1 on the system bus) to put the calling thread on
// SCHED_RR. rtkit hands out realtime scheduling to unprivileged processes, mediated by polkit
// (granted to active sessions only). It addresses threads by kernel tid (gettid), not pthread_t,
// and grants SCHED_RR with SCHED_RESET_ON_FORK set, so fork children reset to SCHED_OTHER
// kernel-side.
static bool tryRtkit(int prio) {
    try {
        auto connection = sdbus::createSystemBusConnection();
        auto proxy      = sdbus::createProxy(*connection, sdbus::ServiceName{RTKIT_SERVICE}, sdbus::ObjectPath{RTKIT_OBJECT});

        // read RTTimeUSecMax, the largest RLIMIT_RTTIME rtkit grants under.
        // Also probes rtkit's liveness before touching RLIMIT_RTTIME: lowering
        // the hard limit must not happen unless rtkit is actually there to
        // grant us realtime in exchange
        sdbus::Variant maxVar;
        proxy->callMethod("Get")
            .onInterface("org.freedesktop.DBus.Properties")
            .withTimeout(RTKIT_DBUS_TIMEOUT)
            .withArguments(RTKIT_INTERFACE, "RTTimeUSecMax")
            .storeResultsTo(maxVar);

        const auto maxUs = maxVar.get<int64_t>();
        if (maxUs <= 0) {
            Log::logger->log(Log::DEBUG, "rtkit: no usable RTTimeUSecMax, not requesting realtime");
            return false;
        }

        // rtkit refuses to grant realtime unless the caller has a finite RLIMIT_RTTIME at or
        // below its RTTimeUSecMax. Lowering the hard limit is irreversible without
        // CAP_SYS_RESOURCE and the new limit is inherited by forked children: apps launched from
        // Hyprland that gain realtime through their own privileges will live under this budget
        // too.
        rlimit current = {};
        if (getrlimit(RLIMIT_RTTIME, &current)) {
            Log::logger->log(Log::WARN, "rtkit: failed to get RLIMIT_RTTIME: {}", strerror(errno));
            return false;
        }

        // an unprivileged process cannot raise a finite hard limit, so stay under a pre-existing
        // one (RLIM_INFINITY compares greater than any finite limit). Keep soft below hard: an
        // overrun then raises a catchable SIGXCPU (which our handler turns into a demotion)
        // before the uncatchable SIGKILL at the hard limit.
        const rlim_t hard     = std::min(current.rlim_max, sc<rlim_t>(maxUs));
        const rlimit newLimit = {.rlim_cur = std::min(hard / 4 * 3, current.rlim_cur), .rlim_max = hard};

        if (setrlimit(RLIMIT_RTTIME, &newLimit)) {
            Log::logger->log(Log::WARN, "rtkit: failed to set RLIMIT_RTTIME: {}", strerror(errno));
            return false;
        }

        // fails with a polkit denial for sessions that aren't active locally (e.g. ssh),
        // which is fine because realtime is best-effort. the armed RLIMIT_RTTIME must stay,
        // hard limits cannot be raised back.
        proxy->callMethod("MakeThreadRealtime").onInterface(RTKIT_INTERFACE).withTimeout(RTKIT_DBUS_TIMEOUT).withArguments(sc<uint64_t>(gettid()), sc<uint32_t>(prio));

        return true;
    } catch (const sdbus::Error& e) {
        Log::logger->log(Log::DEBUG, "rtkit: {}", e.what());
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
#endif

    gained = trySchedDirect(minPrio);
    if (gained)
        Log::logger->log(Log::DEBUG, "Gained realtime scheduling directly");

#ifdef HAS_RTKIT
    if (!gained && (gained = tryRtkit(minPrio)))
        Log::logger->log(Log::DEBUG, "Gained realtime scheduling via rtkit");
#endif

    if (!gained) {
        Log::logger->log(Log::WARN, "Failed to gain realtime scheduling");
        return;
    }

#ifndef HAS_RTKIT
    // spawned children must not inherit RT, on Linux the kernel handles this via
    // SCHED_RESET_ON_FORK on both promotion paths
    pthread_atfork(nullptr, nullptr, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            Log::logger->log(Log::WARN, "Failed to reset process scheduling strategy");
    });
#endif
}
