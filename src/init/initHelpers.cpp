#if defined(__linux__)
#include <linux/capability.h>
#include <sys/prctl.h>
#endif

#include "initHelpers.hpp"

bool NInit::isSudo() {
    return getuid() != geteuid() || !geteuid();
}

#if defined(__linux__)
// NixOS-specific fix to prevent all children from inheriting
// CAP_SYS_NICE due to how the security wrapper works.
void NInit::lowerAmbientCaps() {
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER, CAP_SYS_NICE, 0, 0);
}
#endif
