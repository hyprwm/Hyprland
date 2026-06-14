#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include <optional>

namespace Fullscreen {

    enum eFullscreenMode : int8_t {
        FSMODE_NONE = 0,
        FSMODE_MAXIMIZED,
        FSMODE_FULLSCREEN,
    };

    enum eFullscreenHandler : int8_t {
        FULLSCREEN_HANDLER_NONE = 0,
        FULLSCREEN_HANDLER_DEFAULT,
        FULLSCREEN_HANDLER_SCROLLING,
    };

    enum eFullscreenRequestResult : uint8_t {
        FULLSCREEN_REQUEST_FAILED = 0,
        FULLSCREEN_REQUEST_DEFAULT,
        FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT,
    };

    struct SFullscreenMode {
        eFullscreenMode internal = FSMODE_NONE;
        eFullscreenMode client   = FSMODE_NONE;
    };


    /*
    FS Controller: To be used to set and get fullscreen state of windows in all parts of the codebase except Layout specific source files. The controller interfaces with the FSHandler of a window to facilitate
    FS state handling.

    FS Handlers: One default, One per Layout that wished to implement custom FS behaviour.
      - If a Layout does not implement their own FS behaviour, thus doesn't have their own FS Handler; the Default FS handler is used.
      - Stores the FS states of the windows they are responsible for.

    A layout may decide to define their own FS behaviour. To do this, they should create a FS Handler class and tie it (UP<>) to their Layout Algorithm class.
      - One LayoutAlgorithm object per workspace; therefore one FS Handler object per workspace.

    Fullscreen/Maximise is a behaviour that is specific to Windows. If a layout wants to define a custom ITarget that also has a FS or FS-like behaviour; and it is outside of Hyprland's CWindow framework, they are to
    handle the FS(-like) behaviour of this Target, as well as the storage of FS(-like) target states, in their own FS Handler classes.
    

    Non-Covering Fullscreens
    ------------------------

    Layouts may have non-covering fullscreens. Currently this is a binary toggle (Either covering or not covering).
    Controller still includes information about the coverage of an FS window because in some parts of the code, this is necessary. (i.e. fully abstracting this away inside the layout FS handler isn't possible at this moment)


    */
    class CFullscreenController {

      public:
        // TODO: make functions constant if they can be

        // Window

        /// @param covering If passed, can determine if a window must be covering, must be non-covering. If not passed, window can be either --> handler's isFullscreen() method will be used, which provides no guarantee of coverage
        bool                   isFullscreen(const PHLWINDOW window, const std::optional<bool> covering = std::nullopt);
        bool                   isLayoutManagedFullscreen(const PHLWINDOW window);
        SFullscreenMode        getFullscreenMode(const PHLWINDOW window);

        // Workspace

        bool                   hasCoveringFullscreen(const PHLWORKSPACE workspace);
        PHLWINDOW              getCoveringFullscreenWindow(const PHLWORKSPACE workspace);
        SFullscreenMode        getCoveringFullscreenMode(const PHLWORKSPACE workspace);

        // Monitor

        bool                   hasCoveringFullscreen(const PHLMONITOR monitor);
        PHLWINDOW              getCoveringFullscreenWindow(const PHLMONITOR monitor);
        SFullscreenMode        getCoveringFullscreenMode(const PHLMONITOR monitor);

        // Handler

        eFullscreenHandler getFullscreenHandler(const PHLWINDOW window);

        // FS Mode Setters

        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode);
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode);

        void setWindowFullscreenClient(const PHLWINDOW window, const eFullscreenMode mode, bool force);
        void setWindowFullscreenInternal(const PHLWINDOW window, const eFullscreenMode mode, bool force);

        // Misc. Operations

        void moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace);

      private:
        void setWindowFullscreenState(const PHLWINDOW window, SFullscreenMode state, bool force);
    };

    inline UP<CFullscreenController> fullscreenController = makeUnique<CFullscreenController>();

}
