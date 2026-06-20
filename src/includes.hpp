#pragma once

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
