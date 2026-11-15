#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <array>
#include <chrono>
#include <format>
#include <string>
#include <thread>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

// RIGHT is created before LEFT. XWayland's monitor layout follows creation order, so there
// the two sit the other way around, which a pointer extrapolated from a surface gets wrong.
static constexpr const char* XWL_RIGHT = "HYPRTEST-XWL-RIGHT";
static constexpr const char* XWL_LEFT  = "HYPRTEST-XWL-LEFT";

static std::string           xwlMonitorBlock(const std::string& name) {
    const auto MONITORS = getFromSocket("/monitors all");
    const auto BEGIN    = MONITORS.find(std::format("Monitor {} (ID ", name));
    if (BEGIN == std::string::npos)
        return "";

    const auto END = MONITORS.find("\n\n", BEGIN);
    return MONITORS.substr(BEGIN, END == std::string::npos ? std::string::npos : END - BEGIN);
}

static bool waitForXwlOutput(const std::string& name, double scale, int x) {
    for (int i = 0; i < 50; ++i) {
        const auto BLOCK = xwlMonitorBlock(name);
        if (BLOCK.contains(std::format("scale: {}\n", scale)) && BLOCK.contains(std::format(" at {}x0\n", x)) && BLOCK.contains("disabled: false"))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    NLog::log("XWayland test output did not settle: {}", xwlMonitorBlock(name));
    return false;
}

static bool waitForXwlOutputRemoved(const std::string& name) {
    for (int i = 0; i < 50; ++i) {
        if (xwlMonitorBlock(name).empty())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

static std::pair<int, int> parseXY(const std::string& value) {
    const auto COMMA = value.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};

    try {
        return {std::stoi(value.substr(0, COMMA)), std::stoi(value.substr(COMMA + 1))};
    } catch (...) { return {0, 0}; }
}

/// Tests that while a held button keeps the pointer focus on an X11 window, the X server keeps
/// seeing the pointer where the cursor really is after it moves onto another monitor, with and
/// without force_zero_scaling. Dragging between X11 apps on different monitors depends on it.
TEST_CASE(xwaylandPointerFollowsCursorAcrossMonitors) {
    ASSERT(xwlMonitorBlock(XWL_RIGHT).empty(), true);
    ASSERT(xwlMonitorBlock(XWL_LEFT).empty(), true);
    bool        rightCreated = false, leftCreated = false, buttonHeld = false;
    CScopeGuard cleanup([&] {
        if (buttonHeld)
            EXPECT_OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
        Tests::killAllWindows();
        if (leftCreated) {
            EXPECT_OK(getFromSocket(std::format("/output remove {}", XWL_LEFT)));
            EXPECT(waitForXwlOutputRemoved(XWL_LEFT), true);
        }
        if (rightCreated) {
            EXPECT_OK(getFromSocket(std::format("/output remove {}", XWL_RIGHT)));
            EXPECT(waitForXwlOutputRemoved(XWL_RIGHT), true);
        }
        EXPECT_OK(getFromSocket("/reload"));
    });

    struct SScenario {
        bool        forceZeroScaling;
        double      rightScale;
        const char* rightScaleStr;
    };

    for (const auto& [FZS, RIGHT_SCALE, RIGHT_SCALE_STR] :
         std::array{SScenario{.forceZeroScaling = false, .rightScale = 1, .rightScaleStr = "1"}, SScenario{.forceZeroScaling = true, .rightScale = 2, .rightScaleStr = "2"}}) {
        NLog::log("{}Scenario: force_zero_scaling = {}, right monitor scale = {}", Colors::YELLOW, FZS, RIGHT_SCALE);

        OK(getFromSocket(std::format("/eval hl.config({{ xwayland = {{ force_zero_scaling = {} }} }})", FZS)));
        OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '800x600@60', position = '5800x0', scale = '{}' }}); "
                                     "hl.monitor({{ output = '{}', mode = '800x600@60', position = '5000x0', scale = '1' }})",
                                     XWL_RIGHT, RIGHT_SCALE_STR, XWL_LEFT)));

        if (!rightCreated) {
            const auto CREATED = getFromSocket(std::format("/output create headless {}", XWL_RIGHT));
            rightCreated       = CREATED == "ok";
            OK(CREATED);
        }
        if (!leftCreated) {
            const auto CREATED = getFromSocket(std::format("/output create headless {}", XWL_LEFT));
            leftCreated        = CREATED == "ok";
            OK(CREATED);
        }
        ASSERT(waitForXwlOutput(XWL_RIGHT, RIGHT_SCALE, 5800), true);
        ASSERT(waitForXwlOutput(XWL_LEFT, 1, 5000), true);

        // an X11 window on the right monitor, with the pointer pressed on it
        OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", XWL_RIGHT)));
        OK(getFromSocket("/dispatch hl.dsp.exec_cmd('xeyes')"));
        Tests::waitUntilWindowsN(1);
        ASSERT_CONTAINS(getFromSocket("/clients"), "class: XEyes\n");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:XEyes' })"));

        const auto [AT_X, AT_Y]     = parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "at"));
        const auto [SIZE_X, SIZE_Y] = parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "size"));
        ASSERT(AT_X >= 5800, true);
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", AT_X + (SIZE_X / 2), AT_Y + (SIZE_Y / 2))));
        OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
        buttonHeld = true;
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_pointer('XEyes')"));

        // with the button still held, onto the left monitor: focus stays on the X11 window
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 5300, y = 200 })"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_pointer('XEyes')"));

        OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
        buttonHeld = false;
        ASSERT(Tests::killAllWindows(), true);
    }
}
