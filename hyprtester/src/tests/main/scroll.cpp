#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SScrollClientBox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

static std::optional<std::string> scrollClientSectionForClass(const std::string& class_) {
    const auto CLIENTS    = getFromSocket("/clients");
    const auto CLASS_POS  = CLIENTS.find("class: " + class_);
    const auto ENTRY_POS  = CLASS_POS == std::string::npos ? std::string::npos : CLIENTS.rfind("Window ", CLASS_POS);
    const auto ENTRY_NEXT = CLASS_POS == std::string::npos ? std::string::npos : CLIENTS.find("\nWindow ", CLASS_POS);

    if (CLASS_POS == std::string::npos || ENTRY_POS == std::string::npos)
        return std::nullopt;

    return CLIENTS.substr(ENTRY_POS, ENTRY_NEXT == std::string::npos ? std::string::npos : ENTRY_NEXT - ENTRY_POS);
}

static std::optional<std::pair<int, int>> scrollParsePair(const std::string& value) {
    const auto COMMA = value.find(',');
    if (COMMA == std::string::npos)
        return std::nullopt;

    try {
        return std::pair{std::stoi(value.substr(0, COMMA)), std::stoi(value.substr(COMMA + 1))};
    } catch (...) { return std::nullopt; }
}

static std::optional<SScrollClientBox> scrollClientBoxForClass(const std::string& class_) {
    const auto SECTION = scrollClientSectionForClass(class_);
    if (!SECTION)
        return std::nullopt;

    const auto AT   = scrollParsePair(Tests::getAttribute(*SECTION, "at"));
    const auto SIZE = scrollParsePair(Tests::getAttribute(*SECTION, "size"));
    if (!AT || !SIZE)
        return std::nullopt;

    return SScrollClientBox{.x = AT->first, .y = AT->second, .w = SIZE->first, .h = SIZE->second};
}

static bool scrollSpawnWindows(const std::vector<std::string>& classes) {
    for (const auto& class_ : classes) {
        if (!Tests::spawnKitty(class_))
            return false;
    }

    return true;
}

TEST_CASE(scrollFocusCycling) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'up' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }
}

TEST_CASE(scrollFocusWrapping) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_focus to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: a");
    }

    // set wrap_focus to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: a");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }
}

TEST_CASE(scrollSwapcolWrapping) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_swapcol to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_swapcol to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }
}

TEST_CASE(scrollWindowRule) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing Scrolling Width", Colors::GREEN);

    // inject a new rule.
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', match = { class = 'kitty_scroll' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', scrolling_width = 0.1 })"));

    if (!Tests::spawnKitty("kitty_scroll")) {
        FAIL_TEST("Could not spawn kitty with win class `kitty_scroll`");
    }

    if (!Tests::spawnKitty("kitty_scroll")) {
        FAIL_TEST("Could not spawn kitty with win class `kitty_scroll`");
    }

    ASSERT(Tests::windowCount(), 2);

    // not the greatest test, but as long as res and gaps don't change, we good.
    // if this test breaks, it's likely you broke equal sizing
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "size: 179,1036");
}

TEST_CASE(scrollSpanExpandShrinkCollapse) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_a", "span_b", "span_c", "span_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_b' })"));

    const auto BASE_B = scrollClientBoxForClass("span_b");
    if (!BASE_B)
        FAIL_TEST("Could not find span_b geometry");

    ASSERT_MAX_DELTA(BASE_B->x, 0, 4);
    ASSERT_MAX_DELTA(BASE_B->y, 540, 4);
    ASSERT_MAX_DELTA(BASE_B->w, 960, 4);
    ASSERT_MAX_DELTA(BASE_B->h, 540, 4);

    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto EXPANDED_B = scrollClientBoxForClass("span_b");
    const auto C_ABOVE    = scrollClientBoxForClass("span_c");
    const auto D_ABOVE    = scrollClientBoxForClass("span_d");
    if (!EXPANDED_B || !C_ABOVE || !D_ABOVE)
        FAIL_TEST("Could not find geometry after expanding span_b right");

    ASSERT_MAX_DELTA(EXPANDED_B->x, 0, 4);
    ASSERT_MAX_DELTA(EXPANDED_B->y, 540, 4);
    ASSERT_MAX_DELTA(EXPANDED_B->w, 1920, 4);
    ASSERT_MAX_DELTA(EXPANDED_B->h, 540, 4);
    ASSERT((C_ABOVE->y + C_ABOVE->h) <= EXPANDED_B->y, true);
    ASSERT((D_ABOVE->y + D_ABOVE->h) <= EXPANDED_B->y, true);

    OK(getFromSocket("/dispatch hl.dsp.layout('shrink next')"));

    const auto SHRUNK_B = scrollClientBoxForClass("span_b");
    if (!SHRUNK_B)
        FAIL_TEST("Could not find span_b geometry after shrinking right");

    ASSERT_MAX_DELTA(SHRUNK_B->x, 0, 4);
    ASSERT_MAX_DELTA(SHRUNK_B->w, 960, 4);

    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('collapse')"));

    const auto COLLAPSED_B = scrollClientBoxForClass("span_b");
    if (!COLLAPSED_B)
        FAIL_TEST("Could not find span_b geometry after collapse");

    ASSERT_MAX_DELTA(COLLAPSED_B->x, 0, 4);
    ASSERT_MAX_DELTA(COLLAPSED_B->w, 960, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand prev')"));

    const auto EXPANDED_D = scrollClientBoxForClass("span_d");
    if (!EXPANDED_D)
        FAIL_TEST("Could not find span_d geometry after expanding left");

    ASSERT_MAX_DELTA(EXPANDED_D->x, 0, 4);
    ASSERT_MAX_DELTA(EXPANDED_D->y, 540, 4);
    ASSERT_MAX_DELTA(EXPANDED_D->w, 1920, 4);
    ASSERT_MAX_DELTA(EXPANDED_D->h, 540, 4);

    OK(getFromSocket("/dispatch hl.dsp.layout('shrink prev')"));

    const auto SHRUNK_D = scrollClientBoxForClass("span_d");
    if (!SHRUNK_D)
        FAIL_TEST("Could not find span_d geometry after shrinking left");

    ASSERT_MAX_DELTA(SHRUNK_D->x, 960, 4);
    ASSERT_MAX_DELTA(SHRUNK_D->w, 960, 4);
}

