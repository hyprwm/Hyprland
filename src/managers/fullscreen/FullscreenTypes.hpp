#pragma once

#include <cstdint>

namespace Fullscreen {
    enum eFullscreenMode : int8_t {
        FSMODE_NONE = 0,
        FSMODE_MAXIMIZED,
        FSMODE_FULLSCREEN,
    };

    enum eFullscreenHandler : uint8_t {
        FULLSCREEN_HANDLER_NONE      = 0,
        FULLSCREEN_HANDLER_DEFAULT   = 1 << 0,
        FULLSCREEN_HANDLER_LAYOUT    = 1 << 1,
        FULLSCREEN_HANDLER_SCROLLING = 1 << 2 | FULLSCREEN_HANDLER_LAYOUT,
    };

    enum eFullscreenRequestResult : uint8_t {
        FULLSCREEN_REQUEST_FAILED = 0,
        FULLSCREEN_REQUEST_DEFAULT_HANDLED,
        FULLSCREEN_REQUEST_LAYOUT_HANDLED,
    };

    enum eFullscreenMutationContext : uint8_t {
        FULLSCREEN_MUTATION_NORMAL = 0,
        // Move compositor-owned internal state without changing client-owned state.
        FULLSCREEN_MUTATION_TRANSFER,
    };

    struct SFullscreenMode {
        eFullscreenMode internal = FSMODE_NONE;
        eFullscreenMode client   = FSMODE_NONE;
    };
}
