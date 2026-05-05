#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <chrono>
#include <thread>

TEST_CASE(monitorsColorManagement) {
    std::string monitorsSpec = getFromSocket("j/monitors");
    ASSERT_CONTAINS(monitorsSpec, R"("colorManagementPreset": )");

    ASSERT_CONTAINS(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', bitdepth = 10, cm = 'wide' })"), "ok");

    // monitor settings are applied after a frame is pushed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    monitorsSpec = getFromSocket("j/monitors");
    ASSERT_CONTAINS(monitorsSpec, R"("colorManagementPreset": )");

    ASSERT_CONTAINS(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', bitdepth = 10, cm = 'srgb', sdrbrightness = 1.2, sdrsaturation = 0.98 })"), "ok");
    monitorsSpec = getFromSocket("j/monitors");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_CONTAINS(monitorsSpec, R"("colorManagementPreset": )");
    ASSERT_CONTAINS(monitorsSpec, R"("sdrBrightness": )");
    ASSERT_CONTAINS(monitorsSpec, R"("sdrSaturation": )");
}
