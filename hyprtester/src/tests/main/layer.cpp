#include "../../Log.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <format>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static bool spawnLayer(const std::string& namespace_, const std::vector<std::string>& args = {}) {
    NLog::log("{}Spawning kitty layer {}", Colors::YELLOW, namespace_);
    if (!Tests::spawnLayerKitty(namespace_, args)) {
        NLog::log("{}Error: {} layer did not spawn", Colors::RED, namespace_);
        return false;
    }
    return true;
}

TEST_CASE(plugin_layerrules) {

    EXPECT(spawnLayer("rule-layer"), true);

    OK(getFromSocket("/eval hl.plugin.test.add_layer_rule()"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/eval hl.layer_rule({ match = { namespace = 'rule-layer' }, plugin_rule = 'effect' })"));

    EXPECT(spawnLayer("rule-layer"), true);

    EXPECT(spawnLayer("norule-layer"), true);

    OK(getFromSocket("/eval hl.plugin.test.check_layer_rule()"));
}

TEST_CASE(layerPointerFocusPreservedOnKeyboardRefocus) {
    static constexpr const char* LAYER_NAMESPACE = "pointer-focus-layer";

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 0 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(!!Tests::spawnKitty("pointer_focus_ws1"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    ASSERT(!!Tests::spawnKitty("pointer_focus_ws2"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:pointer_focus_ws1' })"));

    ASSERT(spawnLayer(LAYER_NAMESPACE, {"--edge=top", "--layer=top", "--lines=48px", "--focus-policy=not-allowed"}), true);

    OK(getFromSocket(std::format("/eval hl.plugin.test.set_pointer_focus_layer('{}')", LAYER_NAMESPACE)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_pointer_focus_layer('{}')", LAYER_NAMESPACE)));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: pointer_focus_ws2\n");
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_pointer_focus_layer('{}')", LAYER_NAMESPACE)));
}
