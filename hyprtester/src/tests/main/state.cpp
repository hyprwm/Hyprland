#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

#include <chrono>
#include <thread>

TEST_CASE(state) {
    NLog::log("{}Testing Fallback State", Colors::YELLOW);

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

    OK(getFromSocket("/reload"));
}
