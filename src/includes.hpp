#pragma once

// because C/C++ VS Code intellisense is stupid with includes, we will suppress them here.
// This suppresses all "include file not found" errors.
#ifdef __INTELLISENSE__
#pragma diag_suppress 1696
#endif

#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <ctime>
#include <unistd.h>
#include <wayland-server-core.h>

#define GLES32
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>

#ifdef NO_XWAYLAND
#define XWAYLAND false
#else
#define XWAYLAND true
#endif

#include "SharedDefs.hpp"
