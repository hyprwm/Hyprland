#include "signal-safe.hpp"

#ifndef __GLIBC__
#include <signal.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

extern char** environ;

char const*   sig_getenv(char const* name) {
    size_t len = strlen(name);
    for (char** var = environ; *var != NULL; var++) {
        if (strncmp(*var, name, len) == 0 && (*var)[len] == '=') {
            return (*var) + len + 1;
        }
    }
    return NULL;
}

char const* sig_strsignal(int sig) {
#ifdef __GLIBC__
    return sigabbrev_np(sig);
#elif defined(__DragonFly__) || defined(__FreeBSD__)
    return sys_signame[sig];
#else
    return "unknown";
#endif
}
