#include "Env.hpp"

#include <cstdlib>

bool Env::envEnabled(const char* env) {
    const auto ret = getenv(env);
    return ret && ret[0] != '\0' && !(ret[0] == '0' && ret[1] == '\0');
}

bool Env::isTrace() {
    static bool TRACE = envEnabled("HYPRLAND_TRACE");
    return TRACE;
}
