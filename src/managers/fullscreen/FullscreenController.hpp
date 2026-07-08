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
        FULLSCREEN_HANDLER_NONE = 0, // default/error case
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

    class CFullscreenController {

      public:
        CFullscreenController()  = default;
        ~CFullscreenController() = default;

        // Window

        bool            isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = std::nullopt);

        SFullscreenMode getFullscreenModes(const PHLWINDOW window);

        bool            layoutManagedFS(const PHLWINDOW window);

        // Workspace

        bool hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        /// @note Returns the topmost covering FS window is there are several.
        PHLWINDOW       getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        SFullscreenMode getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor
        // FS windows must be fullscreen (FSMODE_FULLSCREEN) to be considered as FS by monitor

        /// @warning only cosiders internal mode of FS windows
        bool hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        /// @note Returns the topmost covering FS window is there are several.
        PHLWINDOW       getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        SFullscreenMode getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler

        eFullscreenHandler getFullscreenHandlerName(const PHLWINDOW window);

        std::string        getFullscreenHandlerNameAsString(const PHLWINDOW window);

        // FS Mode Setter

        void setFullscreenMode(const PHLWINDOW window, std::optional<eFullscreenMode> internal = std::nullopt, std::optional<eFullscreenMode> client = std::nullopt,
                               std::optional<bool> layoutAware = std::nullopt);

        // Misc. Operations

        // In order to avoid re-setting an FS window's size over and over again if it's FS and already set to the correct value.
        bool m_windowPosSettingQueued = false;

      private:
        struct SFsHandlersForWorkspace {
            const WP<IFullscreenHandler> TILED_FS_HANDLER;
            const WP<IFullscreenHandler> TILED_DEFAULT_FS_HANDLER;
            const WP<IFullscreenHandler> FLOATING_FS_HANDLER;
        };

        // FS Mode Setter Helpers
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);
        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);

        // FS Handler getters

        WP<IFullscreenHandler>  getFsHandler(const PHLWINDOW window, std::optional<bool> layoutHandled = std::nullopt);

        SFsHandlersForWorkspace getFsHandlersForWorkspace(const PHLWORKSPACE workspace) const;

        // List of FSMODE_MAX windows
        std::unordered_set<WP<Desktop::View::CWindow>> m_fsModeMaxWindows;
    };

    UP<CFullscreenController>& controller();

}