TEST_CASE(scrollSpanFitActiveKeepsWindowInView) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_fit_active_a", "span_fit_active_b", "span_fit_active_c", "span_fit_active_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_fit_active_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_fit_active_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit active')"));

    const auto AFTER_B = scrollClientBoxForClass("span_fit_active_b");
    if (!AFTER_B)
        FAIL_TEST("Could not find span_fit_active_b geometry after fit active");

    ASSERT((AFTER_B->w <= 1920), true);
}

TEST_CASE(scrollSpanRejectsFullHeightCollision) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_block_a", "span_block_b"}), true);
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_block_a' })"));

    const auto BEFORE_A = scrollClientBoxForClass("span_block_a");
    const auto BEFORE_B = scrollClientBoxForClass("span_block_b");
    if (!BEFORE_A || !BEFORE_B)
        FAIL_TEST("Could not find full-height span-blocking geometry before expansion");

    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto AFTER_A = scrollClientBoxForClass("span_block_a");
    const auto AFTER_B = scrollClientBoxForClass("span_block_b");
    if (!AFTER_A || !AFTER_B)
        FAIL_TEST("Could not find full-height span-blocking geometry after expansion");

    ASSERT_MAX_DELTA(AFTER_A->x, BEFORE_A->x, 4);
    ASSERT_MAX_DELTA(AFTER_A->y, BEFORE_A->y, 4);
    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_A->h, BEFORE_A->h, 4);
    ASSERT_MAX_DELTA(AFTER_B->x, BEFORE_B->x, 4);
    ASSERT_MAX_DELTA(AFTER_B->y, BEFORE_B->y, 4);
    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT_MAX_DELTA(AFTER_B->h, BEFORE_B->h, 4);
}

TEST_CASE(scrollSpanExpandsBelowExistingSpan) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_expand_stack_a", "span_expand_stack_b", "span_expand_stack_c", "span_expand_stack_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_stack_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_stack_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_stack_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_expand_stack_a");
    const auto BEFORE_B = scrollClientBoxForClass("span_expand_stack_b");
    if (!BEFORE_A || !BEFORE_B)
        FAIL_TEST("Could not find geometry before expanding below existing span");

    ASSERT_MAX_DELTA(BEFORE_A->w, 1920, 4);
    ASSERT_MAX_DELTA(BEFORE_B->w, 960, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_stack_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto AFTER_A = scrollClientBoxForClass("span_expand_stack_a");
    const auto AFTER_B = scrollClientBoxForClass("span_expand_stack_b");
    const auto AFTER_D = scrollClientBoxForClass("span_expand_stack_d");
    if (!AFTER_A || !AFTER_B || !AFTER_D)
        FAIL_TEST("Could not find geometry after expanding below existing span");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_B->w, 1920, 4);
    ASSERT(AFTER_D->y >= AFTER_B->y + AFTER_B->h, true);
}

TEST_CASE(scrollSpanExpandsMiddleRowBelowExistingSpan) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.333333, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_expand_middle_a", "span_expand_middle_b", "span_expand_middle_c", "span_expand_middle_d", "span_expand_middle_e", "span_expand_middle_f"}),
           true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_middle_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_middle_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_middle_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_middle_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_expand_middle_a");
    const auto BEFORE_B = scrollClientBoxForClass("span_expand_middle_b");
    if (!BEFORE_A || !BEFORE_B)
        FAIL_TEST("Could not find geometry before expanding middle row below existing span");

    ASSERT_MAX_DELTA(BEFORE_A->w, 1280, 4);
    ASSERT_MAX_DELTA(BEFORE_B->w, 640, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_middle_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto AFTER_A = scrollClientBoxForClass("span_expand_middle_a");
    const auto AFTER_B = scrollClientBoxForClass("span_expand_middle_b");
    const auto AFTER_D = scrollClientBoxForClass("span_expand_middle_d");
    const auto AFTER_E = scrollClientBoxForClass("span_expand_middle_e");
    if (!AFTER_A || !AFTER_B || !AFTER_D || !AFTER_E)
        FAIL_TEST("Could not find geometry after expanding middle row below existing span");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_B->w, 1280, 4);
    ASSERT(AFTER_D->y >= AFTER_B->y + AFTER_B->h, true);
    ASSERT(AFTER_E->y >= AFTER_D->y + AFTER_D->h, true);
}

TEST_CASE(scrollSpanExpandsFromCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.333333, follow_focus = false } })"));

    ASSERT(
        scrollSpawnWindows({"span_expand_covered_a", "span_expand_covered_b", "span_expand_covered_c", "span_expand_covered_d", "span_expand_covered_e", "span_expand_covered_f"}),
        true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_covered_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_covered_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_covered_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_covered_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_expand_covered_a");
    const auto BEFORE_E = scrollClientBoxForClass("span_expand_covered_e");
    if (!BEFORE_A || !BEFORE_E)
        FAIL_TEST("Could not find geometry before expanding from covered column");

    ASSERT_MAX_DELTA(BEFORE_A->w, 1280, 4);
    ASSERT_MAX_DELTA(BEFORE_E->w, 640, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_covered_e' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto AFTER_A = scrollClientBoxForClass("span_expand_covered_a");
    const auto AFTER_D = scrollClientBoxForClass("span_expand_covered_d");
    const auto AFTER_E = scrollClientBoxForClass("span_expand_covered_e");
    const auto AFTER_F = scrollClientBoxForClass("span_expand_covered_f");
    if (!AFTER_A || !AFTER_D || !AFTER_E || !AFTER_F)
        FAIL_TEST("Could not find geometry after expanding from covered column");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_E->w, 1280, 4);
    ASSERT(AFTER_E->y >= AFTER_D->y + AFTER_D->h, true);
    ASSERT_MAX_DELTA(AFTER_F->y + AFTER_F->h, AFTER_E->y, 4);
}

TEST_CASE(scrollSpanExpandsLeftFromCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.333333, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_expand_left_covered_a", "span_expand_left_covered_b", "span_expand_left_covered_c", "span_expand_left_covered_d", "span_expand_left_covered_e",
                               "span_expand_left_covered_f"}),
           true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_left_covered_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_left_covered_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_left_covered_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_left_covered_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_expand_left_covered_a");
    const auto BEFORE_E = scrollClientBoxForClass("span_expand_left_covered_e");
    if (!BEFORE_A || !BEFORE_E)
        FAIL_TEST("Could not find geometry before expanding left from covered column");

    ASSERT_MAX_DELTA(BEFORE_A->w, 1280, 4);
    ASSERT_MAX_DELTA(BEFORE_E->w, 640, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_left_covered_e' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand prev')"));

    const auto AFTER_A = scrollClientBoxForClass("span_expand_left_covered_a");
    const auto AFTER_C = scrollClientBoxForClass("span_expand_left_covered_c");
    const auto AFTER_D = scrollClientBoxForClass("span_expand_left_covered_d");
    const auto AFTER_E = scrollClientBoxForClass("span_expand_left_covered_e");
    if (!AFTER_A || !AFTER_C || !AFTER_D || !AFTER_E)
        FAIL_TEST("Could not find geometry after expanding left from covered column");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_E->x, 0, 4);
    ASSERT_MAX_DELTA(AFTER_E->w, 1280, 4);
    ASSERT(AFTER_E->y >= AFTER_C->y + AFTER_C->h, true);
    ASSERT(AFTER_E->y >= AFTER_D->y + AFTER_D->h, true);
}

TEST_CASE(scrollSpanRejectsExpansionBelowExistingSpanWithoutClearing) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_expand_reject_a", "span_expand_reject_b", "span_expand_reject_c"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_reject_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_expand_reject_a");
    const auto BEFORE_B = scrollClientBoxForClass("span_expand_reject_b");
    if (!BEFORE_A || !BEFORE_B)
        FAIL_TEST("Could not find geometry before rejected expansion below existing span");

    ASSERT_MAX_DELTA(BEFORE_A->w, 1920, 4);
    ASSERT_MAX_DELTA(BEFORE_B->w, 960, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expand_reject_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto AFTER_A = scrollClientBoxForClass("span_expand_reject_a");
    const auto AFTER_B = scrollClientBoxForClass("span_expand_reject_b");
    if (!AFTER_A || !AFTER_B)
        FAIL_TEST("Could not find geometry after rejected expansion below existing span");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
}

TEST_CASE(scrollSpanKeepsVirtualGapsAligned) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 10, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_gap_a", "span_gap_b", "span_gap_c"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_gap_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto B = scrollClientBoxForClass("span_gap_b");
    const auto C = scrollClientBoxForClass("span_gap_c");
    if (!B || !C)
        FAIL_TEST("Could not find geometry after expanding span_gap_a right");

    ASSERT_MAX_DELTA(B->y, C->y, 4);
    ASSERT_MAX_DELTA(B->h, C->h, 4);
}

TEST_CASE(scrollSpanKeepsInsertedColumnOutsideSpan) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_insert_a", "span_insert_b", "span_insert_c"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_insert_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_A = scrollClientBoxForClass("span_insert_a");
    if (!BEFORE_A)
        FAIL_TEST("Could not find span_insert_a geometry before inserting column");

    if (!Tests::spawnKitty("span_insert_d"))
        FAIL_TEST("Could not spawn span_insert_d");

    const auto AFTER_A = scrollClientBoxForClass("span_insert_a");
    if (!AFTER_A)
        FAIL_TEST("Could not find span_insert_a geometry after inserting column");

    ASSERT_MAX_DELTA(AFTER_A->w, BEFORE_A->w, 4);
}

