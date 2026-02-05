#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

static int ret = 0;

static bool test() {
    NLog::log("{}Testing monitor mode: and desc: selectors", Colors::GREEN);

    std::string monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, "HEADLESS-2");

    NLog::log("{}Testing mode: selector (matching)", Colors::YELLOW);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/keyword monitor ,preferred,auto,2"));
    OK(getFromSocket("/keyword monitor mode:1920x1080,preferred,auto,1"));

    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("scale": 1.00)");

    NLog::log("{}Testing mode: selector (non-matching)", Colors::YELLOW);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/keyword monitor ,preferred,auto,2"));
    OK(getFromSocket("/keyword monitor mode:2560x1440,preferred,auto,1"));

    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("scale": 2.00)");

    NLog::log("{}Testing desc: selector (matching)", Colors::YELLOW);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/keyword monitor ,preferred,auto,2"));
    OK(getFromSocket("/keyword monitor desc:,preferred,auto,1"));

    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("scale": 1.00)");

    NLog::log("{}Testing desc: selector (non-matching)", Colors::YELLOW);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/keyword monitor ,preferred,auto,2"));
    OK(getFromSocket("/keyword monitor desc:NonExistent,preferred,auto,1"));

    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("scale": 2.00)");

    return !ret;
}

REGISTER_TEST_FN(test)
