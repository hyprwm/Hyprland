#include <linux/capability.h>
#include <sys/prctl.h>

#include "initHelpers.hpp"

#if defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <optional>

#include <gio/gio.h>
#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#endif

bool NInit::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

// Asks the kernel directly to put the calling thread on SCHED_RR. Only succeeds if
// the process is privileged to go realtime on its own: CAP_SYS_NICE (e.g. granted via
// setcap or a security wrapper) or a nonzero RLIMIT_RTPRIO (e.g. a "realtime" group
// entry in limits.conf).
static bool trySchedDirect(int prio) {
#if defined(__linux__)
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

#if defined(__linux__)

// Cached for the SIGXCPU handler: it must demote the RT thread specifically, but a
// process-directed signal can be delivered to any thread.
static pid_t mainThreadTid = 0;

// The SIGXCPU disposition from before installSigxcpuHandler(), restored when realtime is not
// gained after all, and by the handler itself once it has demoted the thread.
static struct sigaction previousSigxcpuAction = {};

// Demotes the main thread back to SCHED_OTHER instead of letting the process die. With a finite
// RLIMIT_RTTIME, an RT thread that overruns the soft limit without making a blocking syscall gets
// SIGXCPU (fatal by default), then SIGKILL at the hard limit. The counter resets every time the
// thread blocks (i.e. every frame), so this only fires on a genuinely runaway compositor.
// Only async-signal-safe calls in here: raw syscalls, sigaction() and write().
static void handleSigxcpu(int /* signo */) {
    // realtime was granted with SCHED_RESET_ON_FORK set (on both paths), keep it as clearing it
    // needs CAP_SYS_NICE and the kernel rejects the whole call with EPERM otherwise.
    const struct sched_param param = {.sched_priority = 0};
    if (syscall(SYS_sched_setscheduler, mainThreadTid, SCHED_OTHER | SCHED_RESET_ON_FORK, &param) == 0) {
        // no longer realtime: hand SIGXCPU back to whatever handled it before us
        sigaction(SIGXCPU, &previousSigxcpuAction, nullptr);
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), dropping realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    } else {
        // demotion failed: leave the handler installed
        constexpr char MSG[] = "Realtime budget exceeded (SIGXCPU), failed to drop realtime scheduling\n";
        std::ignore          = write(STDERR_FILENO, MSG, sizeof(MSG) - 1);
    }
}

// Installs handleSigxcpu, saving the previous disposition. Installed before attempting promotion
// (a realtime thread must never run without it), retained only when realtime scheduling is
// successfully acquired.
static bool installSigxcpuHandler() {
    mainThreadTid = gettid();

    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_flags   = SA_RESTART;
    action.sa_handler = handleSigxcpu;

    if (sigaction(SIGXCPU, &action, &previousSigxcpuAction)) {
        Log::logger->log(Log::WARN, "Failed to install the SIGXCPU handler: {}, not attempting realtime", strerror(errno));
        return false;
    }

    return true;
}

constexpr const char* RTKIT_SERVICE         = "org.freedesktop.RealtimeKit1";
constexpr const char* RTKIT_OBJECT          = "/org/freedesktop/RealtimeKit1";
constexpr const char* RTKIT_INTERFACE       = "org.freedesktop.RealtimeKit1";
constexpr gint        RTKIT_DBUS_TIMEOUT_MS = 2000; // GDBus defaults to 25s, a wedged rtkit must not stall startup

// Reads rtkit's RTTimeUSecMax property (int64, µs): the largest RLIMIT_RTTIME it grants under.
// Doubles as the liveness probe for rtkit itself.
static std::optional<int64_t> rtkitGetRttimeMax(GDBusConnection* conn) {
    GError*   error = nullptr;
    GVariant* ret =
        g_dbus_connection_call_sync(conn, RTKIT_SERVICE, RTKIT_OBJECT, "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", RTKIT_INTERFACE, "RTTimeUSecMax"),
                                    G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, RTKIT_DBUS_TIMEOUT_MS, nullptr, &error);
    if (!ret) {
        Log::logger->log(Log::DEBUG, "rtkit: failed to read RTTimeUSecMax: {}", error->message);
        g_error_free(error);
        return std::nullopt;
    }

    GVariant* value = nullptr;
    g_variant_get(ret, "(v)", &value);

    std::optional<int64_t> result;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
        result = g_variant_get_int64(value);

    g_variant_unref(value);
    g_variant_unref(ret);
    return result;
}

// The realtime budget must exceed the longest CPU burst the compositor makes without blocking
// (the RTTIME counter resets whenever the thread blocks, which normally happens every frame), or
// ordinary load rather than a runaway would trip the limit. Normal bursts are a few ms regardless
// of refresh rate, this floor only rejects environments squeezed below usability, rtkit-provided
// budgets are far larger (200ms by default).
constexpr rlim_t MIN_RTTIME_US = 10000;

