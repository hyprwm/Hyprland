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

static bool sendMinimizeRequest(const std::string& appID, const std::string& action) {
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

    ASSERT(sendMinimizeRequest(APP_ID, "minimize"), true);
    ASSERT(getFromSocket("/repl return table.concat(_G.hyprtester_minimize_events, ',')"), std::format("{}:true", kitty->pid()));

    ASSERT(sendMinimizeRequest(APP_ID, "restore"), true);
    ASSERT(getFromSocket("/repl return table.concat(_G.hyprtester_minimize_events, ',')"), std::format("{}:true,{}:false", kitty->pid(), kitty->pid()));
}
