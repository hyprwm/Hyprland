#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

static int  ret = 0;

static bool test() {
    NLog::log("{}Testing hyprctl monitors", Colors::GREEN);

    std::string monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("colorManagementPreset": "srgb")");

    EXPECT_CONTAINS(getFromSocket("/keyword monitor HEADLESS-2,1920x1080x60.00000,0x0,1.0,bitdepth,10,cm,wide"), "ok")
    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("colorManagementPreset": "wide")");

    EXPECT_CONTAINS(getFromSocket("/keyword monitor HEADLESS-2,1920x1080x60.00000,0x0,1.0,bitdepth,10,cm,srgb,sdrbrightness,1.2,sdrsaturation,0.98"), "ok")
    monitorsSpec = getFromSocket("j/monitors");
    EXPECT_CONTAINS(monitorsSpec, R"("colorManagementPreset": "srgb")");
    EXPECT_CONTAINS(monitorsSpec, R"("sdrBrightness": 1.20)");
    EXPECT_CONTAINS(monitorsSpec, R"("sdrSaturation": 0.98)");

    return !ret;
}

REGISTER_TEST_FN(test)
