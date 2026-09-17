#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <cstdint>
#include <string>
#include <format>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

#define UP CUniquePointer
#define SP CSharedPointer

static constexpr auto CONFIGURED_DESCRIPTION_VALUES = R"(
    def matches($name; $default; $current):
        ([.[] | select(.name == $name)] |
            length == 1 and
            .[0].default == $default and
            .[0].current == $current);

    matches("general:border_size"; 1; 2) and
    matches("general:snap:enabled"; false; true) and
    matches("decoration:rounding"; 0; 10) and
    matches("master:new_status"; "slave"; "master") and
    matches("scrolling:follow_min_visible"; 0.4; 1)
)";

static std::string    getCommandStdOut(std::string command) {
    CProcess process("bash", {"-c", command});
    process.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    process.runSync();

    const std::string& stdOut = process.stdOut();

    // Remove trailing new line
    return stdOut.substr(0, stdOut.length() - 1);
}

static bool descriptionsMatch(const std::string& filter) {
    CProcess jqProc("bash", {"-c", std::format("hyprctl descriptions | jq -e '{}'", filter)});
    jqProc.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    jqProc.runSync();
    return jqProc.exitCode() == 0;
}

static bool hyprctlJsonMatches(const std::string& command, const std::string& filter) {
    CProcess jqProc("bash", {"-c", std::format("hyprctl -j {} | jq -e '{}'", command, filter)});
    jqProc.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    jqProc.runSync();
    return jqProc.exitCode() == 0;
}

static void setWindowProp(const std::string& selector, const std::string& prop, const std::string& value) {
    getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = '{}', prop = '{}', value = '{}' }})", selector, prop, value));
}

TEST_CASE(hyprctlDevicesActiveLayoutIndex) {
    // configure layouts
    OK(getFromSocket("r/eval hl.config({ input = { kb_layout = \"us,pl,ua\" } })"));

    for (uint8_t i = 0; i < 3; i++) {
        // set layout
        getFromSocket(std::format("/switchxkblayout all {}", i));
        std::string devicesJson = getFromSocket("j/devices");
        std::string expected    = std::format(R"("active_layout_index": {})", i);
        // check layout index
        EXPECT_CONTAINS(devicesJson, expected);
    }
}

TEST_CASE(hyprctlGetprop) {
    SPAWN_KITTY("kitty");

    // animation
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation"), "(unset)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation -j"), R"({"animation": ""})");
    setWindowProp("class:kitty", "animation", "teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation"), "teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation -j"), R"({"animation": "teststyle"})");

    // max_size
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size"), "inf inf");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size -j"), R"({"max_size": [null,null]})");
    setWindowProp("class:kitty", "max_size", "200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size"), "200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size -j"), R"({"max_size": [200,150]})");

    // min_size
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size"), "20 20");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size -j"), R"({"min_size": [20,20]})");
    setWindowProp("class:kitty", "min_size", "100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size"), "100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size -j"), R"({"min_size": [100,50]})");

    // expr-based min/max _size
    getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:kitty' })"); // need to set floating for tests below
    setWindowProp("class:kitty", "max_size", "90+10 25*2");                                     // set max to the same as min above, forcing window to 100*50
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size"), "100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size -j"), R"({"max_size": [100,50]})");
    setWindowProp("class:kitty", "min_size", "window_w*0.5 window_h-10");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size"), "50 40");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size -j"), R"({"min_size": [50,40]})");
    getFromSocket("/dispatch hl.dsp.window.float({ action = 'unset', window = 'class:kitty' })"); // go back to tiled for consistency

    // opacity
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity -j"), R"({"opacity": 1})");
    setWindowProp("class:kitty", "opacity", "0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity"), "0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity -j"), R"({"opacity": 0.3})");

    // opacity_inactive
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive -j"), R"({"opacity_inactive": 1})");
    setWindowProp("class:kitty", "opacity_inactive", "0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive"), "0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive -j"), R"({"opacity_inactive": 0.5})");

    // opacity_fullscreen
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen -j"), R"({"opacity_fullscreen": 1})");
    setWindowProp("class:kitty", "opacity_fullscreen", "0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen"), "0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen -j"), R"({"opacity_fullscreen": 0.75})");

    // opacity_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override -j"), R"({"opacity_override": false})");
    setWindowProp("class:kitty", "opacity_override", "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override -j"), R"({"opacity_override": true})");

    // opacity_inactive_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override -j"), R"({"opacity_inactive_override": false})");
    setWindowProp("class:kitty", "opacity_inactive_override", "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override -j"), R"({"opacity_inactive_override": true})");

    // opacity_fullscreen_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override -j"), R"({"opacity_fullscreen_override": false})");
    setWindowProp("class:kitty", "opacity_fullscreen_override", "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override -j"), R"({"opacity_fullscreen_override": true})");

    // active_border_color
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color"), "ee33ccff ee00ff99 45deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color -j"), R"({"active_border_color": "ee33ccff ee00ff99 45deg"})");
    setWindowProp("class:kitty", "active_border_color", "rgb(abcdef)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color"), "ffabcdef 0deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color -j"), R"({"active_border_color": "ffabcdef 0deg"})");

    // bool window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input -j"), R"({"allows_input": false})");
    setWindowProp("class:kitty", "allows_input", "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input -j"), R"({"allows_input": true})");

    // int window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "10");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 10})");
    setWindowProp("class:kitty", "rounding", "4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 4})");

    // float window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power"), "2");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power -j"), R"({"rounding_power": 2})");
    setWindowProp("class:kitty", "rounding_power", "1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power"), "1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power -j"), R"({"rounding_power": 1.25})");

    // errors
    EXPECT(getCommandStdOut("hyprctl getprop"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:nonexistantclass animation"), "window not found");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty nonexistantprop"), "prop not found");
}

