#pragma once

#include "../../../../managers/fullscreen/handler/FullscreenHandler.hpp"
#include "../../../../managers/fullscreen/FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"





namespace Fullscreen::ScrollingFullscreenHandler {


    class CScrollingFullscreenHandler : public IFullscreenHandler {
        
      // TODO : edit below comments into a coherent manual

      // Default FS handler with overridable methods for layouts wishing to implement their own FS handlers

      // std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler

      // If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler


      public:
        CScrollingFullscreenHandler(Layout::IModeAlgorithm* algorithm);
        virtual ~CScrollingFullscreenHandler() = default;

        CScrollingFullscreenHandler()                                              = delete;
        CScrollingFullscreenHandler(const CScrollingFullscreenHandler&)            = delete;
        CScrollingFullscreenHandler(CScrollingFullscreenHandler&&)                 = delete;
        CScrollingFullscreenHandler& operator=(const CScrollingFullscreenHandler&) = delete;
        CScrollingFullscreenHandler& operator=(CScrollingFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool      isFullscreen(const PHLWINDOW window, const std::optional<bool> covering = true);
        virtual bool      hasFullscreen(const std::optional<bool> covering = true);
        virtual PHLWINDOW getFullscreen(const std::optional<bool> covering = true);



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
        virtual void syncFullscreenTargets();



        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;


      private:

        /// Tracks FSed windows (internal OR client)
        /// Scrolling layout permits multiple FS windows in a workspace.
        std::unordered_set<SWindowFullscreenState> m_fullscreenWindows;


        /// Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_SCROLLING;




        // Helpers for Scrolling FS behaviour

        void removeCurrentFullscreenWindow(PHLWINDOW window);

    };
}
