#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "desktop/DesktopTypes.hpp"
#include "FullscreenTypes.hpp"
#include <optional>

namespace Fullscreen {

    class IFullscreenHandler;

    class CFullscreenController {

      public:
        CFullscreenController()  = default;
        ~CFullscreenController() = default;

        // Window

        /// @warning will return true for all covering fs windows is there are several
        bool            isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = std::nullopt);

        SFullscreenMode getFullscreenModes(const PHLWINDOW window);

        bool            layoutManagedFS(const PHLWINDOW window);

        // Workspace

        /// @warning only cosiders internal mode of FS windows
        bool hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        /// @warning Returns the topmost covering FS window is there are several.
        PHLWINDOW getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        /// @warning Will return the modes of the TOPMOST Covering FS Window if there are several
        SFullscreenMode getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering = true);

        // Monitor
        // FS windows must be fullscreen (FSMODE_FULLSCREEN) to be considered as FS by monitor

        /// @warning only cosiders internal mode of FS windows
        bool hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        /// @warning Returns the topmost covering FS window is there are several.
        PHLWINDOW getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        /// @warning Will return the modes of the TOPMOST Covering FS Window if there are several
        SFullscreenMode getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering = true);

        // Handler

        eFullscreenHandler getFullscreenHandlerName(const PHLWINDOW window);

        std::string        getFullscreenHandlerNameAsString(const PHLWINDOW window);

        // FS Mode Setter

        void setFullscreenMode(const PHLWINDOW window, std::optional<eFullscreenMode> internal = std::nullopt, std::optional<eFullscreenMode> client = std::nullopt,
                               std::optional<bool> layoutAware = std::nullopt);
        void setInternalFullscreenMode(const PHLWINDOW window, eFullscreenMode internal, bool layoutAware);
        void clearClientFullscreenMode(const PHLWINDOW window);

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
        void setFullscreenModeImpl(const PHLWINDOW window, std::optional<eFullscreenMode> internal, std::optional<eFullscreenMode> client, std::optional<bool> layoutAware,
                                   bool preserveClient);
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware, bool armMaximizeEcho);
        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware);

        // FS Handler getters

        WP<IFullscreenHandler>  getFsHandler(const PHLWINDOW window, std::optional<bool> layoutHandled = std::nullopt);

        SFsHandlersForWorkspace getFsHandlersForWorkspace(const PHLWORKSPACE workspace) const;

        // avoids re-resolving the handlers when the caller already has them
        eFullscreenHandler getFullscreenHandlerName(const PHLWINDOW window, const SFsHandlersForWorkspace& handlers);
    };

    UP<CFullscreenController>& controller();

}
