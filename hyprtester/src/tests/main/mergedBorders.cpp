#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <format>

// The isolated test monitor is 1920x1080 at scale 1. Query without focusing so scrolling's viewport stays put.
SUBTEST(mergedBorderGeometry, const std::string& cls, int x, int y, int width, int height) {
    Tests::sync();
    OK(getFromSocket(std::format("/eval local w = assert(hl.get_window('class:{}')); "
                                 "assert(w.at.x == {} and w.at.y == {} and w.size.x == {} and w.size.y == {}, "
                                 "string.format('%s: at %d,%d size %d,%d', w.class, w.at.x, w.at.y, w.size.x, w.size.y))",
                                 cls, x, y, width, height)));
}

TEST_CASE(mergedBordersDwindleToggle) {
    OK(getFromSocket("/eval hl.config({ general = { border_size = 3, gaps_in = 9, gaps_out = 20 }, dwindle = { force_split = 2 } })"));

    SPAWN_KITTY("a");
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 1874, 1034);
    SPAWN_KITTY("b");

    // Disabled by default: both full borders and both configured inner gaps remain.
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 925, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 972, 23, 925, 1034);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true } })"));
    // A 3px shared border reserves 1px on the left window's right and 2px on the right window's left.
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 935, 1034);
    EXPECT(getFromSocket("/getprop class:a rounding"), "10");

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = false } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 925, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 972, 23, 925, 1034);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('togglesplit')"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 1874, 516);
    CALL_SUBTEST(mergedBorderGeometry, "b", 23, 542, 1874, 515);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = false } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 1874, 505);
    CALL_SUBTEST(mergedBorderGeometry, "b", 23, 552, 1874, 505);
}

