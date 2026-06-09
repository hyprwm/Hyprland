#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "desktop/Workspace.hpp"
#include "desktop/view/Window.hpp"
#include "layout/target/Target.hpp"


// prob need to include as i prob need to call methods
// namespace Layout {
//     class ITarget;
// }


namespace Fullscreen {

    enum eFullscreenMode : int8_t {
        FSMODE_NONE       = 0,
        FSMODE_MAXIMIZED,
        FSMODE_FULLSCREEN,
    };



    struct SFullscreenMode {
        eFullscreenMode internal = FSMODE_NONE;
        eFullscreenMode client   = FSMODE_NONE;
    };




    // one per Window
    struct SWindowFullscreenState {
        SFullscreenMode mode = {.internal = FSMODE_NONE, .client = FSMODE_NONE};
        // instead of layout mananged FS flag, use the FS handler
        Desktop::View::eFullscreenHandler fullscreenHandler = Desktop::View::FULLSCREEN_HANDLER_NONE;
    };



    class CFullscreenController {

        public:

        CFullscreenController()  = default;
        

        ~CFullscreenController(){
            m_fullscreenWindows.clear();
            if (!m_fullscreenWindows.empty())
                Log::logger->log(Log::CRIT, "m_fullscreenWindows.empty() returned false during CFullscreenController() deconstructor");
        }
        

        CFullscreenController(const CFullscreenController&) = delete;
        CFullscreenController(CFullscreenController&)       = delete;
        CFullscreenController(CFullscreenController&&)      = delete;
        CFullscreenController& operator=(const CFullscreenController&) = delete;
        CFullscreenController& operator=(CFullscreenController&&)      = delete;
        



        void setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode);
        void setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode);


        // getFullscreenState
        // getFullscreenState

        SFullscreenMode getFullscreenMode(const PHLWINDOW window);
        SFullscreenMode getCoveringFullscreenMode(const PHLWORKSPACE workspace);

        // setFullscreenHandler(Target/Window)
        // getFullscreenHandler(Target/Window)


        void setWindowFullscreenClient(const PHLWINDOW window, const eFullscreenMode mode, bool force);

        void setWindowFullscreenInternal(const PHLWINDOW window, const eFullscreenMode mode, bool force);

        void setWindowFullscreenState(const PHLWINDOW window, SFullscreenMode state, bool force);



        // for target, take groups into account also! -- otherwise, if it's not a window or group target; return false


        // isCoveringFullscreen(Window/Target) -> handled by FS handlers

        // hasCoveringFullscreen(Workspace) -> check all windows in workspace against the m_fullscreenWindows struct and see if any of the matching FS windows are covering
        // hasCoveringFullscreen(Monitor) -> like above, just dispatch to hasCoveringFullscreen(Workspace) of active workspace of monitor

        // getCoveringFullscreenWindow(workspace/monitor) --> Like above - get the actual window

        // getCoveringFullscreenWindowMode(workspace/monitor) --> Like above - get the actual window

        // moveFullscreenWindowToWorkspace(window, workspace) --> this will be handled by FS handlers.
                                            // Moving from scrolling a covering FS window, a non Covering FS window and vice versa all need to have different behaviours




        private:

        // Tracks FSed windows (internal OR client). For custom layout targets, keep track of them in their own FS handlers
        std::unordered_map<WP<Desktop::View::CWindow>, SWindowFullscreenState> m_fullscreenWindows;    


    };

    inline UP<CFullscreenController> fullscreenController = makeUnique<CFullscreenController>();

}

