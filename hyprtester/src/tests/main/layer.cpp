#include "../../Log.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

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

TEST_CASE(plugin_layerrules) {

    ASSERT(spawnLayer("rule-layer"), true);

    OK(getFromSocket("/eval hl.plugin.test.add_layer_rule()"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/eval hl.layer_rule({ match = { namespace = 'rule-layer' }, plugin_rule = 'effect' })"));

    ASSERT(spawnLayer("rule-layer"), true);

    ASSERT(spawnLayer("norule-layer"), true);

    OK(getFromSocket("/eval hl.plugin.test.check_layer_rule()"));
}
