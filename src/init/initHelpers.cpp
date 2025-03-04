#include "initHelpers.hpp"

bool NInit::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

void NInit::gainRealTime() {
    const int          minPrio = sched_get_priority_min(SCHED_RR);
    int                old_policy;
    struct sched_param param;

    if (pthread_getschedparam(pthread_self(), &old_policy, &param)) {
        NDebug::log(WARN, "Failed to get old pthread scheduling priority");
        return;
    }

    param.sched_priority = minPrio;

    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        NDebug::log(WARN, "Failed to change process scheduling strategy");
        return;
    }

    pthread_atfork(nullptr, nullptr, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            NDebug::log(WARN, "Failed to reset process scheduling strategy");
    });
}