// Computes the RLIMIT_RTTIME to run realtime under: the current limits capped by maxUs, with the
// soft limit kept below hard. Shared by both promotion paths. Returns nullopt when the resulting
// budget would be too small to be worth going realtime under.
static std::optional<rlimit> computeRttimeLimit(const rlimit& current, rlim_t maxUs) {
    // an unprivileged process cannot raise a finite hard limit, so stay under a pre-existing
    // one (RLIM_INFINITY compares greater than any finite limit)
    const rlim_t hard = std::min(current.rlim_max, maxUs);

    // keep soft below hard: an overrun then raises a catchable SIGXCPU (which our handler turns
    // into a demotion) before the uncatchable SIGKILL at the hard limit. A tighter pre-existing
    // soft limit is a deliberate choice, respect it.
    rlim_t soft = hard / 4 * 3;
    if (current.rlim_cur != RLIM_INFINITY)
        soft = std::min(soft, current.rlim_cur);

    if (soft < MIN_RTTIME_US)
        return std::nullopt;

    return rlimit{.rlim_cur = soft, .rlim_max = hard};
}

// Validates an environment-imposed RLIMIT_RTTIME before any promotion attempt, returning the
// pre-existing limits in `original` and lowering the soft limit if needed so the SIGXCPU handler
// gets a demotion window before the SIGKILL at the hard limit. The direct path would otherwise
// go realtime under a zero-headroom or unusably small budget.
static bool prepareExistingRttimeLimit(rlimit& original) {
    if (getrlimit(RLIMIT_RTTIME, &original)) {
        Log::logger->log(Log::WARN, "Failed to get RLIMIT_RTTIME: {}", strerror(errno));
        return false;
    }

    // an infinite soft limit disables RT-time signaling entirely, a finite soft limit under an
    // infinite hard limit raises SIGXCPU without a SIGKILL deadline behind it, so the handler
    // can always demote in time
    if (original.rlim_max == RLIM_INFINITY)
        return original.rlim_cur == RLIM_INFINITY || original.rlim_cur >= MIN_RTTIME_US;

    const auto adjusted = computeRttimeLimit(original, original.rlim_max);
    if (!adjusted)
        return false;

    if (adjusted->rlim_cur == original.rlim_cur)
        return true;

    // only ever lowers the soft limit, which needs no privilege
    return setrlimit(RLIMIT_RTTIME, &*adjusted) == 0;
}

// Sets RLIMIT_RTTIME as rtkit requires: it refuses to grant realtime unless the caller has a
// finite RLIMIT_RTTIME at or below its RTTimeUSecMax.
static bool configureRttimeLimit(rlim_t maxUs) {
    rlimit current = {};
    if (getrlimit(RLIMIT_RTTIME, &current)) {
        Log::logger->log(Log::WARN, "rtkit: failed to get RLIMIT_RTTIME: {}", strerror(errno));
        return false;
    }

    const auto newLimit = computeRttimeLimit(current, maxUs);
    if (!newLimit) {
        Log::logger->log(Log::WARN, "rtkit: the RLIMIT_RTTIME budget would be too small, not requesting realtime");
        return false;
    }

    // rtkit validates the hard limit. Lowering it is irreversible without CAP_SYS_RESOURCE and
    // the new limit is inherited by forked children: apps launched from Hyprland that gain
    // realtime through their own privileges will live under this budget too.
    if (setrlimit(RLIMIT_RTTIME, &*newLimit)) {
        Log::logger->log(Log::WARN, "rtkit: failed to set RLIMIT_RTTIME: {}", strerror(errno));
        return false;
    }

    return true;
}

// Asks rtkit (org.freedesktop.RealtimeKit1 on the system bus) to put the calling thread on
// SCHED_RR. rtkit hands out realtime scheduling to unprivileged processes, mediated by polkit
// (granted to active sessions only). It addresses threads by kernel tid (gettid), not pthread_t,
// and grants SCHED_RR with SCHED_RESET_ON_FORK set, so fork children reset to SCHED_OTHER
// kernel-side.
static bool tryRtkit(int prio) {
    GError* error   = nullptr;
    gchar*  address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!address) {
        Log::logger->log(Log::DEBUG, "rtkit: no system bus: {}", error->message);
        g_error_free(error);
        return false;
    }

    // a private connection instead of g_bus_get_sync, so it can be fully torn down after this one-shot call
    GDBusConnection* conn = g_dbus_connection_new_for_address_sync(
        address, sc<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr, &error);
    g_free(address);
    if (!conn) {
        Log::logger->log(Log::DEBUG, "rtkit: failed to connect to the system bus: {}", error->message);
        g_error_free(error);
        return false;
    }

    Hyprutils::Utils::CScopeGuard closeConn([conn] {
        g_dbus_connection_close_sync(conn, nullptr, nullptr);
        g_object_unref(conn);
    });

    // probe rtkit's config before touching RLIMIT_RTTIME: lowering the hard limit must not
    // happen unless rtkit is actually there to grant us realtime in exchange
    const auto maxUs = rtkitGetRttimeMax(conn);
    if (!maxUs || *maxUs <= 0) {
        Log::logger->log(Log::DEBUG, "rtkit: no usable RTTimeUSecMax, not requesting realtime");
        return false;
    }

    if (!configureRttimeLimit(sc<rlim_t>(*maxUs)))
        return false;

    // fails with a polkit denial for sessions that aren't active locally (e.g. ssh),
    // which is fine because realtime is best-effort. the armed RLIMIT_RTTIME must stay,
    // hard limits cannot be raised back.
    GVariant* reply =
        g_dbus_connection_call_sync(conn, RTKIT_SERVICE, RTKIT_OBJECT, RTKIT_INTERFACE, "MakeThreadRealtime", g_variant_new("(tu)", sc<guint64>(gettid()), sc<guint32>(prio)),
                                    nullptr, G_DBUS_CALL_FLAGS_NONE, RTKIT_DBUS_TIMEOUT_MS, nullptr, &error);
    if (!reply) {
        Log::logger->log(Log::DEBUG, "rtkit: MakeThreadRealtime failed: {}", error->message);
        g_error_free(error);
        return false;
    }

    g_variant_unref(reply);
    return true;
}

