#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "FullscreenTypes.hpp"

namespace Fullscreen {
    // Only transaction batches own this hold; the workspace observes it weakly.
    // Shared ownership keeps it alive until transferred/requeued participants finish.
    struct SFullscreenPresentation {
        PHLWORKSPACE    workspace;
        PHLWINDOWREF    window;
        SFullscreenMode modes;
    };
}
