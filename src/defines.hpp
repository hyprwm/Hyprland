#include "includes.hpp"

#ifndef NDEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif

#define RIP(format, ... ) { fprintf(stderr, format "\n", ##__VA_ARGS__); exit(EXIT_FAILURE); }

#define LISTENER(name) inline wl_listener listener_##name = {.notify = ##name};