#endif

// Puts the main thread on SCHED_RR so a busy session cannot starve the compositor of CPU.
// Going realtime directly needs privilege and is tried first, rtkit is the standard
// unprivileged fallback on desktop systems. Best-effort: on failure we just run as SCHED_OTHER.
void NInit::gainRealTime() {
    const int minPrio = sched_get_priority_min(SCHED_RR);
    bool      gained  = false;

#if defined(__linux__)
    // an environment-imposed RLIMIT_RTTIME must leave a usable budget with headroom for the
    // SIGXCPU handler on either promotion path
    rlimit originalLimit = {};
    if (!prepareExistingRttimeLimit(originalLimit)) {
        Log::logger->log(Log::WARN, "Existing RLIMIT_RTTIME leaves no usable realtime budget, not attempting realtime");
        return;
    }

    // preparation may have lowered the soft limit, put it back on every path that does not end
    // up going realtime under it (rtkit configures its own limits, see below)
    bool                          restorePreparedLimitOnExit = true;
    Hyprutils::Utils::CScopeGuard restorePreparedLimit([&] {
        if (restorePreparedLimitOnExit && setrlimit(RLIMIT_RTTIME, &originalLimit))
            Log::logger->log(Log::WARN, "Failed to restore RLIMIT_RTTIME: {}", strerror(errno));
    });

    // hold back SIGXCPU while the handler and the scheduling state are in flux: a signal arriving
    // in between would otherwise hit our handler while not realtime (RLIMIT_CPU also raises
    // SIGXCPU) or hit the default fatal disposition while realtime
    sigset_t blockedSet = {};
    sigset_t oldMask    = {};
    sigemptyset(&blockedSet);
    sigaddset(&blockedSet, SIGXCPU);

    if (pthread_sigmask(SIG_BLOCK, &blockedSet, &oldMask)) {
        Log::logger->log(Log::WARN, "Failed to block SIGXCPU, not attempting realtime");
        return;
    }

    bool viaRtkit = false;
    {
        // unblocks only at scope exit, after the disposition is settled, so a pending SIGXCPU is
        // delivered to the right handler
        Hyprutils::Utils::CScopeGuard restoreMask([&] { pthread_sigmask(SIG_SETMASK, &oldMask, nullptr); });

        if (!installSigxcpuHandler())
            return;

        gained = trySchedDirect(minPrio);

        if (gained)
            restorePreparedLimitOnExit = false;
        else {
            // best-effort: the direct attempt did not end up using the prepared soft limit, put
            // it back (the hard limit is unchanged at this point, so this needs no privilege).
            // rtkit then applies its own deliberate, permanent configuration.
            restorePreparedLimitOnExit = false;
            if (setrlimit(RLIMIT_RTTIME, &originalLimit))
                Log::logger->log(Log::WARN, "Failed to restore RLIMIT_RTTIME: {}", strerror(errno));

            gained = viaRtkit = tryRtkit(minPrio);
        }

        if (!gained)
            sigaction(SIGXCPU, &previousSigxcpuAction, nullptr);
    }

    // logging waits until SIGXCPU is unblocked: while realtime with the signal held back, any
    // non-blocking work eats into the demotion headroom between the soft and hard limits
    if (gained)
        Log::logger->log(Log::DEBUG, viaRtkit ? "Gained realtime scheduling via rtkit" : "Gained realtime scheduling directly");
#else
    gained = trySchedDirect(minPrio);
    if (gained)
        Log::logger->log(Log::DEBUG, "Gained realtime scheduling directly");
#endif

    if (!gained) {
        Log::logger->log(Log::WARN, "Failed to gain realtime scheduling");
        return;
    }

    // NixOS-specific fix to prevent all children from inheriting
    // CAP_SYS_NICE due to how the security wrapper works.
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER, CAP_SYS_NICE, 0, 0);

#if !defined(__linux__)
    // spawned children must not inherit RT, on Linux the kernel handles this via
    // SCHED_RESET_ON_FORK on both promotion paths
    pthread_atfork(nullptr, nullptr, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            Log::logger->log(Log::WARN, "Failed to reset process scheduling strategy");
    });
#endif
}
