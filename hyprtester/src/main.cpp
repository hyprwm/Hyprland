
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
#include "tests/window/groups.hpp"
#include "tests/plugin/plugin.hpp"

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
static bool launchHyprland(std::string configPath, std::string binaryPath) {
    if (binaryPath == "") {
        std::error_code ec;
        if (!std::filesystem::exists(cwd + "/../build/Hyprland", ec) || ec) {
            std::println("{}No Hyprland binary", Colors::RED);
            return false;
        }

        binaryPath = cwd + "/../build/Hyprland";
    }

    if (configPath == "") {
        std::error_code ec;
        if (!std::filesystem::exists(cwd + "/test.conf", ec) || ec) {
            std::println("{}No test config", Colors::RED);
            return false;
        }

        configPath = cwd + "/test.conf";
    }

    std::println("{}Launching Hyprland", Colors::YELLOW);
    hyprlandProc = CProcess{binaryPath, {"--config", configPath, "--i-am-really-stupid"}};
    hyprlandProc.addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    return hyprlandProc.runAsync();
    std::println("{}Launched async process", Colors::YELLOW);
}

static bool hyprlandAlive() {
    std::println("{}hyprlandAlive", Colors::YELLOW);
    kill(hyprlandProc.pid(), 0);
    return errno != ESRCH;
}

static void help() {
    std::println("usage: hyprtester [arg [...]].\n");
    std::println(R"(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use
    --binary FILE       -b FILE  - Specify Hyprland binary to use)");
}

int main(int argc, char** argv, char** envp) {

    std::string              configPath = "";
    std::string              binaryPath = "";

    std::vector<std::string> args{argv + 1, argv + argc};

    for (auto it = args.begin(); it != args.end(); it++) {
        if (*it == "--config" || *it == "-c") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            configPath = *std::next(it);

            try {
                configPath = std::filesystem::canonical(configPath);

                if (!std::filesystem::is_regular_file(configPath)) {
                    throw std::exception();
                }
            } catch (...) {
                std::println(stderr, "[ ERROR ] Config file '{}' doesn't exist!", configPath);
                help();

                return 1;
            }

            it++;

            continue;
        } else if (*it == "--binary" || *it == "-b") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            binaryPath = *std::next(it);

            try {
                binaryPath = std::filesystem::canonical(binaryPath);

                if (!std::filesystem::is_regular_file(binaryPath)) {
                    throw std::exception();
                }
            } catch (...) {
                std::println(stderr, "[ ERROR ] Binary '{}' doesn't exist!", binaryPath);
                help();

                return 1;
            }

            it++;

            continue;
        } else if (*it == "--help" || *it == "-h") {
            help();

            return 0;
        } else {
            std::println(stderr, "[ ERROR ] Unknown option '{}' !", *it);
            help();

            return 1;
        }
    }

    if (!launchHyprland(configPath, binaryPath))
        return 1;

    // hyprland has launched, let's check if it's alive after 2s
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::println("{}slept for 2s", Colors::YELLOW);
    if (!hyprlandAlive()) {
        std::println("{}Hyprland failed to launch", Colors::RED);
        return 1;
    }

    // wonderful, we are in. Let's get the instance signature.
    std::println("{}trying to get INSTANCES", Colors::YELLOW);
    const auto INSTANCES = instances();
    if (INSTANCES.empty()) {
        std::println("{}Hyprland failed to launch (2)", Colors::RED);
        return 1;
    }

    HIS       = INSTANCES.back().id;
    WLDISPLAY = INSTANCES.back().wlSocket;

    std::println("{}trying to get create headless output", Colors::YELLOW);
    getFromSocket("/output create headless");

    std::println("{}trying to load plugin", Colors::YELLOW);
    if (getFromSocket(std::format("/plugin load {}/plugin/hyprtestplugin.so", std::filesystem::current_path().string())) != "ok") {
        std::println("{}Failed to load the test plugin", Colors::RED);
        return 1;
    }

    std::println("{}Loaded plugin", Colors::YELLOW);

    // now we can start issuing stuff.
    std::println("{}testing windows", Colors::YELLOW);
    EXPECT(testWindows(), true);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::println("{}testing groups", Colors::YELLOW);
    EXPECT(testGroups(), true);

    std::println("{}running plugin test", Colors::YELLOW);
    EXPECT(testPlugin(), true);

    // kill hyprland
    std::println("{}dispatching exit", Colors::YELLOW);
    getFromSocket("/dispatch exit");

    std::println("\n{}Summary:\n\tPASSED: {}{}{}/{}\n\tFAILED: {}{}{}/{}\n{}", Colors::RESET, Colors::GREEN, TESTS_PASSED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, Colors::RED,
                 TESTS_FAILED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, (TESTS_FAILED > 0 ? std::string{Colors::RED} + "\nSome tests failed.\n" : ""));

    return ret || TESTS_FAILED;
}
