#include "tests.hpp"

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../../Log.hpp"

TEST_CASE(plugin) {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.test()");

    if (RESPONSE != "ok")
        FAIL_TEST("Plugin tests failed, plugin returned:\n{}", RESPONSE);
}

TEST_CASE(pluginVkb) {
    const auto RESPONSE = getFromSocket("/eval hl.plugin.test.vkb()");

    if (RESPONSE != "ok")
        FAIL_TEST("Vkb tests failed, tests returned:\n{}", RESPONSE);
}
