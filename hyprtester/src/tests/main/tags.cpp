#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

TEST_CASE(tags) {
    NLog::log("{}Spawning kittyProcA&B on ws 1", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty("tagged");
    auto kittyProcB = Tests::spawnKitty("untagged");

    if (!kittyProcA || !kittyProcB) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Testing testTag tags", Colors::YELLOW);

    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-1', tag = '+testTag' })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-1', match = { class = 'tagged' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-2', match = { tag = 'negative:testTag' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-2', no_shadow = true })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-3', match = { tag = 'testTag' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'tag-test-3', no_dim = true })"));

    ASSERT(Tests::windowCount(), 2);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:tagged' })"));
    NLog::log("{}Testing tagged window for no_dim 0 & no_shadow", Colors::YELLOW);
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    ASSERT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "true");
    ASSERT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "false");
    NLog::log("{}Testing untagged window for no_dim & no_shadow", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:untagged' })"));
    ASSERT_NOT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    ASSERT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "true");
    ASSERT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "false");
}
