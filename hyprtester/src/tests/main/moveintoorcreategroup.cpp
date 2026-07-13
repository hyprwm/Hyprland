#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

TEST_CASE(moveIntoOrCreateGroup) {
    NLog::log("{}Dispatching workspace `moveintoorcreategroup`", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:moveintoorcreategroup' })");

    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));

    NLog::log("{}Spawning kittyA", Colors::YELLOW);
    auto kittyA = Tests::spawnKitty("kitty_A");
    if (!kittyA) {
        FAIL_TEST("Could not spawn kitty_A");
    }

    NLog::log("{}Spawning kittyB", Colors::YELLOW);
    auto kittyB = Tests::spawnKitty("kitty_B");
    if (!kittyB) {
        FAIL_TEST("Could not spawn kitty_B");
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 2);

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));

    NLog::log("{}Move kittyA into group with kittyB (creates group)", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'right' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped:");
    }

    NLog::log("{}Verify active window is kitty_A (the moved window)", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 0);

    NLog::log("{}Testing moveintoorcreategroup into existing group", Colors::YELLOW);

    NLog::log("{}Spawning kittyC", Colors::YELLOW);
    auto kittyC = Tests::spawnKitty("kitty_C");
    NLog::log("{}Spawning kittyD", Colors::YELLOW);
    auto kittyD = Tests::spawnKitty("kitty_D");
    NLog::log("{}Spawning kittyE", Colors::YELLOW);
    auto kittyE = Tests::spawnKitty("kitty_E");

    NLog::log("{}Expecting 3 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 3);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_D' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_E' })"));

    NLog::log("{}Move kittyE into existing group with kittyD", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'left' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped:");
    }

    NLog::log("{}Verify active window is kitty_E (the moved window)", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "kitty_E");
    }
}

// Moving a window from a group on one monitor into a group on another monitor
// must leave the moved window's monitor field set to the destination.
TEST_CASE(crossMonitorGroupJoin) {
    // We merge windows ourselves, suppress auto-grouping on spawn for predictability
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));

    // Create a destination monitor to the right of the default one and pin the
    // destination workspace to it
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-CROSSGROUP', mode = '1920x1080@60', position = 'auto-right', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-CROSSGROUP"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:groupcross-dst', monitor = 'HYPRTEST-CROSSGROUP' })"));

    // Source group: 2 kittys on the default monitor, merged into one group
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groupcross-src' })"));
    auto srcA = Tests::spawnKitty("groupcross_srcA");
    auto srcB = Tests::spawnKitty("groupcross_srcB");
    if (!srcA || !srcB)
        FAIL_TEST("Could not spawn source kittys");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:groupcross_srcB' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'left' })"));
    const auto MON_SRC_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // Destination group: 2 kittys on the new monitor, merged into one group
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groupcross-dst' })"));
    auto dstA = Tests::spawnKitty("groupcross_dstA");
    auto dstB = Tests::spawnKitty("groupcross_dstB");
    if (!dstA || !dstB)
        FAIL_TEST("Could not spawn destination kittys");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:groupcross_dstB' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'right' })"));
    const auto MON_DST_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // Sanity check: the two groups really do live on different monitors
    ASSERT_NOT(MON_SRC_ID, MON_DST_ID);

    // move srcA out of the source group rightward into the destination group on
    // the other monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:groupcross_srcA' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'right' })"));

    const auto active = getFromSocket("/activewindow");
    // moved window becomes active
    EXPECT_CONTAINS(active, "class: groupcross_srcA");
    // and it now lives on the destination monitor
    EXPECT(Tests::getAttribute(active, "monitor"), MON_DST_ID);

    Tests::killAllWindows();
    OK(getFromSocket("/output remove HYPRTEST-CROSSGROUP"));
}
