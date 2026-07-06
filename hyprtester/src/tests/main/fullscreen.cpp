

// Temp Dump for moving around FS tests and blueprint junkyard for ideas



void testFsState() {

    // Tests::Fullscreen

    // Maximise


}


void setFsStateGroups(){
    // floating windows and all
}



// todo better name pls
void testFsUnderlyingWindowNonInterference() {

    // FS and unFS a window. Under deafult handled FS, all the window locations, sizes, etc... should remain exactly as they were before being FSed

}


void testMovingFsBetweenWorkspaces() {

    // move a FS window between 2 workspaces with the same FS Handler (default here)



}



void testFloatUnfloatFsWindow() {


    // float tiled FS window


    // unFloat(tile) floating FS window


    // float tiled FS window, then retile it while it is Fs.


    // tile floating Fs window, then float it again.
      // unFS it and check that the size is the same as it was before being FSed at all


}


void testWindowHidingUnderFs() {

    // check m_allowedOverTests::Fullscreen values of windows under FS.


    // all other windows other than the FSed window should be under it upon FSing.
    // All opened tiled windows that we don't switch to (or ignore the opening of and don't unFS the current window) should remain under it
    // All floating windows opened after the window was FSed should remain above it
    // unFs that window - no FS windows should remain

}


void testFsWindowSwitchingFsAnotherWindow() {
    // Fs tiled window, open floating window, FS that. Fs should have cycled. unFs that window, no Fs windows should remain
}





void testFsWindowRule() {
    // use gaps_out

    // other propes too need to think about how to test
}

void testFsWorkspaceRule() {
    // use gaps_out

    // other propes too need to think about how to test
}


void testFsCycleFsWithFocus() {

    // movefocus_cycles_Tests::Fullscreen = true


    // movefocus_cycles_Tests::Fullscreen = false
}



void testFsPinnedWindows() {

    // allow_pin_Tests::Fullscreen = true

    // allow_pin_Tests::Fullscreen = false
}

// todo rename this as this can be confused with a test for 'ability to focus on windows under FS' feature
void testFsFocusUnderFs() {

    // on_focus_under_Tests::Fullscreen = 0
    // on_focus_under_Tests::Fullscreen = 1
    // on_focus_under_Tests::Fullscreen = 2


}


// TEST_CASE(temp) {

//     NLog::log("{}Testing new_window_takes_over_fullscreen", Colors::YELLOW);

//     OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

//     Tests::spawnKitty("kitty_A");

//     OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//         EXPECT_CONTAINS(str, "kitty_A");
//     }

//     Tests::spawnKitty("kitty_B");

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//         EXPECT_CONTAINS(str, "kitty_A");
//     }

//     OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

//     {
//         // should be ignored as per focus_under_fullscreen 0
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//         EXPECT_CONTAINS(str, "kitty_A");
//     }

//     OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

//     Tests::spawnKitty("kitty_C");

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//         EXPECT_CONTAINS(str, "kitty_C");
//     }

//     OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

//     Tests::spawnKitty("kitty_D");

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 0");
//         EXPECT_CONTAINS(str, "fullscreenClient: 0");
//         EXPECT_CONTAINS(str, "kitty_D");
//     }

//     OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

//     Tests::killAllWindows();

//     NLog::log("{}Testing exit_window_retains_fullscreen", Colors::YELLOW);

//     OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = false } })"));

//     Tests::spawnKitty("kitty_A");
//     Tests::spawnKitty("kitty_B");

//     OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//     }

//     OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
//     Tests::waitUntilWindowsN(1);

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 0");
//         EXPECT_CONTAINS(str, "fullscreenClient: 0");
//     }

//     Tests::spawnKitty("kitty_B");
//     OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
//     OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = true } })"));

//     OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
//     Tests::waitUntilWindowsN(1);

//     {
//         auto str = getFromSocket("/activewindow");
//         EXPECT_CONTAINS(str, "fullscreen: 2");
//         EXPECT_CONTAINS(str, "fullscreenClient: 2");
//     }

//     Tests::killAllWindows();

// }