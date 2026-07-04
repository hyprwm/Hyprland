
#include "managers/fullscreen/FullscreenController.hpp"

/* Shared FS bevaiour tests and helpers */


// TODO this should cover all default handled algos. simply set the layout to that algo and call these.
// For scrolling, some apply some don't. Need to test extra features of scrolling too







// make a helper to use FS dispatchers with given mode programatically - most of the below should be tested with both maximised and fullscreen


static void testFsState(Fullscreen::eFullscreenMode mode) {

    // Fullscreen



    // Maximise


}



// todo better name pls
static void testFsUnderlyingWindowNonInterference() {

    // FS and unFS a window. Under deafult handled FS, all the window locations, sizes, etc... should remain exactly as they were before being FSed

}


static void testMovingFsBetweenWorkspaces() {

    // move a FS window between 2 workspaces with the same FS Handler (default here)



}



static void testFloatUnfloatFsWindow() {


    // float tiled FS window


    // unFloat(tile) floating FS window


    // float tiled FS window, then retile it while it is Fs.


    // tile floating Fs window, then float it again.
      // unFS it and check that the size is the same as it was before being FSed at all


}


static void testWindowHidingUnderFs() {

    // check m_allowedOverFullscreen values of windows under FS.


    // all other windows other than the FSed window should be under it upon FSing.
    // All opened tiled windows that we don't switch to (or ignore the opening of and don't unFS the current window) should remain under it
    // All floating windows opened after the window was FSed should remain above it
    // unFs that window - no FS windows should remain

}


static void testFsWindowSwitchingFsAnotherWindow() {
    // Fs tiled window, open floating window, FS that. Fs should have cycled. unFs that window, no Fs windows should remain
}





static void testFsWindowRule() {
    // use gaps_out

    // other propes too need to think about how to test
}

static void testFsWorkspaceRule() {
    // use gaps_out

    // other propes too need to think about how to test
}


static void testFsCycleFsWithFocus() {

    // movefocus_cycles_fullscreen = true


    // movefocus_cycles_fullscreen = false
}



static void testFsPinnedWindows() {

    // allow_pin_fullscreen = true

    // allow_pin_fullscreen = false
}

// todo rename this as this can be confused with a test for 'ability to focus on windows under FS' feature
static void testFsFocusUnderFs() {

    // on_focus_under_fullscreen = 0
    // on_focus_under_fullscreen = 1
    // on_focus_under_fullscreen = 2

}


