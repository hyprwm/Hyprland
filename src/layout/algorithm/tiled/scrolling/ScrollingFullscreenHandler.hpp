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

        // TODO : edit below comments into a coherent manual

        // Default FS handler with overridable methods for layouts wishing to implement their own FS handlers

        // std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler

        /*
      Scrolling FS Behaviour
      ----------------------

      -> Scrolling layout permits multiple FS targets in a workspace, but there can only be one covering FS target.


        Scrolling FS target must have/be: ERSTARR TODO - COMPLETE THIS AND MAKE THIS CLEANER
           - In the list
             -> FSMODE != NONE (this should still always be checked)
             -> WP<ITarget> != null
           - be the only taget in its column

      
      */

      public:
        CScrollingFullscreenHandler(Layout::Tiled::CScrollingAlgorithm* const algorithm);
        virtual ~CScrollingFullscreenHandler();

        CScrollingFullscreenHandler()                                              = delete;
        CScrollingFullscreenHandler(const CScrollingFullscreenHandler&)            = delete;
        CScrollingFullscreenHandler(CScrollingFullscreenHandler&&)                 = delete;
        CScrollingFullscreenHandler& operator=(const CScrollingFullscreenHandler&) = delete;
        CScrollingFullscreenHandler& operator=(CScrollingFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool            isFullscreen(const SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        virtual bool            hasFullscreen(const std::optional<bool> covering = true);
        virtual SP<Layout::ITarget>       getFullscreen(const std::optional<bool> covering = true);
        /// @warning Does NOT check if a target is fullscreen, merely returns its fullscreenMode
        virtual SFullscreenMode getFullscreenModes(const SP<Layout::ITarget> target);

        // FS Request

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);

        // Optional

        // ERSTARR TODO - THIS IS FOR WINDOWS ONLY - HANDLE
        // FS Window hiding behaviour
        virtual void setNoMembersAboveFullscreen();

        // FS Target State Syncing (cleaning up FS Target list if exists, other self corrections and error mitigation)
        virtual void syncFullscreenTargets();

        /// ERSTARR NOTE - remove a Target from the handler. This is not the place to check if a Target has all its values correctly set; this just removes it from the list after doing layout specific stuff
        virtual void removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard = false);

        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;

        // ERSTARR TODO: extract FS related logic from recalculate() and put them in a helper function here. Note that it's a helper for scrolling but it must be public since the algo must directly use it.
        // ---> it'd be truly ideal if this can be integrated into the syncFullscreens, but that might not be the best idea since syncFullscreen already handles its own function


        void sScrollingDataRecalculateHelper(const SP<Layout::Tiled::SScrollingTargetData> CURRENT_FS_TDATA, const PHLMONITOR MONITOR, const bool TARGET_WORKSPACE_HAS_FS);


          private:

            // ERSTARR TODO - This is specific to windows. need to put a guard in place for ITargets that are not windows
            struct SScrollingFullscreenWindowHidingState {

                PHLWINDOWREF                     lastTiledLayoutManagedFsWindow;
                eFullscreenMode                  lastTiledLayoutManagedFsWindowMode;
                std::unordered_set<PHLWINDOWREF> hiddenFloatingWindowsUnderFSWindow;

            } m_fullscreenWindowHidingState;

            Layout::Tiled::CScrollingAlgorithm* const m_scrollingAlgorithm;

            /// Tracks FSed Targets (internal OR client)
            std::unordered_map<WP<Layout::ITarget>, SFullscreenScrollState> m_fsTargets;

            const eFullscreenHandler                                        FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_SCROLLING;

            // Helpers for Scrolling FS behaviour

            void  saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW window);

            float fullscreenColumnWidth() const;
            bool  columnCoversMonitor(SP<Layout::Tiled::SColumnData> col) const;
            bool  columnCoversWorkArea(SP<Layout::Tiled::SColumnData> col) const;
            void  updateFullscreenFade(bool coversMonitor);

            float getTargetColumnWidthBeforeFullscreenOrMaximise(const SP<Layout::ITarget> target);
    };
}
