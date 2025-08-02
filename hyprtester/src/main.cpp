
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
#include "tests/main/tests.hpp"
#include "tests/plugin/plugin.hpp"

#include <filesystem>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <print>

#include "Log.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

static int               ret = 0;
static SP<CProcess>      hyprlandProc;
static const std::string cwd = std::filesystem::current_path().string();

//
static bool launchHyprland(std::string configPath, std::string binaryPath) {
    if (binaryPath == "") {
        std::error_code ec;
        if (!std::filesystem::exists(cwd + "/../build/Hyprland", ec) || ec) {
            NLog::log("{}No Hyprland binary", Colors::RED);
            return false;
        }

        binaryPath = cwd + "/../build/Hyprland";
    }

    if (configPath == "") {
        std::error_code ec;
        if (!std::filesystem::exists(cwd + "/test.conf", ec) || ec) {
            NLog::log("{}No test config", Colors::RED);
            return false;
        }

        configPath = cwd + "/test.conf";
    }

    NLog::log("{}Launching Hyprland", Colors::YELLOW);
    hyprlandProc = makeShared<CProcess>(binaryPath, std::vector<std::string>{"--config", configPath});
    hyprlandProc->addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    NLog::log("{}Launched async process", Colors::YELLOW);

    return hyprlandProc->runAsync();
}

static bool hyprlandAlive() {
    NLog::log("{}hyprlandAlive", Colors::YELLOW);
    kill(hyprlandProc->pid(), 0);
    return errno != ESRCH;
}

static void help() {
    NLog::log("usage: hyprtester [arg [...]].\n");
    NLog::log(R"(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use
    --binary FILE       -b FILE  - Specify Hyprland binary to use
    --plugin FILE       -p FILE  - Specify the location of the test plugin)");
}

int main(int argc, char** argv, char** envp) {

    std::string              configPath = "";
    std::string              binaryPath = "";
    std::string              pluginPath = std::filesystem::current_path().string();

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
        } else if (*it == "--plugin" || *it == "-p") {
            if (std::next(it) == args.end()) {
                help();

                return 1;
            }

            pluginPath = *std::next(it);

            try {
                pluginPath = std::filesystem::canonical(pluginPath);

                if (!std::filesystem::is_regular_file(pluginPath)) {
                    throw std::exception();
                }
            } catch (...) {
                std::println(stderr, "[ ERROR ] plugin '{}' doesn't exist!", pluginPath);
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

    NLog::log("{}launching hl", Colors::YELLOW);
    if (!launchHyprland(configPath, binaryPath)) {
        NLog::log("{}well it failed", Colors::RED);
        return 1;
    }

    // hyprland has launched, let's check if it's alive after 10s
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    NLog::log("{}slept for 10s", Colors::YELLOW);
    if (!hyprlandAlive()) {
        NLog::log("{}Hyprland failed to launch", Colors::RED);
        return 1;
    }

    // wonderful, we are in. Let's get the instance signature.
    NLog::log("{}trying to get INSTANCES", Colors::YELLOW);
    const auto INSTANCES = instances();
    if (INSTANCES.empty()) {
        NLog::log("{}Hyprland failed to launch (2)", Colors::RED);
        return 1;
    }

    HIS       = INSTANCES.back().id;
    WLDISPLAY = INSTANCES.back().wlSocket;

    NLog::log("{}trying to get create headless output", Colors::YELLOW);
    getFromSocket("/output create headless");

    NLog::log("{}trying to load plugin", Colors::YELLOW);
    if (const auto R = getFromSocket(std::format("/plugin load {}", pluginPath)); R != "ok") {
        NLog::log("{}Failed to load the test plugin: {}", Colors::RED, R);
        getFromSocket("/dispatch exit 1");
        return 1;
    }

    NLog::log("{}Loaded plugin", Colors::YELLOW);

    for (const auto& fn : testFns) {
        EXPECT(fn(), true);
    }

    NLog::log("{}running plugin test", Colors::YELLOW);
    EXPECT(testPlugin(), true);

    NLog::log("{}running vkb test from plugin", Colors::YELLOW);
    EXPECT(testVkb(), true);

    // kill hyprland
    NLog::log("{}dispatching exit", Colors::YELLOW);
    getFromSocket("/dispatch exit");

    NLog::log("\n{}Summary:\n\tPASSED: {}{}{}/{}\n\tFAILED: {}{}{}/{}\n{}", Colors::RESET, Colors::GREEN, TESTS_PASSED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, Colors::RED,
              TESTS_FAILED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, (TESTS_FAILED > 0 ? std::string{Colors::RED} + "\nSome tests failed.\n" : ""));

    kill(hyprlandProc->pid(), SIGKILL);

    hyprlandProc.reset();

    return ret || TESTS_FAILED;
}
