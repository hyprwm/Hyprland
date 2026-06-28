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

TEST_CASE(crossMonitorFullscreenFocus) {
    // Create a destination monitor to the right of the default one and pin the
    // destination workspace to it
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-2', mode = '1920x1080@60', position = 'auto-right', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-2"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:workspace2', monitor = 'HYPRTEST-2' })"));

    // 1 window on the left monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace1' })"));
    auto src = Tests::spawnKitty("1A");
    if (!src)
        FAIL_TEST("Could not spawn kitty");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    const auto MON_SRC_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // 2 windows on the right monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace2' })"));
    auto dstA = Tests::spawnKitty("2A");
    auto dstB = Tests::spawnKitty("2B");
    if (!dstA || !dstB)
        FAIL_TEST("Could not spawn destination kittys");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    const auto MON_DST_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // Sanity check: the two windows really do live on different monitors
    ASSERT_NOT(MON_SRC_ID, MON_DST_ID);

    // float and fullscreen 2B
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'toggle' })"));

    // focus 1A and focus to the right
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"right\" })"));

    const auto active = getFromSocket("/activewindow");
    EXPECT_CONTAINS(active, "class: 2B");

    Tests::killAllWindows();
    OK(getFromSocket("/output remove HYPRTEST-2"));
}
