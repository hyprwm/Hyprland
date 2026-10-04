#include "../../Log.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <chrono>
#include <format>
#include <string_view>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

static bool spawnLayer(const std::string& namespace_, const std::vector<std::string>& args = {}) {
    NLog::log("{}Spawning kitty layer {}", Colors::YELLOW, namespace_);
    if (!Tests::spawnLayerKitty(namespace_, args)) {
        NLog::log("{}Error: {} layer did not spawn", Colors::RED, namespace_);
        return false;
    }
    return true;
}

static std::string getLayerLine(const std::string& layers, const std::string& target) {

    auto pos = layers.find(std::format("namespace: {}", target));
    if (pos == std::string::npos)
        return "";

    auto start = layers.rfind('\n', pos);
    start      = (start == std::string::npos) ? 0 : start + 1;

    auto end = layers.find('\n', pos);

    return layers.substr(start, end - start);
}

// Poll only observations: refocusing or moving the pointer here would hide a broken restoration.
static std::string waitForFocusCheck(const std::string& check) {
    auto result = getFromSocket("/eval " + check);
    for (int i = 0; i < 50 && result != "ok"; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        result = getFromSocket("/eval " + check);
    }
    return result;
}

static std::string waitForWindowFocus(const std::string& cls) {
    return waitForFocusCheck(std::format("hl.plugin.test.check_keyboard_focus_window('{}'); hl.plugin.test.check_pointer_focus_window('{}')", cls, cls));
}

static std::string waitForLayerFocus(const std::string& namespace_) {
    return waitForFocusCheck(std::format("hl.plugin.test.check_keyboard_focus_layer('{}'); hl.plugin.test.check_pointer_focus_layer('{}')", namespace_, namespace_));
}

// The floating fixture is centered on (960, 540); the attempted motion stays on the monitor but leaves the window.
SUBTEST(ruleConfinementClampsMotion, const std::string& cls) {
    const auto WINDOW = std::format("local w = hl.get_window('class:{}'); assert(w); ", cls);
    OK(waitForFocusCheck(WINDOW + "assert(w.floating and w.at.x == 660 and w.at.y == 340 and w.size.x == 600 and w.size.y == 400)"));
    OK(waitForWindowFocus(cls));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 1400, y = 540 })"));
    OK(getFromSocket("/eval " + WINDOW + "local p = hl.get_cursor_pos(); assert(p.x >= w.at.x + w.size.x - 1 and p.x < w.at.x + w.size.x and p.y == 540)"));
    OK(waitForWindowFocus(cls));
    // Reset only during setup, never after the focus transition under test.
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
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
    SPAWN_KITTY("pointer_focus_ws1");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    SPAWN_KITTY("pointer_focus_ws2");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:pointer_focus_ws1' })"));

    ASSERT(spawnLayer(LAYER_NAMESPACE, {"--edge=top", "--layer=top", "--lines=48px", "--focus-policy=not-allowed"}), true);

    OK(getFromSocket(std::format("/eval hl.plugin.test.set_pointer_focus_layer('{}')", LAYER_NAMESPACE)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_pointer_focus_layer('{}')", LAYER_NAMESPACE)));
    OK(getFromSocket("/eval hl.plugin.test.check_keyboard_focus_window('pointer_focus_ws1')"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: pointer_focus_ws2\n");
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_pointer_focus_layer('{}')", LAYER_NAMESPACE)));
    OK(getFromSocket("/eval hl.plugin.test.check_keyboard_focus_window('pointer_focus_ws2')"));
}

TEST_CASE(exclusiveLayerTakesPointerFocusFromRuleConfinedWindow) {
    static constexpr const char* WINDOW_CLASS    = "layer_confined_window";
    static constexpr const char* LAYER_NAMESPACE = "exclusive-confined-layer";

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 0 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket(std::format("/eval hl.window_rule({{ match = {{ class = '^{}$' }}, confine_pointer = true }})", WINDOW_CLASS)));

    SPAWN_KITTY(WINDOW_CLASS);
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", WINDOW_CLASS)));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));

    const auto WINDOW_POINTER_CHECK = std::format("/eval hl.plugin.test.check_pointer_focus_window('{}')", WINDOW_CLASS);
    const auto LAYER_POINTER_CHECK  = std::format("/eval hl.plugin.test.check_pointer_focus_layer('{}')", LAYER_NAMESPACE);
    OK(getFromSocket(WINDOW_POINTER_CHECK));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));

    ASSERT(spawnLayer(LAYER_NAMESPACE,
                      {
                          "--edge=center",
                          "--layer=overlay",
                          "--focus-policy=exclusive",
                          "--output-name=HEADLESS-1",
                      }),
           true);

    // Registration can precede mapping. Wait without moving the cursor or explicitly setting pointer focus.
    auto layerFocus = getFromSocket(LAYER_POINTER_CHECK);
    for (int i = 0; i < 50 && layerFocus != "ok"; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        layerFocus = getFromSocket(LAYER_POINTER_CHECK);
    }
    OK(layerFocus);
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_layer('{}')", LAYER_NAMESPACE)));

    ASSERT(Tests::killAllLayers(), true);
    OK(getFromSocket(WINDOW_POINTER_CHECK));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));
}

