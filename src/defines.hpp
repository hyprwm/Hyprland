#include "includes.hpp"
#include "debug/Log.hpp"
#include "helpers/MiscFunctions.hpp"
#include "helpers/WLListener.hpp"

#ifndef NDEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif

#define RIP(format, ... ) { fprintf(stderr, format "\n", ##__VA_ARGS__); exit(EXIT_FAILURE); }

#define LISTENER(name) void listener_##name(wl_listener*, void*); inline wl_listener listen_##name = { .notify = listener_##name };
#define DYNLISTENFUNC(name) void listener_##name(void*, void*);
#define DYNLISTENER(name) CHyprWLListener hyprListener_##name;
#define DYNMULTILISTENER(name) wl_listener listen_##name;

#define VECINRECT(vec, x1, y1, x2, y2) (vec.x >= (x1) && vec.x <= (x2) && vec.y >= (y1) && vec.y <= (y2))

#define interface class

#define STICKS(a, b) abs((a) - (b)) < 2

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c) ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c) ((double)(((c)) & 0xff) / 255.0)

#define HYPRATOM(name) {name, 0}

#ifndef __INTELLISENSE__
#define RASSERT(expr, reason)                                                                                                                                                                                                                                                                                                                  \
    if (!expr) {                                                                                                                                                                                                                                                                                                                               \
        Debug::log(CRIT, "\n==========================================================================================\nASSERTION FAILED! \n\n%s\n\nat: line %d in %s", std::string(reason).c_str(), __LINE__, ([]() constexpr->std::string { return std::string(__FILE__).substr(std::string(__FILE__).find_last_of('/') + 1); })().c_str()); \
        RIP("Assertion failed! See the log in /tmp/hypr/hyprland.log for more info.");                                                                                                                                                                                                                                                         \
    }
#else
#define RASSERT(expr, reason)
#endif

#define ASSERT(expr) RASSERT(expr, "?")