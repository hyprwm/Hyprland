
// This is a tester for Hyprland. It will launch the built binary in ./build/Hyprland
// in headless mode and test various things.
// for now it's quite basic and limited, but will be expanded in the future.

// NOTE: This tester has to be ran from its directory!!

// Some TODO:
// - Add a plugin built alongside so that we can do more detailed tests (e.g. simulating keystrokes)
// - test coverage
// - maybe figure out a way to do some visual tests too?

// Required runtime deps for checks:
// - kitty
// - xeyes

#include "shared.hpp"
#include "hyprctlCompat.hpp"
#include "tests/window/window.hpp"

#include <filesystem>
#include <hyprutils/os/Process.hpp>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <print>

using namespace Hyprutils::OS;

static int                     ret = 0;
static Hyprutils::OS::CProcess hyprlandProc("", {});
static const std::string       cwd = std::filesystem::current_path().string();

//
static bool launchHyprland() {
    std::error_code ec;
    if (!std::filesystem::exists(cwd + "/../build/Hyprland", ec) || ec) {
        std::println("{}No Hyprland binary", Colors::RED);
        return false;
    }

    hyprlandProc = CProcess{cwd + "/../build/Hyprland", {"--config", cwd + "/test.conf"}};
    hyprlandProc.addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    return hyprlandProc.runAsync();
}

static bool hyprlandAlive() {
    kill(hyprlandProc.pid(), 0);
    return errno != ESRCH;
}

int main(int argc, char** argv, char** envp) {

    if (!launchHyprland())
        return 1;

    // hyprland has launched, let's check if it's alive after 2s
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (!hyprlandAlive()) {
        std::println("{}Hyprland failed to launch", Colors::RED);
        return 1;
    }

    // wonderful, we are in. Let's get the instance signature.
    const auto INSTANCES = instances();
    if (INSTANCES.empty()) {
        std::println("{}Hyprland failed to launch (2)", Colors::RED);
        return 1;
    }

    HIS       = INSTANCES.back().id;
    WLDISPLAY = INSTANCES.back().wlSocket;

    getFromSocket("/output create headless");

    // now we can start issuing stuff.
    EXPECT(testWindows(), true);

    // kill hyprland
    getFromSocket("/dispatch exit");

    std::println("\n{}Summary:\n\tPASSED: {}{}{}/{}\n\tFAILED: {}{}{}/{}\n{}", Colors::RESET, Colors::GREEN, TESTS_PASSED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, Colors::RED,
                 TESTS_FAILED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, (TESTS_FAILED > 0 ? std::string{Colors::RED} + "\nSome tests failed.\n" : ""));

    return ret;
}