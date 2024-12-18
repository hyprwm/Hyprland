#include "signal-safe.hpp"

#ifndef __GLIBC__
#include <signal.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

// NOLINTNEXTLINE
extern "C" char** environ;
; // clang-format kludge

char const* sigGetenv(char const* name) {
    const size_t len = strlen(name);
    for (char** var = environ; *var != nullptr; var++) {
        if (strncmp(*var, name, len) == 0 && (*var)[len] == '=') {
            return (*var) + len + 1;
        }
    }
    return nullptr;
}

char const* sigStrsignal(int sig) {
#ifdef __GLIBC__
    return sigabbrev_np(sig);
#elif defined(__DragonFly__) || defined(__FreeBSD__)
    return sys_signame[sig];
#else
    return "unknown";
#endif
}
