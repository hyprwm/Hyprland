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
        EXPECT_CONTAINS(str, "solitary: 0\n");
        EXPECT_CONTAINS(str, "solitaryBlockedBy: windowed mode,missing candidate");
        EXPECT_CONTAINS(str, "activelyTearing: false");
        EXPECT_CONTAINS(str, "tearingBlockedBy: next frame is not torn,user settings,not supported by monitor,missing candidate");
        EXPECT_CONTAINS(str, "directScanoutTo: 0\n");
        EXPECT_CONTAINS(str, "directScanoutBlockedBy: user settings,software renders/cursors,missing candidate");
    }

    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();

    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    OK(getFromSocket("/keyword general:allow_tearing true"));
    OK(getFromSocket("/keyword render:direct_scanout 1"));
    NLog::log("{}", getFromSocket("/clients"));
    OK(getFromSocket("/dispatch fullscreen"));
    NLog::log("{}", getFromSocket("/clients"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    NLog::log("{}Expecting kitty to almost pass for solitary/DS/tearing", Colors::YELLOW);
    {
        auto str = getFromSocket("/monitors");
        EXPECT_NOT_CONTAINS(str, "solitary: 0\n");
        EXPECT_CONTAINS(str, "solitaryBlockedBy: null");
        EXPECT_CONTAINS(str, "activelyTearing: false");
        EXPECT_CONTAINS(str, "tearingBlockedBy: next frame is not torn,not supported by monitor,window settings");
    }

    OK(getFromSocket("/dispatch setprop active immediate 1"));
    NLog::log("{}Expecting kitty to almost pass for tearing", Colors::YELLOW);
    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "tearingBlockedBy: next frame is not torn,not supported by monitor\n");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test)
