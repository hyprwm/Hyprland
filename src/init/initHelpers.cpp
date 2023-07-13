#include "initHelpers.hpp"

bool Init::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

void Init::gainRealTime() {
    const int                minPrio = sched_get_priority_min(SCHED_RR);
    const struct sched_param param   = {.sched_priority = minPrio};

#ifdef SCHED_RESET_ON_FORK
    if (pthread_setschedparam(pthread_self(), SCHED_RR | SCHED_RESET_ON_FORK, &param))
        Debug::log(WARN, "Failed to change process scheduling strategy");
#else
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        Debug::log(WARN, "Failed to change process scheduling strategy");
        return;
    }

    pthread_atfork(NULL, NULL, []() {
        const struct sched_param param = {.sched_priority = 0};
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param))
            Debug::log(WARN, "Failed to reset process scheduling strategy");
    });
#endif
}