TEST_CASE(scrollSpanShrinksWhenMiddleColumnRemoved) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.333333, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_remove_a", "span_remove_b", "span_remove_c", "span_remove_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_remove_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_remove_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_remove_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_remove_b geometry before removing middle column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1920, 4);

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:span_remove_c' })"));
    Tests::waitUntilWindowsN(3);

    const auto AFTER_B = scrollClientBoxForClass("span_remove_b");
    if (!AFTER_B)
        FAIL_TEST("Could not find span_remove_b geometry after removing middle column");

    ASSERT_MAX_DELTA(AFTER_B->w, 1280, 4);
    ASSERT(AFTER_B->w < BEFORE_B->w, true);
}

TEST_CASE(scrollSpanSurvivesClosingWindowInCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_close_a", "span_close_b", "span_close_c", "span_close_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_close_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_close_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_close_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_close_b geometry before closing window in covered column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1280, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_close_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:span_close_c' })"));
    Tests::waitUntilWindowsN(3);

    const auto AFTER_B = scrollClientBoxForClass("span_close_b");
    const auto AFTER_C = scrollClientBoxForClass("span_close_c");
    const auto AFTER_D = scrollClientBoxForClass("span_close_d");
    if (!AFTER_B || !AFTER_D)
        FAIL_TEST("Could not find geometry after closing window in covered column");

    ASSERT(!AFTER_C, true);
    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT(AFTER_D->x >= BEFORE_B->x && AFTER_D->x < BEFORE_B->x + BEFORE_B->w, true);
}

TEST_CASE(scrollSpanSurvivesUnrelatedConsume) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_consume_a", "span_consume_b", "span_consume_c", "span_consume_d", "span_consume_e"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_consume_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_consume_b geometry before unrelated consume");

    ASSERT_MAX_DELTA(BEFORE_B->w, 960, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));

    const auto AFTER_B = scrollClientBoxForClass("span_consume_b");
    if (!AFTER_B)
        FAIL_TEST("Could not find span_consume_b geometry after unrelated consume");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
}

TEST_CASE(scrollSpanSurvivesConsumeIntoCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_consume_into_a", "span_consume_into_b", "span_consume_into_c", "span_consume_into_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_into_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_into_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_consume_into_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_consume_into_b geometry before consuming into covered column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1280, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_consume_into_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));

    const auto AFTER_B = scrollClientBoxForClass("span_consume_into_b");
    const auto AFTER_C = scrollClientBoxForClass("span_consume_into_c");
    const auto AFTER_D = scrollClientBoxForClass("span_consume_into_d");
    if (!AFTER_B || !AFTER_C || !AFTER_D)
        FAIL_TEST("Could not find geometry after consuming into covered column");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT((AFTER_C->y + AFTER_C->h) <= AFTER_D->y || (AFTER_D->y + AFTER_D->h) <= AFTER_C->y, true);
}

TEST_CASE(scrollSpanSurvivesUnrelatedWindowMove) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_move_a", "span_move_b", "span_move_c", "span_move_d", "span_move_e"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_move_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_move_b geometry before unrelated move");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_e' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    const auto AFTER_B = scrollClientBoxForClass("span_move_b");
    if (!AFTER_B)
        FAIL_TEST("Could not find span_move_b geometry after unrelated move");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
}

TEST_CASE(scrollSpanSurvivesMoveIntoCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_move_into_a", "span_move_into_b", "span_move_into_c", "span_move_into_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_into_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_into_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_move_into_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_move_into_b geometry before moving into covered column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1280, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_move_into_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    const auto AFTER_B = scrollClientBoxForClass("span_move_into_b");
    const auto AFTER_C = scrollClientBoxForClass("span_move_into_c");
    const auto AFTER_D = scrollClientBoxForClass("span_move_into_d");
    if (!AFTER_B || !AFTER_C || !AFTER_D)
        FAIL_TEST("Could not find geometry after moving into covered column");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT((AFTER_C->y + AFTER_C->h) <= AFTER_D->y || (AFTER_D->y + AFTER_D->h) <= AFTER_C->y, true);
}

TEST_CASE(scrollSpanSurvivesSwapInCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_swap_a", "span_swap_b", "span_swap_c", "span_swap_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_swap_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_swap_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_swap_b");
    const auto BEFORE_C = scrollClientBoxForClass("span_swap_c");
    const auto BEFORE_D = scrollClientBoxForClass("span_swap_d");
    if (!BEFORE_B || !BEFORE_C || !BEFORE_D)
        FAIL_TEST("Could not find geometry before swapping in covered column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1280, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_swap_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.swap({ target = 'class:span_swap_d' })"));

    const auto AFTER_B = scrollClientBoxForClass("span_swap_b");
    const auto AFTER_C = scrollClientBoxForClass("span_swap_c");
    const auto AFTER_D = scrollClientBoxForClass("span_swap_d");
    if (!AFTER_B || !AFTER_C || !AFTER_D)
        FAIL_TEST("Could not find geometry after swapping in covered column");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT_MAX_DELTA(AFTER_C->x, BEFORE_D->x, 4);
    ASSERT_MAX_DELTA(AFTER_D->x, BEFORE_C->x, 4);
}

