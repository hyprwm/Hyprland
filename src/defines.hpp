#include "includes.hpp"
#include "debug/Log.hpp"
#include "helpers/MiscFunctions.hpp"
#include "helpers/WLListener.hpp"
#include "helpers/Color.hpp"

#include "wlrunstable/wlr_ext_workspace_v1.hpp"

#include <utility>

#ifndef NDEBUG
#ifdef HYPRLAND_DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif
#else
#define ISDEBUG false
#endif

#define LISTENER(name)                                                                                                                                                             \
    void               listener_##name(wl_listener*, void*);                                                                                                                       \
    inline wl_listener listen_##name = {.notify = listener_##name}
#define DYNLISTENFUNC(name)    void listener_##name(void*, void*)
#define DYNLISTENER(name)      CHyprWLListener hyprListener_##name
#define DYNMULTILISTENER(name) wl_listener listen_##name

#define VECINRECT(vec, x1, y1, x2, y2) ((vec).x >= (x1) && (vec).x <= (x2) && (vec).y >= (y1) && (vec).y <= (y2))

#define DELTALESSTHAN(a, b, delta) (abs((a) - (b)) < (delta))

#define PIXMAN_DAMAGE_FOREACH(region)                                                                                                                                              \
    int        rectsNum = 0;                                                                                                                                                       \
    const auto RECTSARR = pixman_region32_rectangles(region, &rectsNum);                                                                                                           \
    for (int i = 0; i < rectsNum; ++i)

#define PIXMAN_REGION_FOREACH(region) PIXMAN_DAMAGE_FOREACH(region)

#define interface class

#define STICKS(a, b) abs((a) - (b)) < 2

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c)   ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c)  ((double)(((c)) & 0xff) / 255.0)

#define HYPRATOM(name)                                                                                                                                                             \
    { name, 0 }

#ifndef __INTELLISENSE__
#define RASSERT(expr, reason, ...)                                                                                                                                                 \
    if (!(expr)) {                                                                                                                                                                 \
        Debug::log(CRIT, "\n==========================================================================================\nASSERTION FAILED! \n\n%s\n\nat: line %d in %s",            \
                   getFormat(reason, ##__VA_ARGS__).c_str(), __LINE__,                                                                                                             \
                   ([]() constexpr->std::string { return std::string(__FILE__).substr(std::string(__FILE__).find_last_of('/') + 1); })().c_str());                                 \
        printf("Assertion failed! See the log in /tmp/hypr/hyprland.log for more info.");                                                                                          \
        *((int*)nullptr) = 1; /* so that we crash and get a coredump */                                                                                                            \
    }
#else
#define RASSERT(expr, reason, ...)
#endif

#define ASSERT(expr) RASSERT(expr, "?")

#if ISDEBUG
#define UNREACHABLE()                                                                                                                                                              \
    {                                                                                                                                                                              \
        Debug::log(CRIT, "\n\nMEMORY CORRUPTED: Unreachable failed! (Reached an unreachable position, memory corruption!!!)");                                                     \
        *((int*)nullptr) = 1;                                                                                                                                                      \
    }
#else
#define UNREACHABLE() std::unreachable();
#endif

// git stuff
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "?"
#endif
#ifndef GIT_BRANCH
#define GIT_BRANCH "?"
#endif
#ifndef GIT_COMMIT_MESSAGE
#define GIT_COMMIT_MESSAGE "?"
#endif
#ifndef GIT_DIRTY
#define GIT_DIRTY "?"
#endif

#define SPECIAL_WORKSPACE_START (-99)

#define PI 3.14159265358979