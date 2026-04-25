#include "Env.hpp"

#include <cstdlib>
#include <string_view>

bool Env::envEnabled(const std::string& env) {
    auto ret = getenv(env.c_str());
    if (!ret)
        return false;

    const std::string_view sv = ret;

    return !sv.empty() && sv != "0";
}

bool Env::isTrace() {
    static bool TRACE = envEnabled("HYPRLAND_TRACE");
    return TRACE;
}