TEST_CASE(scrollSpanSurvivesMovingCoveredColumnToMonitor) {
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x1080', scale = '1' })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-SPAN-MON', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-SPAN-MON"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:span_monitor_src', monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:span_monitor_dst', monitor = 'HYPRTEST-SPAN-MON' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HYPRTEST-SPAN-MON' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:span_monitor_dst' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:span_monitor_src' })"));

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ binds = { window_direction_monitor_fallback = true } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.333333, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_monitor_a", "span_monitor_b", "span_monitor_c", "span_monitor_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_monitor_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_monitor_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_monitor_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_monitor_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_monitor_b geometry before moving covered column to monitor");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1920, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_monitor_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'up' })"));

    const auto AFTER_B = scrollClientBoxForClass("span_monitor_b");
    const auto AFTER_C = scrollClientBoxForClass("span_monitor_c");
    const auto AFTER_D = scrollClientBoxForClass("span_monitor_d");
    if (!AFTER_B || !AFTER_C || !AFTER_D)
        FAIL_TEST("Could not find geometry after moving covered column to monitor");

    const auto AFTER_B_SECTION = scrollClientSectionForClass("span_monitor_b");
    const auto AFTER_C_SECTION = scrollClientSectionForClass("span_monitor_c");
    const auto AFTER_D_SECTION = scrollClientSectionForClass("span_monitor_d");
    if (!AFTER_B_SECTION || !AFTER_C_SECTION || !AFTER_D_SECTION)
        FAIL_TEST("Could not find client sections after moving covered column to monitor");

    ASSERT_NOT(Tests::getAttribute(*AFTER_C_SECTION, "monitor"), Tests::getAttribute(*AFTER_B_SECTION, "monitor"));
    ASSERT(Tests::getAttribute(*AFTER_D_SECTION, "monitor"), Tests::getAttribute(*AFTER_B_SECTION, "monitor"));
    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT(AFTER_D->x >= AFTER_B->x && AFTER_D->x < AFTER_B->x + AFTER_B->w, true);
    ASSERT((AFTER_D->y + AFTER_D->h) <= AFTER_B->y || (AFTER_B->y + AFTER_B->h) <= AFTER_D->y, true);

    OK(getFromSocket("/output remove HYPRTEST-SPAN-MON"));
}

TEST_CASE(scrollSpanSurvivesConsumeOrExpelFromCoveredColumn) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"span_expel_a", "span_expel_b", "span_expel_c", "span_expel_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expel_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expel_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    const auto BEFORE_B = scrollClientBoxForClass("span_expel_b");
    if (!BEFORE_B)
        FAIL_TEST("Could not find span_expel_b geometry before expelling from covered column");

    ASSERT_MAX_DELTA(BEFORE_B->w, 1280, 4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expel_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:span_expel_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume_or_expel next')"));

    const auto AFTER_B = scrollClientBoxForClass("span_expel_b");
    const auto AFTER_C = scrollClientBoxForClass("span_expel_c");
    const auto AFTER_D = scrollClientBoxForClass("span_expel_d");
    if (!AFTER_B || !AFTER_C || !AFTER_D)
        FAIL_TEST("Could not find geometry after expelling from covered column");

    ASSERT_MAX_DELTA(AFTER_B->w, BEFORE_B->w, 4);
    ASSERT(AFTER_D->x >= AFTER_C->x + AFTER_C->w, true);
}

