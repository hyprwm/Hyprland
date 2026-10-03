#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <format>

TEST_CASE(tags) {
    NLog::log("{}Spawning kittyProcA&B on ws 1", Colors::YELLOW);
    SPAWN_KITTY("tagged");
    SPAWN_KITTY("untagged");

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
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "true");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "false");
    NLog::log("{}Testing untagged window for no_dim & no_shadow", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:untagged' })"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "true");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "false");
}

TEST_CASE(tags_fullscreen_cleanup) {
    SPAWN_KITTY("hyprtester-16423-tags");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:hyprtester-16423-tags' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.tag({ tag = '+hyprtester-16423-foo' })"));
    ASSERT(Tests::getAttribute(getFromSocket("/activewindow"), "tags"), "hyprtester-16423-foo");

    for (const auto& tag : {
             "+hyprtester-16423-foo",
             "hyprtester-16423-foo",
             "+hyprtester-16423-foo*",
             "hyprtester-16423-foo*",
         }) {
        OK(getFromSocket(std::format("/eval hl.window_rule({{ name = 'hyprtester-16423-rule-{}', "
                                     "match = {{ class = '^hyprtester-16423-tags$', fullscreen = true }}, tag = '{}' }})",
                                     tag, tag)));
        Tests::sync();
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "tags"), "hyprtester-16423-foo");

        for (int cycle = 0; cycle < 2; ++cycle) {
            NLog::log("{}Testing fullscreen tag '{}' cleanup, cycle {}", Colors::YELLOW, tag, cycle + 1);

            OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ action = 'set' })"));
            Tests::sync();
            // The static tag must coexist with exactly one normalized dynamic tag.
            EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "tags"), "hyprtester-16423-foo, hyprtester-16423-foo*");

            OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ action = 'unset' })"));
            Tests::sync();
            EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "tags"), "hyprtester-16423-foo");
        }

        OK(getFromSocket(std::format("/eval hl.window_rule({{ name = 'hyprtester-16423-rule-{}', enabled = false }})", tag)));
        Tests::sync();
    }
}
