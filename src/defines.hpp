#include "includes.hpp"

#ifndef NDEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif

#define RIP(format, ... ) { fprintf(stderr, format "\n", ##__VA_ARGS__); exit(EXIT_FAILURE); }

#define LISTENER(name) void listener_##name(wl_listener*, void*); inline wl_listener listen_##name = { .notify = listener_##name };