#pragma once

#include "../../../../managers/fullscreen/handler/FullscreenHandler.hpp"
#include "../../../../managers/fullscreen/FullscreenController.hpp"





namespace Fullscreen::ScrollingFullscreenHandler {


    class CScrollingFullscreenHandler : public IFullscreenHandler {
        
        // Scrolling layout's custom FS handler



        // all scrolling related helpers, 2 lists of scrolling FS targets (make em unordered sets while you're at it)


        private:

        const eFullscreenHandler FULLSCREEN_HANDLER_NAME = FULLSCREEN_HANDLER_SCROLLING; // better name pls.

        // Tracks FSed windows (internal OR client)
        // Scrolling layout permits multiple FS windows in a workspace.
        std::unordered_map<WP<Desktop::View::CWindow>, SFullscreenMode> m_fullscreenWindows;

        // For custom layout targets, keep track of them in their own FS handlers


    };
}
