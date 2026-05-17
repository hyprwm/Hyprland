#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

TEST_CASE(state) {
    NLog::log("{}Testing Fallback State", Colors::YELLOW);

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-1', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-3', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-4', disabled = true })"));

    {
        auto str = getFromSocket("/monitors");
        ASSERT_CONTAINS(str, "FALLBACK");
    }

    OK(getFromSocket("/reload"));
}
