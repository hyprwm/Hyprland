#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include "layout/target/WindowGroupTarget.hpp"
#include <optional>






namespace Fullscreen {

    class IFullscreenHandler;



    enum eFullscreenMode : int8_t {
      FSMODE_NONE       = 0,
      FSMODE_MAXIMIZED  = 1 << 0,
      FSMODE_FULLSCREEN = 1 << 1,
      FSMODE_MAX        = (1 << 2) - 1
    };

    enum eFullscreenHandler : int8_t {
        FULLSCREEN_HANDLER_NONE = 0,
        FULLSCREEN_HANDLER_DEFAULT,
        // All layout FS handlers go below this line
        FULLSCREEN_HANDLER_SCROLLING,
    };

    enum eFullscreenRequestResult : uint8_t {
        FULLSCREEN_REQUEST_FAILED = 0,
        FULLSCREEN_REQUEST_DEFAULT_HANDLED,
        FULLSCREEN_REQUEST_LAYOUT_HANDLED,
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

    Fullscreen/Maximise is a behaviour that is specific to Windows. If a layout wants to define a custom ITarget that also has a FS or FS-like behaviour, that's for the handler to handle.
      it can get the underlying type and dispatch to internal helper, or it can handle in the virtual function, or whatever. If target has FS behaviour that can't be handled with the controller, handler needs to
      handle those parts entirely (and besides in that case that behaviour would have to be isolated to CWeirdAlgorithm files anyway to not break encapsulation)


    A window may either be layout handled or default handled.
    Layout handled matters if window's algo has custom FS behaviour.
    A window may still be explicitly default handled in such a workspace
    
    A window may not be a part of more than one FS handler


    Fullscreen Handlers
    -------------------

    There are 2 for layout algorithms on each workspace: floating and tiling. Hence, there are 2 FS handlers on each workspace.


    Non-Covering Fullscreens
    ------------------------

    Layouts may have non-covering fullscreens. Currently this is a binary toggle (Either covering or not covering).


    Fullscreen Modes
    ----------------

    Internal: what Hyprland considers to be fullscreen. Functions that check if a window is FS will use this value to judge.
    Client:   what application considers to be fullscreen. Hyprland doesn't consider a window to be "fullscreen" but hints to the application that it is running in fullscreen mode.

    */
    class CFullscreenController {

      public:

        CFullscreenController() = default;
        ~CFullscreenController() = default;

        // TODO: make functions constant if they can be
        // TODO: optional for covering. default is to be true since that's what's used almost everywhere. if nullopt, consider "if fullscreen", if false; fullscreen but not covering, if true only covering

        // Window

        /// @param covering If passed can determine if a window must be covering or must be non-covering. If not passed, window can be either
        /// @warning only cosiders internal mode of FS windows
        // ERSTARR TODO - note that passing mode = FSMODE_NONE will not fly. return false then in any case and log an error
        bool            isFullscreen( const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = std::nullopt);
        /// @warning considers both internal and client FS modes of window
        SFullscreenMode getFullscreenMode(const PHLWINDOW window);
        bool            layoutManagedFS(const PHLWINDOW window); // TODO: use the handler to judge - delete this todo after implemented

        // ERSTARR TODO - if covering is true; need to check if floating algo has FS first, THEN the default handler of a layout handler. ONLY after that check the layout handler.

        // Workspace
        /// @warning only cosiders internal mode of FS windows
        bool            hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        /// @warning Returns the topmost covering FS window is there are several.
        /// @warning only cosiders internal mode of FS windows
        PHLWINDOW       getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenMode(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor
        /// @warning only cosiders internal mode of FS windows
        bool            hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        /// @note Returns the topmost covering FS window is there are several.
        /// @warning only cosiders internal mode of FS windows
        PHLWINDOW       getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenMode(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler
        /// @warning considers both internal and client FS modes of window
        eFullscreenHandler getFullscreenHandlerName(const PHLWINDOW window); // CHECK the floating first. After that, check the default handler base class in layout handler. After that check the layout handler.

        // FS Mode Setter


        // ERSTARR TODO - MAKE THIS AN ACTUAL DOYXGEN DOC
        // set window's internal, client (either or both) FS modes. Also allows overriding if you want to FS a window using default FS handler or the layout FS handler it might have access to 
        void setFullscreenMode(const PHLWINDOW window, const std::optional<eFullscreenMode> internal = std::nullopt, const std::optional<eFullscreenMode> client = std::nullopt, std::optional<bool> layoutAware = std::nullopt);

        // Misc. Operations

        // Probably redundant
        // void moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace);

      private:

              // ERSTARR TODO - sync the internal and client -> in client dispatches to internal and internal follows the standard FS path

        // ERSTARR TODO - FORCE SHOULD HOPEFULLY BE IRRELEVANT IF DONE WELL

        // FS Mode Setter Helpers
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware, const bool force);
        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);
        // void setWindowFullscreenState(const PHLWINDOW window, const SFullscreenMode mode, const bool layoutAware, const bool force); // ERSTARR TODO - THIS SHOULD BE MADE REDUNDANT HOPEFULLY --
                                                                                                                                     // OR KEEP IT, AND MAKE INTERNAL SETTER JUST THAT


        // FS Handler getters

        // if layoutHandled not passed; if window is FS, return the FS handler that it is using. If it is not, return layout handler
        SP<IFullscreenHandler> getFSHandler(const PHLWINDOW window, std::optional<bool> layoutHandled = std::nullopt);



        // void setWindowFullscreenState(const PHLWINDOW window, SFullscreenMode state, bool force); // Probably redundant
    };

    
  }
  
inline UP<Fullscreen::CFullscreenController> g_pfullscreenController = makeUnique<Fullscreen::CFullscreenController>();