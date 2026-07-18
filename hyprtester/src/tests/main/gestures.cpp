#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <format>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::String;

#define UP CUniquePointer
#define SP CSharedPointer

// TODO: refactor and reuse `Tests::waitUntilWindowsN`
static bool waitForWindowCount(int expectedWindowCnt, std::string_view expectation, int waitMillis = 100, int maxWaitCnt = 50) {
    int counter = 0;
    while (Tests::windowCount() != expectedWindowCnt) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMillis));

        if (counter > maxWaitCnt) {
            NLog::log("{}Unmet expectation: {}", Colors::RED, expectation);
            return false;
        }
    }
    return true;
}

static std::string evalLua(std::string_view code) {
    return getFromSocket(std::format("/eval {}", code));
}

TEST_CASE(live_gesture_callbacks) {
    OK(evalLua(R"(
        __liveGesture = { swipe = { start = 0, update = 0, finish = 0 } }

        local function expect_eq(actual, expected, name)
            if actual ~= expected then
                error(name .. ': expected ' .. tostring(expected) .. ', got ' .. tostring(actual))
            end
        end

        local function expect_vec(vec, x, y, name)
            if vec == nil then
                error(name .. ': missing vector')
            end
            expect_eq(vec.x, x, name .. '.x')
            expect_eq(vec.y, y, name .. '.y')
        end

        hl.gesture({
            fingers = 6,
            direction = 'left',
            action = {
                start = function(e)
                    __liveGesture.swipe.start = __liveGesture.swipe.start + 1
                    expect_eq(e.phase, 'start', 'swipe start phase')
                    expect_eq(e.type, 'swipe', 'swipe start type')
                    expect_eq(e.direction, 'LEFT', 'swipe start direction')
                    expect_eq(e.fingers, 6, 'swipe start fingers')
                    expect_vec(e.delta, -300, 0, 'swipe start delta')
                end,
                update = function(e)
                    __liveGesture.swipe.update = __liveGesture.swipe.update + 1
                    expect_eq(e.phase, 'update', 'swipe update phase')
                    expect_eq(e.type, 'swipe', 'swipe update type')
                    expect_eq(e.direction, 'LEFT', 'swipe update direction')
                    expect_eq(e.fingers, 6, 'swipe update fingers')
                    expect_vec(e.delta, -300, 0, 'swipe update delta')
                end,
                finish = function(e)
                    __liveGesture.swipe.finish = __liveGesture.swipe.finish + 1
                    expect_eq(e.phase, 'end', 'swipe end phase')
                    expect_eq(e.type, 'swipe', 'swipe end type')
                    expect_eq(e.direction, 'LEFT', 'swipe end direction')
                    expect_eq(e.cancelled, false, 'swipe end cancelled')
                    expect_eq(e.fingers, nil, 'swipe end fingers')
                    expect_eq(e.delta, nil, 'swipe end delta')
                end,
            },
        })
    )"));

    OK(evalLua(R"(
        hl.plugin.test.gesture('left', 6)

        if __liveGesture.swipe.start ~= 1 or __liveGesture.swipe.update ~= 1 or __liveGesture.swipe.finish ~= 1 then
            error('unexpected swipe counts: start=' .. __liveGesture.swipe.start .. ', update=' .. __liveGesture.swipe.update .. ', end=' .. __liveGesture.swipe.finish)
        end
    )"));

    OK(evalLua(R"(
        __liveGesture.pinch = { start = 0, update = 0, finish = 0, last_scale = 0, last_rotation = 0 }

        local function expect_eq(actual, expected, name)
            if actual ~= expected then
                error(name .. ': expected ' .. tostring(expected) .. ', got ' .. tostring(actual))
            end
        end

        local function expect_vec(vec, x, y, name)
            if vec == nil then
                error(name .. ': missing vector')
            end
            expect_eq(vec.x, x, name .. '.x')
            expect_eq(vec.y, y, name .. '.y')
        end

        hl.gesture({
            fingers = 7,
            direction = 'pinch',
            action = {
                start = function(e)
                    __liveGesture.pinch.start = __liveGesture.pinch.start + 1
                    expect_eq(e.phase, 'start', 'pinch start phase')
                    expect_eq(e.type, 'pinch', 'pinch start type')
                    expect_eq(e.direction, 'PINCH_IN', 'pinch start direction')
                    expect_eq(e.fingers, 7, 'pinch start fingers')
                    expect_vec(e.delta, 11, 12, 'pinch start delta')
                    expect_eq(e.scale, 1.2, 'pinch start scale')
                    expect_eq(e.rotation, 13, 'pinch start rotation')
                end,
                update = function(e)
                    __liveGesture.pinch.update = __liveGesture.pinch.update + 1
                    expect_eq(e.phase, 'update', 'pinch update phase')
                    expect_eq(e.type, 'pinch', 'pinch update type')
                    expect_eq(e.direction, 'PINCH_IN', 'pinch update direction')
                    expect_eq(e.fingers, 7, 'pinch update fingers')
                    __liveGesture.pinch.last_scale = e.scale
                    __liveGesture.pinch.last_rotation = e.rotation
                end,
                finish = function(e)
                    __liveGesture.pinch.finish = __liveGesture.pinch.finish + 1
                    expect_eq(e.phase, 'end', 'pinch end phase')
                    expect_eq(e.type, 'pinch', 'pinch end type')
                    expect_eq(e.direction, 'PINCH', 'pinch end direction')
                    expect_eq(e.cancelled, false, 'pinch end cancelled')
                    expect_eq(e.fingers, nil, 'pinch end fingers')
                    expect_eq(e.delta, nil, 'pinch end delta')
                end,
            },
        })
    )"));

    OK(evalLua(R"(
        hl.plugin.test.pinch_update(7, 1.2, 11, 12, 13)
        hl.plugin.test.pinch_update(7, 1.4, 21, 22, 23)
        hl.plugin.test.pinch_end()

        if __liveGesture.pinch.start ~= 1 or __liveGesture.pinch.update ~= 2 or __liveGesture.pinch.finish ~= 1 then
            error('unexpected pinch counts: start=' .. __liveGesture.pinch.start .. ', update=' .. __liveGesture.pinch.update .. ', end=' .. __liveGesture.pinch.finish)
        end
        if __liveGesture.pinch.last_scale ~= 1.4 or __liveGesture.pinch.last_rotation ~= 23 then
            error('unexpected last pinch payload: scale=' .. tostring(__liveGesture.pinch.last_scale) .. ', rotation=' .. tostring(__liveGesture.pinch.last_rotation))
        end
    )"));

    OK(evalLua(R"(
        __liveGesture.legacy = { count = 0, argc = -1 }

        hl.gesture({
            fingers = 6,
            direction = 'right',
            action = function(...)
                __liveGesture.legacy.count = __liveGesture.legacy.count + 1
                __liveGesture.legacy.argc = select('#', ...)
            end,
        })

        hl.plugin.test.gesture('right', 6)

        if __liveGesture.legacy.count ~= 1 or __liveGesture.legacy.argc ~= 0 then
            error('legacy gesture callback changed: count=' .. __liveGesture.legacy.count .. ', argc=' .. __liveGesture.legacy.argc)
        end
    )"));

    EXPECT_CONTAINS(evalLua("hl.gesture({ fingers = 8, direction = 'up', action = { start = 1 } })"), "action.start must be a function");
    EXPECT_CONTAINS(evalLua("hl.gesture({ fingers = 8, direction = 'up', action = {} })"), "must define at least one of start, update, end, or finish");
}

// TODO: decompose this into multiple test cases
TEST_CASE(gestures) {
    Tests::spawnKitty();
    ASSERT(Tests::windowCount(), 1);

    // Give the shell a moment to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    OK(getFromSocket("/eval hl.plugin.test.gesture('up', 5)"));
    OK(getFromSocket("/eval hl.plugin.test.gesture('down', 5)"));
    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 5)"));
    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 5)"));
    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 4)"));

    EXPECT(waitForWindowCount(0, "Gesture sent paste exit + enter to kitty"), true);

    EXPECT(Tests::windowCount(), 0);

    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 3)"));

    EXPECT(waitForWindowCount(1, "Gesture spawned kitty"), true);

    EXPECT(Tests::windowCount(), 1);

    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/eval hl.plugin.test.gesture('down', 3)"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/eval hl.plugin.test.gesture('down', 3)"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    OK(getFromSocket("/eval hl.plugin.test.alt(1)"));

    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    // check for crashes
    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/eval hl.config({ gestures = { workspace_swipe_invert = 0 } })"));

    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/eval hl.config({ gestures = { workspace_swipe_invert = 1 } })"));
    OK(getFromSocket("/eval hl.config({ gestures = { workspace_swipe_create_new = 0 } })"));

    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 3)"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
        EXPECT_CONTAINS(str, "ID 1 (1)");
    }

    OK(getFromSocket("/eval hl.plugin.test.gesture('down', 3)"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 0");
    }

    OK(getFromSocket("/eval hl.plugin.test.alt(0)"));

    OK(getFromSocket("/eval hl.plugin.test.gesture('up', 3)"));

    EXPECT(waitForWindowCount(0, "Gesture closed kitty"), true);

    ASSERT(Tests::windowCount(), 0);

    // This test ensures that `movecursortocorner`, which expects
    // a single-character direction argument, is parsed correctly.
    Tests::spawnKitty();
    OK(getFromSocket("/dispatch hl.dsp.cursor.move_to_corner({ corner = 0, window = 'activewindow' })"));
    const std::string cursorPos1 = getFromSocket("/cursorpos");
    OK(getFromSocket("/eval hl.plugin.test.gesture('left', 4)"));
    const std::string cursorPos2 = getFromSocket("/cursorpos");
    // The cursor should have moved because of the gesture
    EXPECT(cursorPos1 != cursorPos2, true);

    // Test that `workspace previous` works correctly after a workspace gesture.
    {
        OK(getFromSocket("/eval hl.config({ gestures = { workspace_swipe_invert = 0 } })"));
        OK(getFromSocket("/eval hl.config({ gestures = { workspace_swipe_create_new = 1 } })"));
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));

        // Come to workspace 5 from workspace 3: 5 will remember that.
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '5' })"));
        Tests::spawnKitty(); // Keep workspace 5 open

        // Swipe from 1 to 5: 5 shall remember that.
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
        OK(getFromSocket("/eval hl.plugin.test.alt(1)"));
        OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));
        OK(getFromSocket("/eval hl.plugin.test.alt(0)"));
        EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 5 (5)");

        // Must return to 1 rather than 3
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));
        EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1 (1)");

        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));
        EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 5 (5)");

        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    }
    const std::string cursorPosBeforePinch = getFromSocket("/cursorpos");

    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 500, y = 500 })"));
    OK(getFromSocket("/eval hl.config({ cursor = { zoom_factor = 1 } })"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1, 0.01)"));

    OK(getFromSocket("/eval hl.plugin.test.pinch_update(2, 1.2)"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1.2, 0.01)"));
    OK(getFromSocket("/eval hl.plugin.test.pinch_update(2, 1.6)"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1.6, 0.01)"));
    OK(getFromSocket("/eval hl.plugin.test.pinch_end()"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1.6, 0.01)"));

    OK(getFromSocket("/eval hl.plugin.test.pinch_update(2, 0.64)"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1, 0.01)"));
    OK(getFromSocket("/eval hl.plugin.test.pinch_end()"));
    OK(getFromSocket("/eval hl.plugin.test.expect_cursor_zoom(1, 0.01)"));

    const auto comma = cursorPosBeforePinch.find(',');

    if (comma != std::string::npos) {
        auto xSv = std::string_view(cursorPosBeforePinch).substr(0, comma);
        auto ySv = std::string_view(cursorPosBeforePinch).substr(comma + 1);
        while (!xSv.empty() && xSv.front() == ' ')
            xSv.remove_prefix(1);
        while (!ySv.empty() && ySv.front() == ' ')
            ySv.remove_prefix(1);

        const auto x = strToNumber<int>(xSv);
        const auto y = strToNumber<int>(ySv);

        if (!x || !y)
            FAIL_TEST("Failed to restore cursor pos");

        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x.value(), y.value())));
    }
}
