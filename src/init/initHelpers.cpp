#include "initHelpers.hpp"
#include <sys/resource.h>

bool Init::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

void Init::gainRealTime() {
    // If something else is adjusting priority, don't interfere
    if (getpriority(PRIO_PROCESS, 0) != 0) {
        return;
    }

    const int                minPrio = sched_get_priority_min(SCHED_RR);
    const struct sched_param param   = {.sched_priority = minPrio};

    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        Debug::log(WARN, "Failed to change process scheduling strategy");
        return;
    }

    pthread_atfork(NULL, NULL, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            Debug::log(WARN, "Failed to reset process scheduling strategy");
    });
}