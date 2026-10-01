#include "tests.hpp"
#include "../../hyprctlCompat.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

TEST_CASE(xwaylandSelectionSourceGuard) {
    Hyprutils::Utils::CScopeGuard cleanup([] { Tests::killAllWindows(); });

    // Trigger lazy XWayland startup and wait for a mapped X11 client before probing the WM.
    OK(getFromSocket("/dispatch hl.dsp.exec_cmd('xeyes')"));
    Tests::waitUntilWindowsN(1);
    ASSERT_CONTAINS(getFromSocket("/clients"), "class: XEyes\n");

    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_selection_guard('clipboard', true)"));
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_selection_guard('primary', true)"));
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_selection_guard('clipboard', false)"));
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_selection_guard('primary', false)"));
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_xwayland_selection_guard('dnd', true)"));
}
