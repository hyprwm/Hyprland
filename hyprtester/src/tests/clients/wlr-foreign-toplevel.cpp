#include "../../Log.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <format>
#include <string>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

static bool sendToplevelRequest(const std::string& appID, const std::string& action) {
    CProcess process(std::format("{}/wlr-foreign-toplevel", binaryDir), {appID, action});
    process.addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    if (!process.runSync()) {
        NLog::log("{}Failed to run wlr-foreign-toplevel helper", Colors::RED);
        return false;
    }

    if (process.exitCode() == 0)
        return true;

    NLog::log("{}wlr-foreign-toplevel helper failed with exit code {}", Colors::RED, process.exitCode());
    return false;
}

TEST_CASE(wlrForeignToplevelMinimizeEvents) {
    constexpr auto APP_ID = "hyprtester-wlr-minimize";

    CScopeGuard    guard = {[&]() {
        getFromSocket("/eval do if _G.hyprtester_minimize_subscription then _G.hyprtester_minimize_subscription:remove() end; "
                      "_G.hyprtester_minimize_subscription = nil; _G.hyprtester_minimize_events = nil end");
        Tests::killAllWindows();
    }};

    OK(getFromSocket("/eval do _G.hyprtester_minimize_events = {}; _G.hyprtester_minimize_subscription = hl.on('window.minimize', function(window, state) "
                     "table.insert(_G.hyprtester_minimize_events, window.pid .. ':' .. tostring(state)) end) end"));

    auto kitty = Tests::spawnKitty(APP_ID);
    if (!kitty)
        FAIL_TEST("Could not spawn kitty with class: {}", APP_ID);

    ASSERT(getFromSocket("/repl return #_G.hyprtester_minimize_events"), "0");

    ASSERT(sendToplevelRequest(APP_ID, "minimize"), true);
    ASSERT(getFromSocket("/repl return table.concat(_G.hyprtester_minimize_events, ',')"), std::format("{}:true", kitty->pid()));

    ASSERT(sendToplevelRequest(APP_ID, "restore"), true);
    ASSERT(getFromSocket("/repl return table.concat(_G.hyprtester_minimize_events, ',')"), std::format("{}:true,{}:false", kitty->pid(), kitty->pid()));
}

SUBTEST(foreignToplevelFullscreenMode, pid_t pid, int mode) {
    const auto WINDOW = getFromSocket("/activewindow");
    ASSERT(Tests::getAttribute(WINDOW, "pid"), std::to_string(pid));
    ASSERT(Tests::getAttribute(WINDOW, "fullscreen"), std::to_string(mode));
    ASSERT(Tests::getAttribute(WINDOW, "fullscreenClient"), std::to_string(mode));
}

TEST_CASE(wlrForeignToplevelMaximizeRequests) {
    constexpr auto APP_ID = "hyprtester-wlr-maximize";
    CScopeGuard    guard  = {[]() { Tests::killAllWindows(); }};

    auto           kitty = Tests::spawnKitty(APP_ID);
    if (!kitty)
        FAIL_TEST("Could not spawn kitty with class: {}", APP_ID);

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", kitty->pid())));
    OK(getFromSocket(std::format("/dispatch hl.dsp.window.float({{ action = 'enable', window = 'pid:{}' }})", kitty->pid())));
    CALL_SUBTEST(foreignToplevelFullscreenMode, kitty->pid(), 0);

    // Foreign-toplevel requests are explicit state changes, unlike XDG's maximize toggles.
    for (const bool MAXIMIZED : {true, true, false, false}) {
        ASSERT(sendToplevelRequest(APP_ID, MAXIMIZED ? "maximize" : "unmaximize"), true);
        CALL_SUBTEST(foreignToplevelFullscreenMode, kitty->pid(), MAXIMIZED ? 1 : 0);
    }

    OK(getFromSocket(std::format("/dispatch hl.dsp.window.fullscreen({{ mode = 'fullscreen', action = 'set', window = 'pid:{}' }})", kitty->pid())));
    CALL_SUBTEST(foreignToplevelFullscreenMode, kitty->pid(), 2);

    for (const auto ACTION : {"maximize", "unmaximize"}) {
        ASSERT(sendToplevelRequest(APP_ID, ACTION), true);
        CALL_SUBTEST(foreignToplevelFullscreenMode, kitty->pid(), 2);
    }
}
