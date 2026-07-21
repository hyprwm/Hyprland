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

static std::string getLayerLine(const std::string& layers, const std::string& target) {

    auto pos = layers.find("namespace: " + target);
    if (pos == std::string::npos)
        return "";

    auto start = layers.rfind('\n', pos);
    start      = (start == std::string::npos) ? 0 : start + 1;

    auto end = layers.find('\n', pos);

    return layers.substr(start, end - start);
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

TEST_CASE(layerVisibilityOnFs) {

    // For default handled fullscreen

    static constexpr const char* LAYER_NAMESPACE = "bar-like-layer";

    ASSERT(spawnLayer(LAYER_NAMESPACE, {"--edge=top", "--layer=top", "--lines=48px", "--focus-policy=not-allowed"}), true);

    Tests::spawnKitty("cat");

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));

    {

        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 0")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }
}

TEST_CASE(windowRefocusRestoresKeyboardFocusAfterSurfaceFocusCleared) {
    static constexpr const char* WINDOW_CLASS = "keyboard_refocus_target";

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(!!Tests::spawnKitty(WINDOW_CLASS), true);
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", WINDOW_CLASS)));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), std::format("class: {}\n", WINDOW_CLASS));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));

    OK(getFromSocket("/eval hl.plugin.test.clear_surface_focus()"));
    OK(getFromSocket(std::format("/eval hl.plugin.test.window_soft_focus('{}')", WINDOW_CLASS)));

    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));
}
