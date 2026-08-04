#include <chrono>
#include <thread>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

TEST_CASE(focusEffectConfig) {
    NLog::log("{}Testing decoration:focus_effect config", Colors::GREEN);

    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'none' } })"));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'flash' } })"));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'shrink' } })"));
    NOK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'explode' } })"));

    OK(getFromSocket("/eval hl.config({ decoration = { focus_flash_opacity = 0.25 } })"));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_shrink_percentage = 0.6 } })"));
    NOK(getFromSocket("/eval hl.config({ decoration = { focus_flash_opacity = 1.5 } })"));
    NOK(getFromSocket("/eval hl.config({ decoration = { focus_shrink_percentage = 0.05 } })"));

    // restore defaults
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'none' } })"));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_flash_opacity = 0.5 } })"));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_shrink_percentage = 0.8 } })"));
}

TEST_CASE(focusEffectAnimations) {
    NLog::log("{}Testing focus effect animation nodes", Colors::GREEN);

    auto str = getFromSocket("/animations");
    ASSERT_CONTAINS(str, "\tname: focus\n");
    ASSERT_CONTAINS(str, "\tname: focusFlash\n");
    ASSERT_CONTAINS(str, "\tname: focusShrink\n");
}

TEST_CASE(focusEffectDispatcher) {
    NLog::log("{}Testing hl.dsp.window.focus_effect", Colors::GREEN);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:focus_effect' })"));

    if (!Tests::spawnKitty("focus_effect_a"))
        FAIL_TEST("Could not spawn kitty");

    if (!Tests::spawnKitty("focus_effect_b"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:focus_effect_a' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.focus_effect({ effect = 'flash' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.focus_effect({ effect = 'shrink' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.focus_effect({ effect = 'none' })"));
    NOK(getFromSocket("/dispatch hl.dsp.window.focus_effect({ effect = 'explode' })"));

    // Config-driven focus path should not error.
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'flash' } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:focus_effect_b' })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'shrink' } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:focus_effect_a' })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    OK(getFromSocket("/eval hl.config({ decoration = { focus_effect = 'none' } })"));

    Tests::killAllWindows();
}
