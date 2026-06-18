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

        // If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler

        /*
      Scrolling FS Behaviour
      ----------------------

      -> Scrolling layout permits multiple FS windows in a workspace, but there can only be one covering FS window.


        Scrolling FS window must have/be: ERSTARR TODO - COMPLETE THIS AND MAKE THIS CLEANER
           - In the list
             -> FSMODE != NONE (this should still always be checked)
             -> PHLWINDOWREF != null
           - be the only taget in its column

      
      */

      public:
        CScrollingFullscreenHandler(Layout::IModeAlgorithm* algorithm);
        virtual ~CScrollingFullscreenHandler();

        CScrollingFullscreenHandler()                                              = delete;
        CScrollingFullscreenHandler(const CScrollingFullscreenHandler&)            = delete;
        CScrollingFullscreenHandler(CScrollingFullscreenHandler&&)                 = delete;
        CScrollingFullscreenHandler& operator=(const CScrollingFullscreenHandler&) = delete;
        CScrollingFullscreenHandler& operator=(CScrollingFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool            isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        virtual bool            hasFullscreen(const std::optional<bool> covering = true);
        virtual PHLWINDOW       getFullscreen(const std::optional<bool> covering = true);
        /// @warning Does NOT check if a window is fullscreen, merely returns its fullscreenMode
        virtual SFullscreenMode getFullscreenMode(const PHLWINDOW window);

        // FS Request

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode);
        virtual void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode);

        // Window Movement Between FS Handlers

        // the window passed is guaranteed to be a FS window
        // Must rerender by the end
        virtual void moveFullscreenWindowToHandler(const PHLWINDOW window, const std::optional<bool> covering = true);

        // Must properly remove FS properties and state of the window in preparation for move to a new workspace with potentially a new handler.
        // Simply removing the window from various lists should suffice. Re-render and re-decoration will be handled by the receiving handler.
        virtual void moveFullscreenWindowOutOfHandler(const PHLWINDOW window);

        // Optional

        // FS window hiding behaviour
        virtual void setNoMembersAboveFullscreen();

        // FS Window State Syncing (cleaning up FS window list if exists, other self corrections and error mitigation)
        virtual void syncFullscreenWindows();

        /// ERSTARR NOTE - remove a window from the handler. This is not the place to check if a window has all its values correctly set; this just removes it from the list after doing layout specific stuff
        virtual void removeWindowFromHandler(PHLWINDOW window);

        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;

        struct SScrollingFullscreenWindowHidingState {

            PHLWINDOWREF                     lastTiledLayoutManagedFsWindow;
            eFullscreenMode                  lastTiledLayoutManagedFsWindowMode;
            std::unordered_set<PHLWINDOWREF> hiddenFloatingWindowsUnderFSWindow;

            void                             saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW fullscreenWindow);

        } m_fullscreenWindowHidingState;

        // ERSTARR TODO: extract FS related logic from recalculate() and put them in a helper function here. Note that it's a helper for scrolling but it must be public since the algo must directly use it.
        // ---> it'd be truly ideal if this can be integrated into the syncFullscreens, but that might not be the best idea since syncFullscreen already handles its own function

      private:
        Layout::Tiled::CScrollingAlgorithm* m_scrollingAlgorithm = nullptr;

        /// Tracks FSed windows (internal OR client)
        std::unordered_map<PHLWINDOWREF, SFullscreenScrollState> m_fsWindows;

        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_SCROLLING;

        // Helpers for Scrolling FS behaviour

        // SFullscreenScrollState*  fullscreenStateForTarget(SP<Layout::ITarget> target, eFullscreenMode targetFullscreenMode); -- Should be Redundant
        // SFullscreenScrollState*  fullscreenStateForData(SP<Layout::Tiled::SScrollingTargetData> target, eFullscreenMode targetFullscreenMode); -- Should be redundant

        // gets the FS target in a column. Includes check that a col with a FS target must have only one target, which is the FS window
        SP<Layout::Tiled::SScrollingTargetData> fullscreenTargetDataForColumn(SP<Layout::Tiled::SColumnData> col) const;

        /**
        * @note This gets the current tiling FS window even if there is a floating fullscreen window is above it/
        */
        // SP<Layout::ITarget> layoutFullscreenTarget() const; // ----> This is to be replaced by isFullscreen(covering = true)

        /**
        * @note the window of @p target does not necessarily need to cover the monitor/work area for this to return `true`
        * @warning @p mode must not be `FSMODE_NONE`; to check for non-fullscreen, negate the result instead.
        */
        // bool                                isFullscreenTarget(SP<Layout::Tiled::SScrollingTargetData> target, std::optional<eFullscreenMode> mode = std::nullopt) const; -- Redundant -  use isFullscreen()


        // void clearFullscreenWindow(std::unordered_map<PHLWINDOWREF, SFullscreenScrollState>& m_fsWindows, PHLWINDOW window = nullptr);


        float fullscreenColumnWidth() const;
        bool  fullscreenColumnCoversMonitor(SP<Layout::Tiled::SColumnData> col) const;
        bool  fullscreenColumnCoversWorkArea(SP<Layout::Tiled::SColumnData> col) const;
        void  updateFullscreenFade(bool coversMonitor);

        float getTargetColumnWidthBeforeFullscreenOrMaximise(SP<Layout::ITarget> target);
    };
}
