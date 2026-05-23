#include "tests.hpp"

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../../Log.hpp"

TEST_CASE(plugin) {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.test()");

    if (RESPONSE != "ok")
        FAIL_TEST("{}Plugin tests failed, plugin returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
    else
        NLog::log("{}Test passed: plugin test", Colors::GREEN);
}

TEST_CASE(pluginVkb) {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.vkb()");

    if (RESPONSE != "ok")
        NLog::log("{}Vkb tests failed, tests returned:\n{}{}", Colors::RED, Colors::RESET, RESPONSE);
    else
        NLog::log("{}Test passed: vkb test from plugin", Colors::GREEN);
}