TEST_CASE(hyprctlSubmap) {
    EXPECT(getCommandStdOut("hyprctl submap"), "default\n");
    EXPECT(getCommandStdOut("hyprctl submap -j | jq -r \".\""), "default");
}

TEST_CASE(hyprctlJsonErrors) {
    EXPECT(descriptionsMatch(R"(type == "array")"), true);
}

TEST_CASE(hyprctlDescriptionsCurrentValues) {
    EXPECT(descriptionsMatch(CONFIGURED_DESCRIPTION_VALUES), true);

    OK(getFromSocket(
        "/eval hl.config({ general = { border_size = 7, snap = { enabled = false } }, master = { new_status = 'inherit' }, scrolling = { follow_min_visible = 0.625 } })"));

    EXPECT(descriptionsMatch(R"(
        def matches($name; $default; $current):
            ([.[] | select(.name == $name)] |
                length == 1 and
                .[0].default == $default and
                .[0].current == $current);

        matches("general:border_size"; 1; 7) and
        matches("general:snap:enabled"; false; false) and
        matches("master:new_status"; "slave"; "inherit") and
        matches("scrolling:follow_min_visible"; 0.4; 0.625)
    )"),
           true);

    OK(getFromSocket("/reload"));
    EXPECT(descriptionsMatch(CONFIGURED_DESCRIPTION_VALUES), true);
}

TEST_CASE(hyprctlBindsJson) {
    EXPECT(getFromSocket("/eval hl.bind('SUPER + F12', hl.dsp.exec_cmd('true'), { description = 'hyprctl binds JSON regression', locked = true, repeating = true, "
                         "allow_input_capture = false })"),
           "ok");

    CProcess jqProc("bash", {"-c", R"(hyprctl -j binds | jq -e '
        type == "array" and
        ([.[] | select(.key == "F12")] | length == 1) and
        any(.[];
            .key == "F12" and
            .has_description == true and
            (.description | type) == "string" and
            .description == "hyprctl binds JSON regression" and
            (.allow_input_capture | type) == "boolean" and
            .allow_input_capture == false
        )
    ')"});
    jqProc.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    jqProc.runSync();
    EXPECT(jqProc.exitCode(), 0);

    const auto BINDS    = getFromSocket("/binds");
    const auto DESC_POS = BINDS.find("description: hyprctl binds JSON regression");
    EXPECT(DESC_POS != std::string::npos, true);
    if (DESC_POS != std::string::npos) {
        const auto BIND_POS = BINDS.rfind("bind\n", DESC_POS);
        EXPECT(BIND_POS != std::string::npos, true);
        if (BIND_POS != std::string::npos) {
            const auto BLOCK = BINDS.substr(BIND_POS, DESC_POS - BIND_POS);
            EXPECT(BLOCK.contains("\tflags: locked, repeat\n"), true);
        }
    }

    EXPECT(getFromSocket("/eval hl.unbind('SUPER + F12')"), "ok");
}

TEST_CASE(hyprctlWorkspaceJson) {
    static constexpr auto WORKSPACE_ID      = 4201;
    static constexpr auto WORKSPACE_NAME    = "hyprctl_workspace_json";
    static constexpr auto SPECIAL_WORKSPACE = "hyprctl_workspace_json_special";

    CScopeGuard           guard = {[&]() {
        if (getFromSocket("/monitors").contains(std::format("(special:{})", SPECIAL_WORKSPACE)))
            getFromSocket(std::format("/dispatch hl.dsp.workspace.toggle_special('{}')", SPECIAL_WORKSPACE));
        getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");
    }};

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ workspace = '{}' }})", WORKSPACE_ID)));
    OK(getFromSocket(std::format("/dispatch hl.dsp.workspace.rename({{ workspace = '{}', name = '{}' }})", WORKSPACE_ID, WORKSPACE_NAME)));
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ workspace = 'special:{}' }})", SPECIAL_WORKSPACE)));

    EXPECT(hyprctlJsonMatches("monitors",
                              std::format(R"(
        type == "array" and
        any(.[];
            .name == "HEADLESS-2" and
            (.activeWorkspace |
                (.id | type) == "number" and
                .id == {} and
                (.name | type) == "string" and
                .name == "{}" and
                (.type | type) == "string" and
                .type == "normal"
            ) and
            (.specialWorkspace |
                has("id") == false and
                (.name | type) == "string" and
                .name == "special:{}" and
                (.type | type) == "string" and
                .type == "special"
            )
        )
    )",
                                          WORKSPACE_ID, WORKSPACE_NAME, SPECIAL_WORKSPACE)),
           true);

    EXPECT(hyprctlJsonMatches("workspaces",
                              std::format(R"(
        type == "array" and
        any(.[];
            (.id | type) == "number" and
            .id == {} and
            (.name | type) == "string" and
            .name == "{}" and
            (.type | type) == "string" and
            .type == "normal"
        ) and
        any(.[];
            has("id") == false and
            (.name | type) == "string" and
            .name == "special:{}" and
            (.type | type) == "string" and
            .type == "special"
        )
    )",
                                          WORKSPACE_ID, WORKSPACE_NAME, SPECIAL_WORKSPACE)),
           true);
}

TEST_CASE(hyprctlREPL) {
    EXPECT(getCommandStdOut("hyprctl repl 'print(type(hl))'"), "table");
    EXPECT(getCommandStdOut("hyprctl eval 'print(type(hl))'"), "ok");
}

TEST_CASE(hyprctlBatch) {
    const auto command  = R"([[BATCH]] activewindow; repl local i = 42\; print(i, "hello\\nworld ]"); clients)";
    const auto expected = R"(Invalid


42	hello
world ]


no open windows)";
    EXPECT(getFromSocket(command), expected);
}
