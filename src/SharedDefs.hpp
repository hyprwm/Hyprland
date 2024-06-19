#pragma once

#include "helpers/math/Math.hpp"
#include <functional>
#include <any>
#include <hyprutils/math/Box.hpp>

using namespace Hyprutils::Math;

enum eIcons {
    ICON_WARNING = 0,
    ICON_INFO,
    ICON_HINT,
    ICON_ERROR,
    ICON_CONFUSED,
    ICON_OK,
    ICON_NONE
};

enum eRenderStage {
    RENDER_PRE = 0,      /* Before binding the gl context */
    RENDER_BEGIN,        /* Just when the rendering begins, nothing has been rendered yet. Damage, current render data in opengl valid. */
    RENDER_PRE_WINDOWS,  /* Pre windows, post bottom and overlay layers */
    RENDER_POST_WINDOWS, /* Post windows, pre top/overlay layers, etc */
    RENDER_LAST_MOMENT,  /* Last moment to render with the gl context */
    RENDER_POST,         /* After rendering is finished, gl context not available anymore */
    RENDER_POST_MIRROR,  /* After rendering a mirror */
    RENDER_PRE_WINDOW,   /* Before rendering a window (any pass) Note some windows (e.g. tiled) may have 2 passes (main & popup) */
    RENDER_POST_WINDOW,  /* After rendering a window (any pass) */
};

enum eInputType {
    INPUT_TYPE_AXIS = 0,
    INPUT_TYPE_BUTTON,
    INPUT_TYPE_DRAG_START,
    INPUT_TYPE_DRAG_END,
    INPUT_TYPE_MOTION
};

struct SCallbackInfo {
    bool cancelled = false; /* on cancellable events, will cancel the event. */
};

enum eHyprCtlOutputFormat {
    FORMAT_NORMAL = 0,
    FORMAT_JSON
};

struct SHyprCtlCommand {
    std::string                                                   name  = "";
    bool                                                          exact = true;
    std::function<std::string(eHyprCtlOutputFormat, std::string)> fn;
};

typedef std::function<void(void*, SCallbackInfo&, std::any)> HOOK_CALLBACK_FN;
