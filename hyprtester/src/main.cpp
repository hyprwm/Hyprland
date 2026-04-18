
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
#include "tests/clients/tests.hpp"
#include "tests/plugin/plugin.hpp"

#include <filesystem>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/Casts.hpp>

#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <print>
#include <string_view>
#include <span>

#include "Log.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using Path = std::filesystem::path;

#define SP CSharedPointer

static int          ret = 0;
static SP<CProcess> hyprlandProc;

static bool         launchHyprland(Path configPath, Path binaryPath) {
    NLog::log("{}Launching Hyprland", Colors::YELLOW);
    hyprlandProc = makeShared<CProcess>(binaryPath, std::vector<std::string>{"--config", configPath});
    hyprlandProc->addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    NLog::log("{}Launched async process", Colors::YELLOW);

    return hyprlandProc->runAsync();
}

static bool hyprlandAlive() {
    NLog::log("{}hyprlandAlive", Colors::YELLOW);
    return kill(hyprlandProc->pid(), 0) == 0 || errno != ESRCH;
}

[[noreturn]] static void helpAndDie(int exit_code) {
    NLog::log("usage: hyprtester [arg [...]].\n");
    NLog::log(R"(Arguments:
    --help              -h       - Show this message again
    --config FILE       -c FILE  - Specify config file to use (default: './test.lua')
    --binary FILE       -b FILE  - Specify Hyprland binary to use (default: '../build/Hyprland')
    --plugin FILE       -p FILE  - Specify the location of the test plugin (default: './'))");

    std::exit(exit_code);
}

static Path validatePathOrDie(Path path) {
    Path canonicalized;
    try {
        canonicalized = std::filesystem::canonical(path);
        if (!std::filesystem::is_regular_file(canonicalized)) {
            throw std::exception();
        }
    } catch (...) {
        std::println(stderr, "[ ERROR ] File '{}' is not accessible or not a regular file", canonicalized.string());
        helpAndDie(EXIT_FAILURE);
    }
    return canonicalized;
}

namespace {
    struct SSettings {
        Path configPath;
        Path binaryPath;
        Path pluginPath;
    };
}

static SSettings parseSettings(const std::span<const char*> args) {
    static const auto cwd = std::filesystem::current_path();
    SSettings         settings{};

    for (auto it = args.begin(); it < args.end(); it++) {
        std::string_view value = *it;

        if (value == "--config" || value == "-c") {
            if (std::next(it) == args.end()) {
                helpAndDie(EXIT_FAILURE);
            }

            settings.configPath = validatePathOrDie(*std::next(it));
            it++;
        } else if (value == "--binary" || value == "-b") {
            if (std::next(it) == args.end()) {
                helpAndDie(EXIT_FAILURE);
            }

            settings.binaryPath = validatePathOrDie(*std::next(it));
            it++;
        } else if (value == "--plugin" || value == "-p") {
            if (std::next(it) == args.end()) {
                helpAndDie(EXIT_FAILURE);
            }

            settings.pluginPath = validatePathOrDie(*std::next(it));
            it++;
        } else if (value == "--help" || value == "-h") {
            helpAndDie(EXIT_SUCCESS);
        } else {
            std::println(stderr, "[ ERROR ] Unknown option '{}' !", *it);
            helpAndDie(EXIT_SUCCESS);
        }
    }

    // Default options
    if (settings.configPath.empty())
        settings.configPath = validatePathOrDie(cwd / "test.lua");
    if (settings.binaryPath.empty())
        settings.binaryPath = validatePathOrDie(cwd / "../build/Hyprland");
    if (settings.pluginPath.empty())
        settings.pluginPath = cwd;

    return settings;
}

static void runTests(std::vector<std::function<bool()>>& tests) {

    for (const auto& fn : tests) {
        EXPECT(fn(), true);
    }
}

int main(int argc, char** argv, char** envp) {

    std::span<const char*> args{const_cast<const char**>(argv + 1), sc<std::size_t>(argc - 1)};
    const SSettings        settings = parseSettings(args);

    NLog::log("{}launching hl", Colors::YELLOW);
    if (!launchHyprland(settings.configPath, settings.binaryPath)) {
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
    if (const auto R = getFromSocket(std::format("/plugin load {}", settings.pluginPath.string())); R != "ok") {
        NLog::log("{}Failed to load the test plugin: {}", Colors::RED, R);
        getFromSocket("/dispatch hl.dsp.exit()");
        return 1;
    }

    NLog::log("{}Loaded plugin", Colors::YELLOW);

    NLog::log("{}Running main tests", Colors::YELLOW);
    runTests(testFns);

    NLog::log("{}Running protocol client tests", Colors::YELLOW);
    runTests(clientTestFns);

    NLog::log("{}running plugin test", Colors::YELLOW);
    EXPECT(testPlugin(), true);

    NLog::log("{}running vkb test from plugin", Colors::YELLOW);
    EXPECT(testVkb(), true);

    // kill hyprland
    NLog::log("{}dispatching exit", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.exit()");

    NLog::log("\n{}Summary:\n\tPASSED: {}{}{}/{}\n\tFAILED: {}{}{}/{}\n{}", Colors::RESET, Colors::GREEN, TESTS_PASSED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, Colors::RED,
              TESTS_FAILED, Colors::RESET, TESTS_PASSED + TESTS_FAILED, (TESTS_FAILED > 0 ? std::string{Colors::RED} + "\nSome tests failed.\n" : ""));

    kill(hyprlandProc->pid(), SIGKILL);

    hyprlandProc.reset();

    return ret || TESTS_FAILED;
}
