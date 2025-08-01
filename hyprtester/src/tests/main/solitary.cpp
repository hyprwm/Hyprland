#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static bool test() {
    NLog::log("{}Testing solitary clients", Colors::GREEN);

    OK(getFromSocket("/keyword general:allow_tearing false"));
    OK(getFromSocket("/keyword render:direct_scanout 0"));
    OK(getFromSocket("/keyword cursor:no_hardware_cursors 1"));
    NLog::log("{}Expecting blocked solitary/DS/tearing", Colors::YELLOW);
    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("solitary: 0\n"), true);
        EXPECT(str.contains("solitaryBlockedBy: windowed mode,missing candidate"), true);
        EXPECT(str.contains("activelyTearing: false"), true);
        EXPECT(str.contains("tearingBlockedBy: next frame is not torn,user settings,not supported by monitor,missing candidate"), true);
        EXPECT(str.contains("directScanoutTo: 0\n"), true);
        EXPECT(str.contains("directScanoutBlockedBy: user settings,missing candidate"), true);
    }

    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();

    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    OK(getFromSocket("/keyword general:allow_tearing true"));
    OK(getFromSocket("/keyword render:direct_scanout 1"));
    OK(getFromSocket("/keyword cursor:no_hardware_cursors 0"));
    OK(getFromSocket("/dispatch fullscreen 1"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    NLog::log("{}Expecting kitty to almost pass for solitary/DS/tearing", Colors::YELLOW);
    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("solitary: 0\n"), false);
        EXPECT(str.contains("solitaryBlockedBy: null"), true);
        EXPECT(str.contains("activelyTearing: false"), true);
        EXPECT(str.contains("tearingBlockedBy: next frame is not torn,not supported by monitor,window settings"), true);
        EXPECT(str.contains("directScanoutTo: 0\n"), false);
        EXPECT(str.contains("directScanoutBlockedBy: null"), true);
    }

    OK(getFromSocket("/dispatch setprop active immediate 1"));
    NLog::log("{}Expecting kitty to almost pass for tearing", Colors::YELLOW);
    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("tearingBlockedBy: next frame is not torn,not supported by monitor\n"), true);
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Restoring config values", Colors::YELLOW);
    OK(getFromSocket("/keyword general:allow_tearing false"));
    OK(getFromSocket("/keyword render:direct_scanout 0"));
    OK(getFromSocket("/keyword cursor:no_hardware_cursors 2"));

    return !ret;
}

REGISTER_TEST_FN(test)
