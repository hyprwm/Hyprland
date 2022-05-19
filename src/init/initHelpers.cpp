#include "initHelpers.hpp"

bool Init::isSudo() {
    return getuid() != geteuid() || !geteuid();
}
