#include "../../Log.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static bool spawnLayer(const std::string& namespace_) {
    NLog::log("{}Spawning kitty layer {}", Colors::YELLOW, namespace_);
    if (!Tests::spawnLayerKitty(namespace_)) {
        NLog::log("{}Error: {} layer did not spawn", Colors::RED, namespace_);
        return false;
    }
    return true;
}

static bool test() {
    NLog::log("{}Testing plugin layerrules", Colors::GREEN);

    if (!spawnLayer("rule-layer"))
        return false;

    OK(getFromSocket("/dispatch plugin:test:add_layer_rule"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/keyword layerrule match:namespace rule-layer, plugin_rule effect"));

    if (!spawnLayer("rule-layer"))
        return false;

    if (!spawnLayer("norule-layer"))
        return false;

    OK(getFromSocket("/dispatch plugin:test:check_layer_rule"));

    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all layers", Colors::YELLOW);
    Tests::killAllLayers();

    NLog::log("{}Expecting 0 layers", Colors::YELLOW);
    EXPECT(Tests::layerCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