// These exercise the confine_pointer rule, not a client's native pointer-constraints protocol.
TEST_CASE(ruleConfinedWindowFocusRestoredAfterEmptyWorkspace) {
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllWindows(), true);
        EXPECT_OK(getFromSocket("/reload"));
    });

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_empty_game$' }, float = true, size = {600, 400}, move = {660, 340}, confine_pointer = true })"));
    SPAWN_KITTY("confined_empty_game");
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
    CALL_SUBTEST(ruleConfinementClampsMotion, "confined_empty_game");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:confined_empty' })"));
    ASSERT_CONTAINS(getFromSocket("/activeworkspace"), "(confined_empty)");
    OK(waitForFocusCheck("hl.plugin.test.check_keyboard_focus_none(); hl.plugin.test.check_pointer_focus_none()"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(waitForWindowFocus("confined_empty_game"));
}

TEST_CASE(ruleConfinedWindowFocusRestoredAfterSpecialWorkspace) {
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllWindows(), true);
        EXPECT_OK(getFromSocket("/reload"));
    });

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    // Populate the special workspace before constraining the game, then reopen it while constrained.
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('confined_special')"));
    SPAWN_KITTY("confined_special_client");
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "(special:confined_special)");
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('confined_special')"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_special_game$' }, float = true, size = {600, 400}, move = {660, 340}, confine_pointer = true })"));
    SPAWN_KITTY("confined_special_game");
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
    CALL_SUBTEST(ruleConfinementClampsMotion, "confined_special_game");

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('confined_special')"));
    OK(waitForWindowFocus("confined_special_client"));
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('confined_special')"));
    OK(waitForWindowFocus("confined_special_game"));
}

TEST_CASE(ruleConfinedWindowFocusRestoredAfterCrossMonitorExclusiveLayer) {
    static constexpr const char* OUTPUT          = "HYPRTEST-CONFINED-LAYER";
    static constexpr const char* LAYER_NAMESPACE = "cross-monitor-confined-layer";

    ASSERT_NOT_CONTAINS(getFromSocket("/monitors all"), OUTPUT);
    bool        outputCreated = false;
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllLayers(), true);
        EXPECT(Tests::killAllWindows(), true);
        if (outputCreated)
            EXPECT_OK(getFromSocket(std::format("/output remove {}", OUTPUT)));
        EXPECT_OK(getFromSocket("/reload"));
    });

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 }, cursor = { no_warps = false } })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', disabled = false, mode = '1920x1080@60', position = '1920x0', scale = '1' }})", OUTPUT)));
    const auto CREATED = getFromSocket(std::format("/output create headless {}", OUTPUT));
    outputCreated      = CREATED == "ok";
    OK(CREATED);
    OK(waitForFocusCheck(std::format("local m = hl.get_monitor('{}'); assert(m and m.enabled and m.x == 1920 and m.width == 1920)", OUTPUT)));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:confined_layer_game' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_layer_game$' }, confine_pointer = true })"));
    SPAWN_KITTY("confined_layer_game");
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
    OK(waitForWindowFocus("confined_layer_game"));

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", OUTPUT)));
    ASSERT(spawnLayer(LAYER_NAMESPACE,
                      {
                          "--edge=center",
                          "--layer=overlay",
                          "--focus-policy=exclusive",
                          std::format("--output-name={}", OUTPUT),
                      }),
           true);
    OK(waitForLayerFocus(LAYER_NAMESPACE));

    // Return while the layer still owns the keyboard. Dismissal must restore both seat surfaces by itself.
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    ASSERT(getFromSocket("/repl hl.get_monitor('HEADLESS-2').focused"), "true");
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_layer('{}')", LAYER_NAMESPACE)));
    ASSERT(Tests::killAllLayers(), true);
    OK(waitForWindowFocus("confined_layer_game"));
}

TEST_CASE(ruleConfinementAllowsExplicitAndNewWindowFocusWithoutWarps) {
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllWindows(), true);
        EXPECT_OK(getFromSocket("/reload"));
    });

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 }, cursor = { no_warps = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_focus_a$' }, float = true, size = {600, 400}, move = {660, 340}, confine_pointer = true })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_focus_(existing|new)$' }, float = true, size = {300, 200}, move = {100, 100} })"));
    SPAWN_KITTY("confined_focus_existing");
    SPAWN_KITTY("confined_focus_a");

    for (const auto TARGET : {"confined_focus_existing", "confined_focus_new"}) {
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:confined_focus_a' })"));
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
        CALL_SUBTEST(ruleConfinementClampsMotion, "confined_focus_a");

        if (std::string_view(TARGET) == "confined_focus_existing")
            OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", TARGET)));
        else
            SPAWN_KITTY(TARGET);

        // Rule-only confinement must allow keyboard focus to leave A, even though the cursor stays over A.
        const auto CHECK = std::format("hl.plugin.test.check_keyboard_focus_window('{}'); local p = hl.get_cursor_pos(); assert(p.x == 960 and p.y == 540)", TARGET);
        OK(waitForFocusCheck(CHECK));
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        OK(getFromSocket("/eval " + CHECK));
    }
}

