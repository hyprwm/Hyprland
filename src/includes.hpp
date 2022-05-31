#pragma once

// because C/C++ VS Code intellisense is stupid with includes, we will suppress them here.
// This suppresses all "include file not found" errors.
#ifdef __INTELLISENSE__
#pragma diag_suppress 1696
#endif

#include <X11/Xlib.h>
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

#define class _class
#define namespace _namespace
#define static

extern "C" {
#include "../wlroots/include/wlr/backend.h"
#include "../wlroots/include/wlr/backend/libinput.h"
#include "../wlroots/include/wlr/render/allocator.h"
#include "../wlroots/include/wlr/render/wlr_renderer.h"
#include "../wlroots/include/wlr/types/wlr_compositor.h"
#include "../wlroots/include/wlr/types/wlr_cursor.h"
#include "../wlroots/include/wlr/types/wlr_data_control_v1.h"
#include "../wlroots/include/wlr/types/wlr_data_device.h"
#include "../wlroots/include/wlr/types/wlr_export_dmabuf_v1.h"
#include "../wlroots/include/wlr/types/wlr_linux_dmabuf_v1.h"
#include "../wlroots/include/wlr/types/wlr_gamma_control_v1.h"
#include "../wlroots/include/wlr/types/wlr_idle.h"
#include "../wlroots/include/wlr/types/wlr_input_device.h"
#include "../wlroots/include/wlr/types/wlr_keyboard.h"
#include "../wlroots/include/wlr/types/wlr_layer_shell_v1.h"
#include "../wlroots/include/wlr/types/wlr_matrix.h"
#include "../wlroots/include/wlr/types/wlr_output.h"
#include "../wlroots/include/wlr/types/wlr_output_layout.h"
#include "../wlroots/include/wlr/types/wlr_output_management_v1.h"
#include "../wlroots/include/wlr/types/wlr_pointer.h"
#include "../wlroots/include/wlr/types/wlr_presentation_time.h"
#include "../wlroots/include/wlr/types/wlr_primary_selection.h"
#include "../wlroots/include/wlr/types/wlr_primary_selection_v1.h"
#include "../wlroots/include/wlr/types/wlr_screencopy_v1.h"
#include "../wlroots/include/wlr/types/wlr_seat.h"
#include "../wlroots/include/wlr/types/wlr_server_decoration.h"
#include "../wlroots/include/wlr/types/wlr_viewporter.h"
#include "../wlroots/include/wlr/types/wlr_virtual_keyboard_v1.h"
#include "../wlroots/include/wlr/types/wlr_xcursor_manager.h"
#include "../wlroots/include/wlr/types/wlr_xdg_activation_v1.h"
#include "../wlroots/include/wlr/types/wlr_xdg_decoration_v1.h"
#include "../wlroots/include/wlr/types/wlr_xdg_output_v1.h"
#include "../wlroots/include/wlr/types/wlr_xdg_shell.h"
#include "../wlroots/include/wlr/types/wlr_subcompositor.h"
#include "../wlroots/include/wlr/types/wlr_scene.h"
#include "../wlroots/include/wlr/types/wlr_output_damage.h"
#include "../wlroots/include/wlr/types/wlr_input_inhibitor.h"
#include "../wlroots/include/wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h"
#include "../wlroots/include/wlr/types/wlr_virtual_pointer_v1.h"
#include "../wlroots/include/wlr/types/wlr_foreign_toplevel_management_v1.h"
#include "../wlroots/include/wlr/util/log.h"
#include "../wlroots/include/wlr/xwayland.h"
#include "../wlroots/include/wlr/util/region.h"
#include <xkbcommon/xkbcommon.h>
#include <X11/Xproto.h>
#include "../wlroots/include/wlr/render/egl.h"
#include "../wlroots/include/wlr/render/gles2.h"
#include "../wlroots/include/wlr/render/wlr_texture.h"
#include "../wlroots/include/wlr/types/wlr_pointer_constraints_v1.h"
#include "../wlroots/include/wlr/types/wlr_relative_pointer_v1.h"
}

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

#ifdef NO_XWAYLAND
#define XWAYLAND false
#else
#define XWAYLAND true
#endif

#include "helpers/Vector2D.hpp"

#include "../ext-workspace-unstable-v1-protocol.h"