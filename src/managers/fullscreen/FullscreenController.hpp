#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include "layout/target/WindowGroupTarget.hpp"
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
  

    Fullscreen Handlers
    -------------------

    There are 2 for layout algorithms on each workspace: floating and tiling. Hence, there are 2 FS handlers on each workspace.


    Non-Covering Fullscreens
    ------------------------

    Layouts may have non-covering fullscreens. Currently this is a binary toggle (Either covering or not covering).



    Fullscreen Modes
    ----------------

    Internal: what Hyprland considers to be fullscreen. Functions that check if a window is FS will use this value to judge.

    Client: what application considers to be fullscreen. Hyprland doesn't consider a window to be "fullscreen" but hints to the application that it is running in fullscreen mode.

    */
    class CFullscreenController {

      public:
        // TODO: make functions constant if they can be
        // TODO: optional for covering. default is to be true since that's what's used almost everywhere. if nullopt, consider "if fullscreen", if false; fullscreen but not covering, if true only covering

        // Window

        /// @param covering If passed, can determine if a window must be covering, must be non-covering. If not passed, window can be either --> handler's isFullscreen() method will be used, which provides no guarantee of coverage
        bool isFullscreen(
            const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt,
            const std::optional<bool> covering = std::nullopt); // ERSTARR TODO - note that passing mode = FSMODE_NONE will not fly. return false then in any case and log an error
        bool            isLayoutManagedFullscreen(const PHLWINDOW window); // TODO: use the handler to judge - delete this todo after implemented
        SFullscreenMode getFullscreenMode(const PHLWINDOW window);

        // Groups

        bool isFullscreen(
            const SP<Layout::CWindowGroupTarget> group, const std::optional<eFullscreenMode> mode = std::nullopt,
            const std::optional<bool> covering = std::nullopt); // ERSTARR TODO - note that passing mode = FSMODE_NONE will not fly. return false then in any case and log an error
        bool            isLayoutManagedFullscreen(const SP<Layout::CWindowGroupTarget> group); // TODO: use the handler to judge - delete this todo after implemented
        SFullscreenMode getFullscreenMode(const SP<Layout::CWindowGroupTarget> group);

        // ERSTARR TODO - if covering is true; need to check if floating algo has FS first. If so, return that; and only if not so do you check tiling

        // Workspace

        bool            hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        PHLWINDOW       getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenMode(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor

        bool            hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        PHLWINDOW       getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenMode(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler

        eFullscreenHandler getFullscreenHandler(const PHLWINDOW window);

        // FS Mode Setters

        // Windows

        // ERSTARR TODO - sync the internal and client -> in client dispatches to internal and internal follows the standard FS path

        void setWindowFullscreenInternal(const PHLWINDOW window, const eFullscreenMode mode, bool force);
        void setWindowFullscreenClient(const PHLWINDOW window, const eFullscreenMode mode, bool force);

        // Groups

        void setWindowFullscreenInternal(const SP<Layout::CWindowGroupTarget> group, const eFullscreenMode mode, bool force);
        void setWindowFullscreenClient(const SP<Layout::CWindowGroupTarget> group, const eFullscreenMode mode, bool force);

        // Misc. Operations

        void moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace);

      private:
        void setWindowFullscreenState(const PHLWINDOW window, SFullscreenMode state, bool force);
    };

    inline UP<CFullscreenController> g_pfullscreenController = makeUnique<CFullscreenController>();

}