TEST_CASE(ruleConfinementRuntimeToggle) {
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllWindows(), true);
        EXPECT_OK(getFromSocket("/reload"));
    });

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = '^confined_runtime$' }, float = true, size = {600, 400}, move = {660, 340} })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'runtime-confinement', match = { class = '^confined_runtime$' }, confine_pointer = false })"));
    SPAWN_KITTY("confined_runtime");

    for (const bool ENABLED : {false, true, false, true}) {
        // Establish the shared keyboard/pointer domain before changing the rule on this mapped window.
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })"));
        OK(waitForWindowFocus("confined_runtime"));
        OK(getFromSocket(std::format("/eval hl.window_rule({{ name = 'runtime-confinement', confine_pointer = {} }})", ENABLED)));
        if (ENABLED)
            CALL_SUBTEST(ruleConfinementClampsMotion, "confined_runtime");
        else {
            OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 1400, y = 540 })"));
            ASSERT(getFromSocket("/cursorpos"), "1400, 540");
        }
    }
}

TEST_CASE(layerVisibilityOnFs) {

    const auto doMassacre = [&]() -> void {
        Tests::killAllLayers();
        Tests::waitUntilLayersN(0);
        Tests::killAllWindows();
        Tests::waitUntilWindowsN(0);
    };

    static constexpr const char* LAYER_NAMESPACE = "bar-like-layer";

    const auto                   spawnLayerAndWaitTillSuccess_TOP = [&]() {
        ASSERT(spawnLayer(LAYER_NAMESPACE, {"--edge=top", "--layer=top", "--lines=48px", "--focus-policy=not-allowed"}), true);
        Tests::waitUntilLayersN(1);
    };

    const auto spawnLayerAndWaitTillSuccess_OVERLAY = [&]() {
        ASSERT(spawnLayer(LAYER_NAMESPACE, {"--edge=top", "--layer=overlay", "--lines=48px", "--focus-policy=not-allowed"}), true);
        Tests::waitUntilLayersN(1);
    };

    // For default handled fullscreen

    // FS after a layer has been created
    spawnLayerAndWaitTillSuccess_TOP();

    SPAWN_KITTY("cat");

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1")
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));
    {

        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);

        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 0");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    // TOP Layer spawn after FS

    doMassacre();

    // allow_new_top_layers_over_existing_fullscreen = true
    OK(getFromSocket("/eval hl.config({ misc = { allow_new_top_layers_over_existing_fullscreen = true } })"));

    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_TOP();
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    doMassacre();
    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_TOP();
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    // allow_new_top_layers_over_existing_fullscreen = false

    doMassacre();

    OK(getFromSocket("/eval hl.config({ misc = { allow_new_top_layers_over_existing_fullscreen = false },})"));

    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_TOP();
    {

        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    doMassacre();
    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_TOP();
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 0");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    doMassacre();

    // Overlay is always ontop, spawn later or before FS

    // we need not test with the allow_new_top_layers_over_existing_fullscreen config opt as it's only relevant to TOP layer.

    // FS after a layer has been created
    spawnLayerAndWaitTillSuccess_OVERLAY();

    SPAWN_KITTY("cat");

    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));
    {

        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    // overlay Layer spawn after FS

    doMassacre();
    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_OVERLAY();
    {

        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }

    doMassacre();
    SPAWN_KITTY("cat");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:cat' })"));
    spawnLayerAndWaitTillSuccess_OVERLAY();
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset', window = 'class:cat' })"));
    {
        auto str = getLayerLine(getFromSocket("/layers"), LAYER_NAMESPACE);
        EXPECT_CONTAINS(str, "a: 1");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
    }
}

TEST_CASE(windowRefocusRestoresKeyboardFocusAfterSurfaceFocusCleared) {
    static constexpr const char* WINDOW_CLASS = "keyboard_refocus_target";

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY(WINDOW_CLASS);
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", WINDOW_CLASS)));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), std::format("class: {}\n", WINDOW_CLASS));
    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));

    OK(getFromSocket("/eval hl.plugin.test.clear_surface_focus()"));
    OK(getFromSocket(std::format("/eval hl.plugin.test.window_soft_focus('{}')", WINDOW_CLASS)));

    OK(getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", WINDOW_CLASS)));
}