TEST_CASE(scrollFullscreen) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing Scrolling FS", Colors::GREEN);

    ASSERT(!!Tests::spawnKitty("kitty_scroll_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_C"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = \"class:kitty_scroll_B\" })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }
}

TEST_CASE(scrollMaximize) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing Scrolling Maximize", Colors::GREEN);

    ASSERT(!!Tests::spawnKitty("kitty_scroll_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_C"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = \"class:kitty_scroll_B\" })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({mode = 'maximized'})"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1870,1040");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1870,1040");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }
}

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocusFalse) {

    /*
     focuswindow DOES NOT move the scrolling view when follow_focus = false
     ---------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focuswindow dispatch SHOULD NOT move scrolling view when follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // if the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));
    if (posAx < 0) {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::GREEN, Colors::RESET, posAx);
    } else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, posAx);
    }
}

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocustrue) {

    /*
     focuswindow DOES move the view when follow_focus = true
     --------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focuswindow dispatch SHOULD move scrolling view when follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport.
    // If it is not, the view moved, which is what we expect to happen.
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));
    if (posAx < 0) {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be >= 0, got {}.", Colors::RED, Colors::RESET, posAx);
    } else {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be >= 0, got {}.", Colors::GREEN, Colors::RESET, posAx);
    }
}

TEST_CASE(testScrollingViewBehaviourFocusFallback) {

    /*
     Focus fallback from killing a floating window onto a tiled window must NOT move scrolling view, regardless of follow_focus
     --------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focus fallback from floating window to a tiled window should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with class `c`", Colors::RED);
    }

    // make it (window of class:c) float - the view now mush have shifted to fit window class:b
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:c'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));

    // kill the floating window
    // Expect the focus to fall back to the left tiled window
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:c'})"));
    Tests::waitUntilWindowsN(2);

    // The focus now must have fallen back to tiled window of class "a".

    // If the view did not move, we expect currently focused window's (class:a) to have "at: " x coordinat value <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourFocusFallbackWithGroups) {

    // same idea as testScrollingViewBehaviourFocusFallback, but with window of class "a" being grouped.

    NLog::log("{}Testing scrolling view behaviour: focus fallback from floating window to a grouped tiled should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // to correctly set up windows for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // only one tiled window will be grouped for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    // make it a grouped. There need not be any other windows in the group for this test
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with class `c`", Colors::RED);
    }

    // make it float - the view now mush have shifted to fit window class:b
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:c'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));

    // kill the floating window
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:c'})"));
    Tests::waitUntilWindowsN(2);

    // The focus now must have fallen back to tiled window of class "a".

    // If the view did not move, we expect currently focused window's (class:a) to have "at: " x coordinat value <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourWorkspaceChange) {

    /*
     When you change to a scrolling workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a scrolling workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // switch to workspace 1 for this test
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to workspace 2, then back to workspace 1 again
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '2'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused window, class:a, must be <0 (must be left of the viewport)
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourSpecialWorkspaceChange) {

    /*
     When you change to a special scrolling workspace from a normal workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a special scrolling workspace from a normal workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to workspace 2, then back to special "scroll_S" workspace again
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '2'})"));
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    // Reestablish focus since it is finnicky in hyprtester - Harmless and does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:c, must be <0 (must be left of the viewport)
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourSpecialToSpecialWorkspaceChange) {

    /*
    We also test switching between 2 special workspaces
    This follows the same idea and dependencies as the test testScrollingViewBehaviourSpecialWorkspaceChange()
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a special scrolling workspace from another special workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to special workspace "scroll_F", then back to special "scroll_S" workspace again
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_F')"));
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    // Reestablish focus since it is finnicky in hyprtester - Harmless and does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:c, must be <0 (must be left of the viewport)

    const std::string currentWindowPosSPECIAL  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosSPECIALX = currentWindowPosSPECIAL.substr(0, currentWindowPosSPECIAL.find(','));
    // test pass
    if (std::stoi(currentWindowPosSPECIALX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosSPECIALX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosSPECIALX);
    }
}

TEST_CASE(testScrollingViewBehaviourCloseWindowInGroup) {

    /*
     When you change close a window inside a group (NOT destroying the group!), it should not cause scrolling view to shift to pull that group into view, regardless of follow_focus
     -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: closing a window in a group (> 1 window in group) should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // We need 2 windows to be grouped, the third one not.
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // switch focus to group. This will not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // kill window class:b. we expect that this should cause not difference in the position of the group
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:b'})"));
    Tests::waitUntilWindowsN(2);

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:a, must be <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveWindowIntoGroupFollowFocusFalse) {

    /*
     when a window is moved inside a group, scrolling view should not move to fit that group when follow_focus = false
     -----------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: moving a window into a group SHOULD NOT move scrolling view if follow_focus = 0", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the view did not move, its x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'b' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'b' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveWindowInGroupFollowFocusTrue) {

    /*
    when a window is moved inside a group, scrolling view should move to fit that group when follow_focus = true
    ------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: moving a window in a group SHOULD move scrolling view if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the scrolling view did move, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}window of class 'b' does not have x coordinates >= 0 for its position: {}", Colors::RED, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'b' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourNewLayer) {

    /*
     Starting a program on a different layer shouldn't cause scrolling view to move to fit the window that was focused when this program was started, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: new program occupying another layer shouldn't move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // focus class:a - this does not move scrolling view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    NLog::log("{}Spawning kitty layer {}", Colors::YELLOW, "myLayer");
    if (!Tests::spawnLayerKitty("myLayer")) {
        FAIL_TEST("{}Error: {} layer did not spawn", Colors::RED, "myLayer");
    }

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }

    // TEST_CASE's own cleanup functions fail to kill all layers with this test. Manually do it

    // kill all layers
    NLog::log("{}Killing all layers", Colors::YELLOW);
    Tests::killAllLayers();
    ASSERT(Tests::layerCount(), 0);

    // kill all windows
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusFalse) {

    /*
     dispatching movefocus when follow_focus = false should not cause scrolling view to move
     ---------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus does not cause scrolling view to move if follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // we expect that after dispatching this, scrolling view must not have moved
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be < 0.
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusTrue) {

    /*
     dispatching movefocus when follow_focus = true should cause scrolling view to move
     ----------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus does cause scrolling view to move if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // we expect that after dispatching this, scrolling view must have moved since follow_focus = true
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view moved, class:a window's x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have x coordinates >= 0 for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'a' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusFalse) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must not move if follow_focus = false
     -----------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus within groups does not cause scrolling view to move if follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view did not move, its x coordinate for its `at:` value should be < 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusTrue) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must move if follow_focus = true
     ------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus within groups does causes scrolling view to move if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view moved, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have x coordinates >= 0 for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'a' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourScheduledPropRefresh) {

    /*
     Scheduled prop refresh must not move scrolling viewport.
     The reason a prop refresh was queued is not saved, therefore it is not possible to clearly tell when and when not to move scrolling viewport
     In this test, we test this by setting a workspace rule, which schedules a prop refresh
     --------------------------------------------------------------------------------------------------------------------------------------
    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
        return;
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
        return;
    }

    // since follow_focus = false, viewport does not move
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    // setting a workspace rule queues a doLater() call in the Event Loop Manager
    OK(getFromSocket("/eval hl.workspace_rule({workspace = hl.get_active_workspace().id,gaps_in = 0})"));

    // Check that the workspace rule is set
    ASSERT_CONTAINS(getFromSocket("/workspacerules"), "gapsIn: 0 0 0 0");

    // The viewport must not have moved: left corner cords of window should be < 0
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollInhibitor) {

    /*
        scroll inhibitor prevent the scrolling view from moving
        ---------------------------------------------------------------------------------
    */

    // set current layout to scrolling
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing inhibit_scroll", Colors::GREEN);

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
        return;
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
        return;
    }

    // Currently, we are focused on window class:b

    // enable scroll inhibitor
    OK(getFromSocket("/dispatch hl.dsp.layout('inhibit_scroll 1')"));

    // dispatching `layoutmsg focus l` will move scrolling view regardless of follow_focus if inhibitor is not working
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    // the focus must have moved regardless of the state of the inhibitor (it only prevents the scrolling view from moving). We are now focused on window class:a

    // if the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));
    if (posAx < 0) {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::GREEN, Colors::RESET, posAx);
    } else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, posAx);
        return;
    }
}

