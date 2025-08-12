#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <print>
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

static bool testGetprop() {
    NLog::log("{}Testing hyprctl getprop", Colors::GREEN);
    if (!Tests::spawnKitty()) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    // animationstyle
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animationstyle"), "(unset)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animationstyle -j"), R"({"animationstyle": ""})");
    getFromSocket("/dispatch setprop class:kitty animationstyle teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animationstyle"), "teststyle");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty animationstyle -j"), R"({"animationstyle": "teststyle"})");

    // maxsize
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty maxsize"), "inf inf");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty maxsize -j"), R"({"maxsize": [null,null]})");
    getFromSocket("/dispatch setprop class:kitty maxsize 200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty maxsize"), "200 150");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty maxsize -j"), R"({"maxsize": [200,150]})");

    // minsize
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty minsize"), "20 20");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty minsize -j"), R"({"minsize": [20,20]})");
    getFromSocket("/dispatch setprop class:kitty minsize 100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty minsize"), "100 50");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty minsize -j"), R"({"minsize": [100,50]})");

    // alpha
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alpha"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alpha -j"), R"({"alpha": 1})");
    getFromSocket("/dispatch setprop class:kitty alpha 0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alpha"), "0.3");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alpha -j"), R"({"alpha": 0.3})");

    // alphainactive
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactive"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactive -j"), R"({"alphainactive": 1})");
    getFromSocket("/dispatch setprop class:kitty alphainactive 0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactive"), "0.5");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactive -j"), R"({"alphainactive": 0.5})");

    // alphafullscreen
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreen"), "1");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreen -j"), R"({"alphafullscreen": 1})");
    getFromSocket("/dispatch setprop class:kitty alphafullscreen 0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreen"), "0.75");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreen -j"), R"({"alphafullscreen": 0.75})");

    // alphaoverride
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphaoverride"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphaoverride -j"), R"({"alphaoverride": false})");
    getFromSocket("/dispatch setprop class:kitty alphaoverride true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphaoverride"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphaoverride -j"), R"({"alphaoverride": true})");

    // alphainactiveoverride
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactiveoverride"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactiveoverride -j"), R"({"alphainactiveoverride": false})");
    getFromSocket("/dispatch setprop class:kitty alphainactiveoverride true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactiveoverride"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphainactiveoverride -j"), R"({"alphainactiveoverride": true})");

    // alphafullscreenoverride
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreenoverride"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreenoverride -j"), R"({"alphafullscreenoverride": false})");
    getFromSocket("/dispatch setprop class:kitty alphafullscreenoverride true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreenoverride"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty alphafullscreenoverride -j"), R"({"alphafullscreenoverride": true})");

    // activebordercolor
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty activebordercolor"), "ee33ccff ee00ff99 45deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty activebordercolor -j"), R"({"activebordercolor": "ee33ccff ee00ff99 45deg"})");
    getFromSocket("/dispatch setprop class:kitty activebordercolor rgb(abcdef)");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty activebordercolor"), "ffabcdef 0deg");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty activebordercolor -j"), R"({"activebordercolor": "ffabcdef 0deg"})");

    // bool window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allowsinput"), "false");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allowsinput -j"), R"({"allowsinput": false})");
    getFromSocket("/dispatch setprop class:kitty allowsinput true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allowsinput"), "true");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty allowsinput -j"), R"({"allowsinput": true})");

    // int window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "10");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 10})");
    getFromSocket("/dispatch setprop class:kitty rounding 4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding"), "4");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty rounding -j"), R"({"rounding": 4})");

    // float window properties
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty roundingpower"), "2");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty roundingpower -j"), R"({"roundingpower": 2})");
    getFromSocket("/dispatch setprop class:kitty roundingpower 1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty roundingpower"), "1.25");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty roundingpower -j"), R"({"roundingpower": 1.25})");

    // errors
    EXPECT(getCommandStdOut("hyprctl getprop"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:kitty"), "not enough args");
    EXPECT(getCommandStdOut("hyprctl getprop class:nonexistantclass animationstyle"), "window not found");
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

    if (!testGetprop()) {
        return false;
    }

    return !ret;
}

REGISTER_TEST_FN(test);
