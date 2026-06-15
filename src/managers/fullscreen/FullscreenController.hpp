#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include "layout/target/WindowGroupTarget.hpp"
#include <functional>
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

    struct SWindowFullscreenState {
      PHLWINDOWREF window;
      SFullscreenMode mode;

      bool operator==(const SWindowFullscreenState& b) const {
        return this->window == b.window;  
      }
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


    */
    class CFullscreenController {

      public:
        // TODO: make functions constant if they can be
        // TODO: optional for covering. default is to be true since that's what's used almost everywhere. if nullopt, consider "if fullscreen", if false; fullscreen but not covering, if true only covering

        // Window

        /// @param covering If passed, can determine if a window must be covering, must be non-covering. If not passed, window can be either --> handler's isFullscreen() method will be used, which provides no guarantee of coverage
        bool                   isFullscreen(const PHLWINDOW window, const std::optional<bool> covering = std::nullopt);
        bool                   isLayoutManagedFullscreen(const PHLWINDOW window); // TODO: use the handler to judge - delete this todo after implemented
        SFullscreenMode        getFullscreenMode(const PHLWINDOW window);


        // Groups

        bool                   isFullscreen(const SP<Layout::CWindowGroupTarget> group, const std::optional<bool> covering = std::nullopt);
        bool                   isLayoutManagedFullscreen(const SP<Layout::CWindowGroupTarget> group); // TODO: use the handler to judge - delete this todo after implemented
        SFullscreenMode        getFullscreenMode(const SP<Layout::CWindowGroupTarget> group);


        // Workspace

        bool                   hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        PHLWINDOW              getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        SFullscreenMode        getFullscreenMode(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor

        bool                   hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        PHLWINDOW              getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        SFullscreenMode        getFullscreenMode(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler

        eFullscreenHandler getFullscreenHandler(const PHLWINDOW window);

        // FS Mode Setters

          // Windows

        void setWindowFullscreenClient(const PHLWINDOW window, const eFullscreenMode mode, bool force);
        void setWindowFullscreenInternal(const PHLWINDOW window, const eFullscreenMode mode, bool force);

          // Groups

        void setWindowFullscreenClient(const SP<Layout::CWindowGroupTarget> group, const eFullscreenMode mode, bool force);
        void setWindowFullscreenInternal(const SP<Layout::CWindowGroupTarget> group, const eFullscreenMode mode, bool force);


        // Misc. Operations

        void moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace);

      private:
        void setWindowFullscreenState(const PHLWINDOW window, SFullscreenMode state, bool force);
    };

    inline UP<CFullscreenController> fullscreenController = makeUnique<CFullscreenController>();

}


namespace std {
template <>
struct hash<Fullscreen::SWindowFullscreenState> {
  size_t operator()(const Fullscreen::SWindowFullscreenState& s) const {
    return std::hash<PHLWINDOWREF>{}(s.window);
  }
};
}