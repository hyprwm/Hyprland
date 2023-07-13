#include "initHelpers.hpp"

bool Init::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

void Init::gainRealTime() {
#ifdef SCHED_RESET_ON_FORK
    const int   minPrio = sched_get_priority_min(SCHED_RR);
    sched_param param   = {.sched_priority = minPrio};

    if (pthread_setschedparam(pthread_self(), SCHED_RR | SCHED_RESET_ON_FORK, &param))
        Debug::log(WARN, "Failed to change process scheduling strategy");
#endif
}