TEST_CASE(mergedBordersMasterWorkspaceGaps) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'master', border_size = 3, gaps_in = 9, gaps_out = 20 }, "
                     "master = { mfact = 0.5, new_status = 'slave', orientation = 'left' } })"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = '1', gaps_in = 17 })"));

    SPAWN_KITTY("a");
    SPAWN_KITTY("b");
    SPAWN_KITTY("c");
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 917, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 980, 23, 917, 497);
    CALL_SUBTEST(mergedBorderGeometry, "c", 980, 560, 917, 497);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true } })"));
    // Exercise the T-junction between the master and the two vertically stacked slaves.
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 935, 516);
    CALL_SUBTEST(mergedBorderGeometry, "c", 962, 542, 935, 515);

    // New configured values are retained while their effective inner gaps stay zero.
    OK(getFromSocket("/eval hl.config({ general = { gaps_in = 13 } }); hl.workspace_rule({ workspace = '1', gaps_in = 11 })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "c", 962, 542, 935, 515);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = false } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 923, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 974, 23, 923, 503);
    CALL_SUBTEST(mergedBorderGeometry, "c", 974, 554, 923, 503);
}

TEST_CASE(mergedBordersMixedWidthsAndFloating) {
    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true, border_size = 3, gaps_in = 9, gaps_out = 20 }, dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'mixed-border', match = { class = '^c$' }, border_size = 7 })"));

    SPAWN_KITTY("a");
    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

    // The equal-width a/b edge must also remain unmerged when c differs. Inner gaps are still zero.
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 934, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 963, 23, 934, 514);
    CALL_SUBTEST(mergedBorderGeometry, "c", 967, 547, 926, 506);

    OK(getFromSocket("/eval hl.window_rule({ name = 'mixed-border', border_size = 3 })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 935, 516);
    CALL_SUBTEST(mergedBorderGeometry, "c", 962, 542, 935, 515);

    // Floating the incompatible participant restores merging without changing its border rule.
    OK(getFromSocket("/eval hl.window_rule({ name = 'mixed-border', border_size = 7 })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 934, 1034);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:c' })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 935, 1034);
    OK(getFromSocket("/eval assert(hl.get_window('class:c').floating)"));
    EXPECT(getFromSocket("/getprop class:c border_size"), "7");
}

TEST_CASE(mergedBordersGroups) {
    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true, border_size = 3, gaps_in = 9, gaps_out = 20 }, dwindle = { force_split = 2 }, "
                     "group = { auto_group = true, groupbar = { enabled = false, height = 10, indicator_height = 2, indicator_gap = 0, gaps_out = 0, "
                     "render_titles = true, stacked = false } } })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^b$' }, border_size = 7 })"));

    SPAWN_KITTY("a");
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    SPAWN_KITTY("b");
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));
    SPAWN_KITTY("c");

    // The group's current member participates in width compatibility.
    CALL_SUBTEST(mergedBorderGeometry, "b", 27, 27, 926, 1026);
    CALL_SUBTEST(mergedBorderGeometry, "c", 963, 23, 934, 1034);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    OK(getFromSocket("/eval assert(not hl.get_window('class:b').accepts_input)"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "c", 962, 23, 935, 1034);

    // A 12px reserved groupbar protects the entire space, including c's undecorated shared edge.
    OK(getFromSocket("/eval hl.config({ group = { groupbar = { enabled = true } } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 35, 934, 1022);
    CALL_SUBTEST(mergedBorderGeometry, "c", 963, 23, 934, 1034);

    OK(getFromSocket("/eval hl.config({ group = { groupbar = { enabled = false } } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "c", 962, 23, 935, 1034);
}

TEST_CASE(mergedBordersScrolling) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling', border_size = 3, gaps_in = 9, gaps_out = 20 }, "
                     "scrolling = { column_width = 0.5, fullscreen_on_one_column = false, focus_fit_method = 1, follow_focus = true } })"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = '1', gaps_in = 11 })"));
    SPAWN_KITTY("a");
    SPAWN_KITTY("b");
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    // Scrolling normally applies inner gaps on both sides of each column, including the tape ends.
    CALL_SUBTEST(mergedBorderGeometry, "a", 34, 23, 912, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 974, 23, 912, 1034);
    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 935, 1034);
    OK(getFromSocket("/eval hl.config({ general = { merge_borders = false } })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 34, 23, 912, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 974, 23, 912, 1034);

    OK(getFromSocket("/eval hl.config({ general = { merge_borders = true } })"));
    SPAWN_KITTY("c");
    SPAWN_KITTY("d");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 937, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "c", 1902, 23, 937, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "d", 2842, 23, 935, 1034);

    // d is entirely offscreen, but its differing border must disable merging for the visible pair too.
    OK(getFromSocket("/eval hl.window_rule({ name = 'offscreen-border', match = { class = '^d$' }, border_size = 7 })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 934, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 963, 23, 934, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "d", 2847, 27, 926, 1026);
    OK(getFromSocket("/eval hl.window_rule({ name = 'offscreen-border', border_size = 3 })"));
    CALL_SUBTEST(mergedBorderGeometry, "a", 23, 23, 936, 1034);
    CALL_SUBTEST(mergedBorderGeometry, "b", 962, 23, 937, 1034);
}

TEST_CASE(mergedBordersSamePhysicalWidthRuleChanges) {
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '0.5' }); "
                     "hl.config({ general = { merge_borders = true, border_size = 2, gaps_in = 9, gaps_out = 20 }, dwindle = { force_split = 2 } }); "
                     "hl.window_rule({ match = { class = '^b$', focus = false }, border_size = 1 })"));
    SPAWN_KITTY("a");
    SPAWN_KITTY("b");

    // Both logical widths reserve one physical pixel. Focus must still invalidate workspace compatibility.
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    Tests::sync();
    OK(getFromSocket("/eval local a = hl.get_window('class:a'); local b = hl.get_window('class:b'); assert(b.at.x - a.at.x - a.size.x == 4)"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    Tests::sync();
    OK(getFromSocket("/eval local a = hl.get_window('class:a'); local b = hl.get_window('class:b'); assert(b.at.x - a.at.x - a.size.x == 2)"));

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
}
