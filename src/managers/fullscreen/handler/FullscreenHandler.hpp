#pragma once

#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/memory/Memory.hpp"
#include "layout/target/Target.hpp"
#include <hyprutils/memory/UniquePtr.hpp>


namespace Layout {
    class IModeAlgorithm;
}


namespace Fullscreen {

    struct SFullscreenRequest {
        SP<Layout::ITarget> target;
        eFullscreenMode     currentEffectiveMode = FSMODE_NONE;
        eFullscreenMode     effectiveMode        = FSMODE_NONE;
    };

    class IFullscreenHandler {
  
      // Default FS handler with overridable methods for layouts wishing to implement their own FS handlers

      public:
        IFullscreenHandler(Layout::IModeAlgorithm* algorithm) : m_algorithm(algorithm){};
        virtual ~IFullscreenHandler() = default;


        IFullscreenHandler()                              = delete;
        IFullscreenHandler(const IFullscreenHandler&)     = delete;
        IFullscreenHandler(IFullscreenHandler&&)           = delete;
        IFullscreenHandler& operator=(const IFullscreenHandler&) = delete;
        IFullscreenHandler& operator=(IFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool      isFullscreen(const PHLWINDOW window, const std::optional<bool> covering = std::nullopt);
        virtual bool      hasCoveringFullscreen();
        virtual PHLWINDOW getCoveringFullscreen();



        // FS Request

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);


        // Window Movement Between FS Handlers

        // the window passed is guaranteed to be a FS window - NECESSARILY NOT COVERING
        // Must rerender by the end
        virtual void moveFullscreenWindowToHandler(const PHLWINDOW window);

        // the window passed is guaranteed to be a FS window - NECESSARILY COVERING
        // Must rerender by the end
        virtual void moveCoveringFullscreenWindowToHandler(const PHLWINDOW window);

        // Must properly remove FS properties and state of the window in preparation for move to a new workspace with potentially a new handler.
        // Simply removing the window from various lists should suffice. Re-render and re-decoration will be handled by the receiving handler.
        virtual void moveFullscreenWindowOutOfHandler(const PHLWINDOW window);


        // Optional

        // FS window hiding behaviour
        virtual void setNoMembersAboveFullscreen();
        
        // FS Window State Syncing (cleaning up FS window list if exists, other self corrections and error mitigation)
        virtual void syncFullscreenTargets();



        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const; // simply return FULLSCREEN_HANDLER_TYPE



      private:

        // Must be defined for each handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;

        // Tracks FSed window (internal OR client)
        // There can only be one default handled fullscreen in a workspace.
        PHLWINDOWREF m_fullscreenWindow = nullptr;

        // Handler will never outlive its algo because algo owns its handler with UP<>
        const Layout::IModeAlgorithm* m_algorithm;

        // If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler
    };

}