TEST_CASE(layoutmsg_fit_into_view) {

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
        return;
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
        return;
    }

    // class:a column is now off screen to the left

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    // fit class:a window into view

    OK(getFromSocket("/dispatch hl.dsp.layout('fit_into_view')"));

    // If it worked, class:a window must now have at: ~= 0,0 -- 0,0 + gaps, border = 22,22.

    ASSERT_CONTAINS(Tests::getAttribute(getFromSocket("/activewindow"), "at"), "22,22");
}

TEST_CASE(layoutRuleExpand) {
    // set current layout to scrolling
    OK(getFromSocket(
        "r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, border_size = 0, gaps_out = 0 }, scrolling = {column_width = 0.5, fullscreen_on_one_column = true} })"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
    }

    const std::string sizeSingle  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    const int         sizeSingleX = std::stoi(sizeSingle.substr(0, sizeSingle.find(',')));

    for (auto const& win : {"b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("dispatch hl.dsp.window.resize({x = 100, y = 500, window = 'class:a'})"));
    OK(getFromSocket("dispatch hl.dsp.window.resize({x = 100, y = 500, window = 'class:c'})"));

    OK(getFromSocket("dispatch hl.dsp.focus({window = 'class:b'})"));

    // const std::string sizeBefore  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    // const int         sizeBeforeX = std::stoi(sizeBefore.substr(0, sizeBefore.find(',')));

    OK(getFromSocket("/dispatch hl.dsp.layout('fit expand')"));

    const std::string sizeAfter  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    const int         sizeAfterX = std::stoi(sizeAfter.substr(0, sizeAfter.find(',')));

    if (sizeAfterX >= sizeSingleX - 200) {
        NLog::log("{}Passed: {}Expected the width of window of class \"b\" to take up all remaining space {}, got {}.", Colors::GREEN, Colors::RESET, sizeSingleX - 200,
                  sizeAfterX);
    } else {
        FAIL_TEST("{}Failed: {}Expected the width of window of class \"b\" to take up all remaining space {}, got {}.", Colors::RED, Colors::RESET, sizeSingleX - 200, sizeAfterX);
        return;
    }
}
TEST_CASE(scrollTapeOnClickOutOfWindow) {
    /*
     * Do not move tape on click in the direction, but out of the window  
     */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));
    OK(getFromSocket("r/eval hl.config({ general = { gaps_out = 100 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { follow_min_visible = 1.0, column_width = 0.6 } })"));
    OK(getFromSocket("r/eval hl.config({ input = { follow_mouse = 1 } })"));

    ASSERT(!!Tests::spawnKitty("A"), true); // A should be at x negative
    ASSERT(!!Tests::spawnKitty("B"), true);

    OK(getFromSocket("/eval hl.plugin.test.window_soft_focus('A')"));     // soft focus A
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 0, y = 20 })")); // move cursor to the gap zone

    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));

    const auto active = getFromSocket("/activewindow");
    ASSERT_CONTAINS(active, "class: A");

    const auto posA  = Tests::getAttribute(active, "at");
    const auto posAx = std::stoi(posA.substr(0, posA.find(',')));

    if (posAx < 0) {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"A\" to be < 0.", Colors::GREEN, Colors::RESET);
    } else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"A\" to be < 0, got {}.", Colors::RED, Colors::RESET, posAx);
    }
}
TEST_CASE(properFocusBehvaior) {
    // test that focus history does not fuck with proper workspace preference

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    OK(getFromSocket("/output create headless HEADLESS-3"));
    CScopeGuard x([&] { OK(getFromSocket("/output remove HEADLESS-3")); });

    auto        test = [&] {
        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));

        Tests::spawnKitty("a");
        Tests::waitUntilWindowsN(1);

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));

        Tests::spawnKitty("b");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
        Tests::spawnKitty("c");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));
        Tests::spawnKitty("d");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

        Tests::waitUntilWindowsN(4);

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: d");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: a");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: d");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        // now we have a situation of:
        // HEADLESS-2: a
        // HEADLESS-3: b | c d | -> b is offscreen

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: a");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        // now we have a history of a being more recent than b, but if we move left, we should still focus b.

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        Tests::killAllWindows();
        Tests::waitUntilWindowsN(0);
    };

    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 0 } })")); // set history mode, default
    test();

    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 1 } })")); // set length mode
    test();
}

