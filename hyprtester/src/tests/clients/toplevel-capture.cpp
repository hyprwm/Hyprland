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

static bool runToplevelCapture(const std::string& appID) {
    CProcess process(std::format("{}/toplevel-capture", binaryDir), {appID});
    process.addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    if (!process.runSync()) {
        NLog::log("{}Failed to run toplevel-capture helper", Colors::RED);
        return false;
    }

    if (process.exitCode() == 0)
        return true;

    NLog::log("{}toplevel-capture helper failed with exit code {}", Colors::RED, process.exitCode());
    return false;
}

TEST_CASE(windowShareCaptureProducesFrame) {
    constexpr auto APP_ID = "hyprtester-toplevel-capture";

    CScopeGuard    guard = {[&]() { Tests::killAllWindows(); }};

    auto           kitty = Tests::spawnKitty(APP_ID);
    if (!kitty)
        FAIL_TEST("Could not spawn kitty with class: {}", APP_ID);

    ASSERT(Tests::windowCount(), 1);

    ASSERT(runToplevelCapture(APP_ID), true);
}