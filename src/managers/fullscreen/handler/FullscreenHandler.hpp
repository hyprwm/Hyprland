#pragma once

#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/memory/Memory.hpp"
#include "layout/algorithm/ModeAlgorithm.hpp"
#include "layout/target/Target.hpp"
#include <unordered_map>
#include <utility>



namespace Fullscreen {

    struct SFullscreenRequest {
        SP<Layout::ITarget> target;
        eFullscreenMode     currentMode = FSMODE_NONE;
        eFullscreenMode     mode        = FSMODE_NONE;
    };

    struct SFsWindowState {
        PHLWINDOWREF window;
        SFullscreenMode mode;
    };


    class IFullscreenHandler {
  
      // TODO : edit below comments into a coherent manual

      // Default FS handler with overridable methods for layouts wishing to implement their own FS handlers

      // std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler

      // If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler


      public:
        IFullscreenHandler(Layout::IModeAlgorithm* algorithm);
        virtual ~IFullscreenHandler() = default;


        IFullscreenHandler()                              = delete;
        IFullscreenHandler(const IFullscreenHandler&)     = delete;
        IFullscreenHandler(IFullscreenHandler&&)           = delete;
        IFullscreenHandler& operator=(const IFullscreenHandler&) = delete;
        IFullscreenHandler& operator=(IFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool      isFullscreen(const PHLWINDOW window, const std::optional<bool> covering = true);
        virtual bool      hasFullscreen(const std::optional<bool> covering = true);
        virtual PHLWINDOW getFullscreen(const std::optional<bool> covering = true);
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


        // Helpers

        virtual void removeFSWindowFromList(PHLWINDOW window) = 0;


        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;


        
      protected:
        // Handler will never outlive its algo because algo owns its handler with UP<>
        const Layout::IModeAlgorithm* m_algorithm;

        SP<Layout::CSpace> getSpace() const;


      private:

        /// Windows with ONLY client FS mode set.
        std::unordered_map<PHLWINDOWREF, SFullscreenMode> m_fsWindows;

        // Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;

        // Helpers for default FS behaviour

    };

}
