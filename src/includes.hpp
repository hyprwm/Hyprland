#pragma once

// because C/C++ VS Code intellisense is stupid with includes, we will suppress them here.
// This suppresses all "include file not found" errors.
#ifdef __INTELLISENSE__
#pragma diag_suppress 1696
#endif

#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <mutex>
#include <thread>
#include <filesystem>
#include <climits>

#if true
// wlroots uses dumb-ass shit that makes it not compile on C++, let's fix that.
// https://github.com/swaywm/wlroots/issues/682
// pthread first because it uses class in a C++ way and XWayland includes that...
#include <pthread.h>

#define class     _class
#define namespace _namespace
#define static
#define delete delete_

extern "C" {
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/drm.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/egl.h>

#include <libdrm/drm_fourcc.h>

#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
}

#undef delete
#undef class
#undef namespace
#undef static
#endif

#ifdef LEGACY_RENDERER
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define GLES2
#else
#define GLES32
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#endif

#if !WLR_HAS_X11_BACKEND
#include "helpers/X11Stubs.hpp"
#endif

#ifdef NO_XWAYLAND
#define XWAYLAND false
#else
#define XWAYLAND true
#endif

#include "helpers/Vector2D.hpp"
#include "helpers/Box.hpp"
#include "SharedDefs.hpp"