TEST_CASE(scrollSpanExpandShrinkVertical) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'down', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"vspan_a", "vspan_b", "vspan_c", "vspan_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:vspan_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:vspan_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:vspan_b' })"));

    const auto BASE_B = scrollClientBoxForClass("vspan_b");
    if (!BASE_B)
        FAIL_TEST("Could not find vspan_b geometry");

    // Two strips: strip 0 (top) has a, strip 1 (bottom) has b, c, d.
    // Window b should be in strip 1, at half height.
    ASSERT_MAX_DELTA(BASE_B->y, 540, 4);
    ASSERT_MAX_DELTA(BASE_B->h, 540, 4);

    // Expand into strip 0 (prev = up in DOWN mode).
    OK(getFromSocket("/dispatch hl.dsp.layout('expand prev')"));

    const auto EXPANDED_B = scrollClientBoxForClass("vspan_b");
    if (!EXPANDED_B)
        FAIL_TEST("Could not find vspan_b geometry after expanding prev");

    // Should now span both strips (full height).
    ASSERT_MAX_DELTA(EXPANDED_B->y, 0, 4);
    ASSERT_MAX_DELTA(EXPANDED_B->h, 1080, 4);

    // Shrink back.
    OK(getFromSocket("/dispatch hl.dsp.layout('shrink prev')"));

    const auto SHRUNK_B = scrollClientBoxForClass("vspan_b");
    if (!SHRUNK_B)
        FAIL_TEST("Could not find vspan_b geometry after shrinking prev");

    ASSERT_MAX_DELTA(SHRUNK_B->y, 540, 4);
    ASSERT_MAX_DELTA(SHRUNK_B->h, 540, 4);

    // Expand into next strip (down in DOWN mode = next strip; boundary test).
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));
    const auto AT_BOUNDARY = scrollClientBoxForClass("vspan_b");
    if (!AT_BOUNDARY)
        FAIL_TEST("Could not find vspan_b geometry after expand next");
    ASSERT_MAX_DELTA(AT_BOUNDARY->y, 540, 4);

    // Collapse.
    OK(getFromSocket("/dispatch hl.dsp.layout('expand prev')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('collapse')"));

    const auto COLLAPSED_B = scrollClientBoxForClass("vspan_b");
    if (!COLLAPSED_B)
        FAIL_TEST("Could not find vspan_b geometry after collapse");
    ASSERT_MAX_DELTA(COLLAPSED_B->y, 540, 4);
    ASSERT_MAX_DELTA(COLLAPSED_B->h, 540, 4);
}

TEST_CASE(scrollSpanRejectsFullWidthCollisionVertical) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'down', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"fwc_a", "fwc_b", "fwc_c", "fwc_d"}), true);

    // Create a single strip: consume all windows into one strip.
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fwc_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fwc_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fwc_d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));

    // One strip has all 4 windows stacked horizontally. fwc_b occupies part of the secondary axis.
    // Expanding forward should fail if it creates a collision with another strip.
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fwc_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand prev')"));

    // Verify geometry is unchanged (expand was rejected or window still in single strip).
    const auto B_AFTER = scrollClientBoxForClass("fwc_b");
    if (!B_AFTER)
        FAIL_TEST("Could not find fwc_b geometry");
    ASSERT((B_AFTER->h < 1080), true);
}

TEST_CASE(scrollSpanSurvivesDirectionChange) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, gaps_out = 0, border_size = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'right', column_width = 0.5, follow_focus = false } })"));

    ASSERT(scrollSpawnWindows({"sdc_a", "sdc_b", "sdc_c", "sdc_d"}), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:sdc_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:sdc_c' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('consume')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:sdc_b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('expand next')"));

    // Verify the span is active (window spans full width).
    const auto EXPANDED = scrollClientBoxForClass("sdc_b");
    if (!EXPANDED)
        FAIL_TEST("Could not find sdc_b geometry after expand next");
    ASSERT_MAX_DELTA(EXPANDED->w, 1920, 4);

    // Change direction to vertical -- spans should be cleared.
    OK(getFromSocket("r/eval hl.config({ scrolling = { direction = 'down' } })"));

    // Force recalculate.
    OK(getFromSocket("/dispatch hl.dsp.layout('fit all')"));

    const auto AFTER_DIR_CHANGE = scrollClientBoxForClass("sdc_b");
    if (!AFTER_DIR_CHANGE)
        FAIL_TEST("Could not find sdc_b geometry after direction change");
    // Window should no longer span full height (span was cleared).
    ASSERT((AFTER_DIR_CHANGE->h < 1080), true);
}
