#pragma once

#include "desktop/DesktopTypes.hpp"
#include "layout/target/Target.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include <optional>
#include <unordered_map>

namespace Layout::Tiled {
    struct SScrollingTargetData;
    struct SColumnData;
    class CScrollingAlgorithm;
}

namespace Fullscreen::ScrollingFullscreenHandler {

    struct SFullscreenScrollState {
        SFullscreenMode      mode;
        std::optional<float> restoreColumnWidth = std::nullopt;
    };

    class CScrollingFullscreenHandler : public IFullscreenHandler {

      public:
        CScrollingFullscreenHandler(Layout::Tiled::CScrollingAlgorithm* const algorithm);
        virtual ~CScrollingFullscreenHandler();

        CScrollingFullscreenHandler()                                              = delete;
        CScrollingFullscreenHandler(const CScrollingFullscreenHandler&)            = delete;
        CScrollingFullscreenHandler(CScrollingFullscreenHandler&&)                 = delete;
        CScrollingFullscreenHandler& operator=(const CScrollingFullscreenHandler&) = delete;
        CScrollingFullscreenHandler& operator=(CScrollingFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool                isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        virtual bool                hasFullscreen(const std::optional<bool> covering = true);
        virtual SP<Layout::ITarget> getFullscreen(const std::optional<bool> covering = true);
        virtual SFullscreenMode     getFullscreenModes(SP<Layout::ITarget> target);

        // FS Request

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);

        // Helpers

        virtual void setTargetSizeAndPosition(const SP<Layout::ITarget> target);
        virtual void setNoMembersAboveFullscreen();
        virtual void syncFullscreenTargets();
        virtual void removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard = false);

        // Misc

        virtual eFullscreenHandler getFullscreenHandlerName() const;

        // Scrolling Specific Helpers

        void sScrollingDataRecalculateHelper(const SP<Layout::Tiled::SScrollingTargetData> CURRENT_FS_TDATA, const PHLMONITOR MONITOR, const bool TARGET_WORKSPACE_HAS_FS);

      private:
        struct SScrollingFullscreenWindowHidingState {

            PHLWINDOWREF                     lastTiledLayoutManagedFsWindow     = nullptr;
            eFullscreenMode                  lastTiledLayoutManagedFsWindowMode = FSMODE_NONE;
            std::unordered_set<PHLWINDOWREF> hiddenFloatingWindowsUnderFSWindow;

        } m_fullscreenWindowHidingState;

        Layout::Tiled::CScrollingAlgorithm* const m_scrollingAlgorithm;

        /// Tracks FSed Targets (internal OR client)
        std::unordered_map<WP<Layout::ITarget>, SFullscreenScrollState> m_fsTargets;

        const eFullscreenHandler                                        FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_SCROLLING;

        // Internal helpers for Scrolling FS behaviour

        // fullscreenWindow is must be tiled, layout handled, and cover the whole monitor or work area.
        void  saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW fullscreenWindow);

        float fullscreenColumnWidth() const;
        bool  columnCoversMonitor(SP<Layout::Tiled::SColumnData> col) const;
        bool  columnCoversWorkArea(SP<Layout::Tiled::SColumnData> col) const;
        void  updateFullscreenFade(bool coversMonitor);

        float getTargetColumnWidthBeforeFullscreenOrMaximise(const SP<Layout::ITarget> target);
    };
}
