#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <cstdint>
#include <print>
#include <string>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <csignal>
#include <cerrno>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static std::string getCommandStdOut(std::string command) {
    CProcess process("bash", {"-c", command});
    process.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    process.runSync();

    const std::string& stdOut = process.stdOut();

    // Remove trailing new line
    return stdOut.substr(0, stdOut.length() - 1);
}

static bool testDevicesActiveLayoutIndex() {
    NLog::log("{}Testing hyprctl devices active_layout_index", Colors::GREEN);

    // configure layouts
    getFromSocket("/keyword input:kb_layout us,pl,ua");

    for (uint8_t i = 0; i < 3; i++) {
        // set layout
        getFromSocket("/switchxkblayout all " + std::to_string(i));
        std::string devicesJson = getFromSocket("j/devices");
        std::string expected    = R"("active_layout_index": )" + std::to_string(i);
        // check layout index
        EXPECT_CONTAINS(devicesJson, expected);
    }

    return true;
}

static bool testGetprop() {
    NLog::log("{}Testing hyprctl getprop", Colors::GREEN);
    if (!Tests::spawnKitty()) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    // animation
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation"), "(unset)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation -j"), R"({"animation": ""})");
    getFromSocket("/dispatch setprop class:kitty animation teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation"), "teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animation -j"), R"({"animation": "teststyle"})");

    // max_size
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size"), "inf inf");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size -j"), R"({"max_size": [null,null]})");
    getFromSocket("/dispatch setprop class:kitty max_size 200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size"), "200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty max_size -j"), R"({"max_size": [200,150]})");

    // min_size
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size"), "20 20");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size -j"), R"({"min_size": [20,20]})");
    getFromSocket("/dispatch setprop class:kitty min_size 100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size"), "100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty min_size -j"), R"({"min_size": [100,50]})");

    // opacity
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity -j"), R"({"opacity": 1})");
    getFromSocket("/dispatch setprop class:kitty opacity 0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity"), "0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity -j"), R"({"opacity": 0.3})");

    // opacity_inactive
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive -j"), R"({"opacity_inactive": 1})");
    getFromSocket("/dispatch setprop class:kitty opacity_inactive 0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive"), "0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive -j"), R"({"opacity_inactive": 0.5})");

    // opacity_fullscreen
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen -j"), R"({"opacity_fullscreen": 1})");
    getFromSocket("/dispatch setprop class:kitty opacity_fullscreen 0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen"), "0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen -j"), R"({"opacity_fullscreen": 0.75})");

    // opacity_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override -j"), R"({"opacity_override": false})");
    getFromSocket("/dispatch setprop class:kitty opacity_override true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_override -j"), R"({"opacity_override": true})");

    // opacity_inactive_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override -j"), R"({"opacity_inactive_override": false})");
    getFromSocket("/dispatch setprop class:kitty opacity_inactive_override true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_inactive_override -j"), R"({"opacity_inactive_override": true})");

    // opacity_fullscreen_override
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override -j"), R"({"opacity_fullscreen_override": false})");
    getFromSocket("/dispatch setprop class:kitty opacity_fullscreen_override true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty opacity_fullscreen_override -j"), R"({"opacity_fullscreen_override": true})");

    // active_border_color
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color"), "ee33ccff ee00ff99 45deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color -j"), R"({"active_border_color": "ee33ccff ee00ff99 45deg"})");
    getFromSocket("/dispatch setprop class:kitty active_border_color rgb(abcdef)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color"), "ffabcdef 0deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty active_border_color -j"), R"({"active_border_color": "ffabcdef 0deg"})");

    // bool window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input -j"), R"({"allows_input": false})");
    getFromSocket("/dispatch setprop class:kitty allows_input true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allows_input -j"), R"({"allows_input": true})");

    // int window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "10");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 10})");
    getFromSocket("/dispatch setprop class:kitty rounding 4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 4})");

    // float window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power"), "2");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power -j"), R"({"rounding_power": 2})");
    getFromSocket("/dispatch setprop class:kitty rounding_power 1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power"), "1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding_power -j"), R"({"rounding_power": 1.25})");

    // errors
    EXPECT(getCommandStdOut("hyprctl getprop"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:nonexistantclass animation"), "window not found");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty nonexistantprop"), "prop not found");

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return true;
}

static bool test() {
    NLog::log("{}Testing hyprctl", Colors::GREEN);

    {
        NLog::log("{}Testing hyprctl descriptions for any json errors", Colors::GREEN);
        CProcess jqProc("bash", {"-c", "hyprctl descriptions | jq"});
        jqProc.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
        jqProc.runSync();
        EXPECT(jqProc.exitCode(), 0);
    }

    testGetprop();
    testDevicesActiveLayoutIndex();
    getFromSocket("/reload");

    return !ret;
}

REGISTER_TEST_FN(test);
