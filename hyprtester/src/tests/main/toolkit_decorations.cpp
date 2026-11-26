#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <cmath>
#include <format>
#include <string>
#include <utility>

// parses an "X,Y" attribute of /activewindow
static std::pair<int, int> activeWindowAt() {
    const auto AT    = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const auto COMMA = AT.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};

    try {
        return {std::stoi(AT.substr(0, COMMA)), std::stoi(AT.substr(COMMA + 1))};
    } catch (...) { return {0, 0}; }
}

static bool nearly(int a, int b, int tolerance = 4) {
    return std::abs(a - b) <= tolerance;
}

TEST_CASE(toolkitDecorationsFollowTheRule) {
    OK(getFromSocket("/eval hl.decoration.register('test_bar', { edge = 'top', size = 30, create = function(deco) end })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_decorated' }, decorations = 'test_bar' })"));

    SPAWN_KITTY("kitty_decorated");
    SPAWN_KITTY("kitty_plain");

    EXPECT_CONTAINS(getFromSocket("/decorations class:kitty_decorated"), "Toolkit: test_bar");
    EXPECT_NOT_CONTAINS(getFromSocket("/decorations class:kitty_plain"), "Toolkit: test_bar");
}

TEST_CASE(toolkitDecorationsDragTheirWindow) {
    OK(getFromSocket("/eval hl.decoration.register('drag_bar', { edge = 'top', size = 30, create = function(deco) "
                     "deco:on('decoration.pointer_button', function(w, name, button, pressed) if pressed and name == deco.name then deco:drag() end end) end })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_dragged' }, float = true, size = {600, 400}, move = {660, 340}, decorations = 'drag_bar' })"));

    SPAWN_KITTY("kitty_dragged");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_dragged' })"));

    // the bar sits right above the surface
    const auto [AT_X, AT_Y] = activeWindowAt();
    const int GRAB_X        = AT_X + 300;
    const int GRAB_Y        = AT_Y - 15;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));

    // the first motion picks the window up, the next one moves it
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X + 5, GRAB_Y)));
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X + 105, GRAB_Y + 100)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));

    {
        const auto [NEW_X, NEW_Y] = activeWindowAt();
        EXPECT(nearly(NEW_X, AT_X + 100), true);
        EXPECT(nearly(NEW_Y, AT_Y + 100), true);
    }

    // a plain click doesn't move it
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X + 100, GRAB_Y + 100)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));

    {
        const auto [NEW_X, NEW_Y] = activeWindowAt();
        EXPECT(nearly(NEW_X, AT_X + 100), true);
        EXPECT(nearly(NEW_Y, AT_Y + 100), true);
    }
}
