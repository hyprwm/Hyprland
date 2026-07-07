#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include <optional>
#include <unordered_set>

namespace Fullscreen {

    class IFullscreenHandler;

    enum eFullscreenMode : int8_t {
        FSMODE_NONE = 0,
        FSMODE_MAXIMIZED,
        FSMODE_FULLSCREEN,
    };

    enum eFullscreenHandler : uint8_t {
        FULLSCREEN_HANDLER_NONE = 0,
        // Types of Handlers
        FULLSCREEN_HANDLER_DEFAULT = 1 << 0,
        FULLSCREEN_HANDLER_LAYOUT  = 1 << 1,
        // Specific Handlers
        FULLSCREEN_HANDLER_SCROLLING = 1 << 2 | FULLSCREEN_HANDLER_LAYOUT,
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


      Controller/Handler Error Resistance, Recovery and Correction
      ------------------------------------------------

      Handlers implement error correction when FS states are set.
      TODO: They should also implement some error correction in FS state getters (noted in Default FS Controller Manual)

      Controller implements error recovery (and correction where it's performant) when querying FS states
      TODO: It should relegate this to the handlers



      FS window positon setting
      -------------------------

      It is done by the handlers. To avoid overriding the positons of FS windows, set m_windowPosSettingQueued before setting window positions.
      
      Make sure that when setting FS window's pos, the window is considered FS by the controller's FS state getter calls to ensure that 
      window/workspace rules correctly apply, window decorations and other properties that may affect an FS window are refreshed,
      and m_windowPosSettingQueued flag is correctly lowered when updatePos() is called


      Window Groups
      -------------

      Window groups do not have a "fullscreen" attribute: the current window within them does.
      Therefore window groups aren't tracked as a whole, but only the currently active window is tracked.

      Controller does NOT accept window groups as parameters - pass the current window instead.
      Handlers accept windowGroup targets for FS state getters (NOT for FS state setters), but they implicitly check the currently active window in the window group - not the group as a whole.

      Important: window group targets have their own targetBox! this needs to be set separately from the window's

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

      Fullscreen Mode Values
      ----------------------

      FSMODE_NONE:       Not a fullscreen
      FSMODE_MAXIMISED:  Maximised window
      FSMODE_FULLSCREEN: Fullscreen Window


      

      FSMODE_MAX
      ----------
      
      FSMODE_MAX : When a client request FSMODE_FULLSCREEN and the current window's internal mode is FSMODE_MAXIMISED, it becomes FSMODE_MAX.


      This is not an official FS mode, but handling of a special case:
        e.g. maximised browser window -> fullscreen and unfullscreen window -> browser window should still be maximised


      Effectively, it is treated as a Fullscreen window in all aspects but one: If this window is un-FSed, it will return to being maximised instead of un-FSed
      
      Handlers don't track this as an official mode

    */
    class CFullscreenController {

      public:
        CFullscreenController()  = default;
        ~CFullscreenController() = default;

        // Window

        /// @param covering If passed can determine if a window must be covering or must be non-covering. If not passed, window can be either
        /// @note only cosiders internal mode of FS windows
        bool isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = std::nullopt);
        /// @note considers both internal and client FS modes of window
        SFullscreenMode getFullscreenModes(const PHLWINDOW window);
        bool            layoutManagedFS(const PHLWINDOW window);

        // Workspace
        /// @warning only cosiders internal mode of FS windows
        bool hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        /// @warning Returns the topmost covering FS window is there are several.
        /// @note only cosiders internal mode of FS windows
        PHLWINDOW       getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor
        // FS windows must be fullscreen (FSMODE_FULLSCREEN) to be considered as FS by monitor

        /// @warning only cosiders internal mode of FS windows
        bool hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        /// @note Returns the topmost covering FS window is there are several.
        /// @warning only cosiders internal mode of FS windows
        PHLWINDOW       getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);
        SFullscreenMode getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler
        /// @warning considers both internal and client FS modes of window
        eFullscreenHandler getFullscreenHandlerName(
            const PHLWINDOW window); // CHECK the floating first. After that, check the default handler base class in layout handler. After that check the layout handler.

        std::string getFullscreenHandlerNameAsString(const PHLWINDOW window);

        // FS Mode Setter

        // set window's internal, client (either or both) FS modes. Also allows overriding if you want to FS a window using default FS handler or the layout FS handler it might have access to
        void setFullscreenMode(const PHLWINDOW window, std::optional<eFullscreenMode> internal = std::nullopt, std::optional<eFullscreenMode> client = std::nullopt,
                               std::optional<bool> layoutAware = std::nullopt);

        // Misc. Operations

        // In order to avoid re-setting an FS window's size over and over again if it's FS and already set to the correct value.
        // Different layout are allowed to implement custom sizes for their FS windows therefore we simply prevent re-setting the values
        bool m_windowPosSettingQueued = false;

      private:
        // FS Mode Setter Helpers
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);
        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);

        // FS Handler getters

        // if layoutHandled not passed; if window is FS, return the FS handler that it is using. If it is not, return layout handler
        WP<IFullscreenHandler> getFSHandler(const PHLWINDOW window, std::optional<bool> layoutHandled = std::nullopt);

        /* FSMODE_MAX */

        // List of FSMODE_MAX windows
        std::unordered_set<WP<Desktop::View::CWindow>> m_fsModeMaxWindows;
    };

    UP<CFullscreenController>& controller();

}
