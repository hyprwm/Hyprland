#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <chrono>
#include <thread>

TEST_CASE(state) {
    NLog::log("{}Testing Fallback State", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '9' })"));
    SPAWN_KITTY("fallback-workspace-a");

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-1', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-3', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-4', disabled = true })"));

    Tests::sync();

    // wait for fallback to appear
    size_t fucker = 0;
    while (fucker++ < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        Tests::sync();

        if (getFromSocket("/monitors").contains("FALLBACK"))
            break;
    }

    {
        auto str = getFromSocket("/monitors");
        ASSERT_CONTAINS(str, "FALLBACK");
    }

    ASSERT_CONTAINS(getFromSocket("/workspaces"), "workspace ID 9 (9) on monitor FALLBACK:");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '9' })"));
    SPAWN_KITTY("fallback-workspace-b");

    OK(getFromSocket("/reload"));
    Tests::sync();

    ASSERT_CONTAINS(getFromSocket("/workspaces"), "workspace ID 9 (9) on monitor HEADLESS-2:");

    Tests::killAllWindows();
}
