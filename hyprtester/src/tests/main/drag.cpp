#include <cmath>
#include <format>
#include <string>
#include <utility>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

/// Parses an "X,Y" attribute (as returned by getAttribute for "at" and "size")
/// into a pair of ints. Returns {0, 0} on a malformed value.
static std::pair<int, int> parseXY(const std::string& value) {
    const auto COMMA = value.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};

    try {
        return {std::stoi(value.substr(0, COMMA)), std::stoi(value.substr(COMMA + 1))};
    } catch (...) { return {0, 0}; }
}

/// Window geometry is subject to rounding, gaps and borders, so positions are
/// compared with a small tolerance.
static bool nearly(int a, int b, int tolerance = 4) {
    return std::abs(a - b) <= tolerance;
}

static std::pair<int, int> activeWindowAt() {
    return parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "at"));
}

static std::pair<int, int> activeWindowSize() {
    return parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "size"));
}

/// Tests that dragging a tiled window with binds:drag_center_window enabled
/// centers the window on the cursor once it becomes floating.
TEST_CASE(dragTiledCentered) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = true } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));

    // grab well away from the center, so centering is observable
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the grab point is ignored, the floating window is centered on the cursor
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT(nearly(NEW_X + NEW_SIZE_X / 2, 700), true);
        EXPECT(nearly(NEW_Y + NEW_SIZE_Y / 2, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a tiled window with binds:drag_center_window disabled maps
/// the grab point proportionally onto the smaller floating window, instead of
/// centering it.
TEST_CASE(dragTiledProportional) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = false } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));

    // grab three quarters across and down the tiled window
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the window shrinks as it becomes floating, but the cursor should still
        // sit three quarters across and down
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT(nearly(NEW_X + (NEW_SIZE_X * 3) / 4, 700), true);
        EXPECT(nearly(NEW_Y + (NEW_SIZE_Y * 3) / 4, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a fullscreen window with binds:drag_center_window enabled
/// centers it on the cursor once it is restored to its floating size.
TEST_CASE(dragFullscreenCentered) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = true } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 600, y = 400, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 100, y = 100, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    // grab well away from the center, so centering is observable
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the grab point is ignored, the restored window is centered on the cursor
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
        EXPECT(nearly(NEW_X + NEW_SIZE_X / 2, 700), true);
        EXPECT(nearly(NEW_Y + NEW_SIZE_Y / 2, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a fullscreen window with binds:drag_center_window disabled
/// maps the grab point proportionally onto the restored floating window.
TEST_CASE(dragFullscreenProportional) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = false } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 600, y = 400, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 100, y = 100, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    // grab three quarters across and down the fullscreen surface, far away from
    // where the restored floating window would otherwise end up
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the window shrinks back to its floating size, but the cursor should still
        // sit three quarters across and down
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
        EXPECT(nearly(NEW_X + (NEW_SIZE_X * 3) / 4, 700, 8), true);
        EXPECT(nearly(NEW_Y + (NEW_SIZE_Y * 3) / 4, 500, 8), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}
