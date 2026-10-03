#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <chrono>
#include <csignal>
#include <format>
#include <string_view>
#include <thread>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

static std::string waitForPopupProbe() {
    std::string result;
    for (size_t i = 0; i < 100; ++i) {
        result = getFromSocket("/eval hl.plugin.test.check_popup_opacity()");
        if (!result.contains("Popup opacity probe pending"))
            return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return result;
}

SUBTEST(popupRender, bool redirected) {
    CProcess client(std::format("{}/popup-render", binaryDir), {});
    client.addEnv("WAYLAND_DISPLAY", WLDISPLAY);
    ASSERT(client.runAsync(), true);
    CScopeGuard cleanup([&] {
        getFromSocket("/eval hl.plugin.test.reset_popup_opacity()");
        if (Tests::processAlive(client.pid()))
            kill(client.pid(), SIGKILL);
    });

    std::string ready;
    for (size_t i = 0; i < 100; ++i) {
        ASSERT(Tests::processAlive(client.pid()), true);
        ready = getFromSocket(std::format("/eval hl.plugin.test.arm_popup_opacity('popup-render', 0.5, false, {})", redirected ? "true" : "false"));
        if (ready == "ok")
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    OK(ready);
    EXPECT_OK(waitForPopupProbe()); // ALL: sibling fades must be 0.2 and 0.4.

    for (const auto* PRESENTATION :
         {"normal", "none", "custom", "isolated-windows-window-offset", "isolated-windows-workspace-offset", "isolated-shell-window-offset", "isolated-shell-workspace-offset"}) {
        for (const bool POPUP_ONLY : {false, true}) {
            for (const float FADE : {0.5F, 0.F}) {
                if (FADE == 0.F && !std::string_view{PRESENTATION}.starts_with("isolated-"))
                    continue;
                // Zero selects zero workspace alpha with a unit window fade, as in the original opacity probe.
                NLog::log("Checking workspace presentation: {}, mode {}, redirected {}, zero workspace alpha {}", PRESENTATION, POPUP_ONLY ? "POPUP" : "ALL", redirected,
                          FADE == 0.F);
                OK(getFromSocket(std::format("/eval hl.plugin.test.arm_popup_opacity('popup-render', {}, {}, {}, '{}')", FADE, POPUP_ONLY ? "true" : "false",
                                             redirected ? "true" : "false", PRESENTATION)));
                EXPECT_OK(waitForPopupProbe());
            }
        }
    }

    if (redirected) {
        OK(getFromSocket("/eval hl.plugin.test.arm_popup_opacity('popup-render', 0.5, true, true)"));
        EXPECT_OK(waitForPopupProbe()); // POPUP: both siblings, no toplevel or root leakage.
        return;
    }

    for (const bool POPUP_ONLY : {false, true}) {
        for (const float FADE : {0.5F, 0.1F, 1.F, 0.F}) {
            if (!POPUP_ONLY && FADE == 0.5F)
                continue;
            NLog::log("Checking popup opacity: mode {}, parent fade {}", POPUP_ONLY ? "POPUP" : "ALL", FADE);
            OK(getFromSocket(std::format("/eval hl.plugin.test.arm_popup_opacity('popup-render', {}, {})", FADE, POPUP_ONLY ? "true" : "false")));
            EXPECT_OK(waitForPopupProbe());
        }
    }
}

TEST_CASE(popupOpacityInheritsParentFade) {
    CALL_SUBTEST(popupRender, false);
}

TEST_CASE(popupRespectsPassRedirection) {
    CALL_SUBTEST(popupRender, true);
}
