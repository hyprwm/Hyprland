
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

#define INCLUDED_FROM_MAIN 1 // Prevent macro redefinition warnings from includes of "tests/*/tests.hpp"

#include "shared.hpp"
#include "hyprctlCompat.hpp"
#include "tests/main/tests.hpp"
#include "tests/clients/tests.hpp"
#include "tests/misc/tests.hpp"
#include "tests/shared.hpp"

#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/Casts.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <span>
#include <thread>
#include <vector>

#include "Log.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using Path = std::filesystem::path;

#define SP CSharedPointer

namespace {
    struct SSettings {
        Path                     configPath;
        Path                     binaryPath;
        Path                     pluginPath;
        std::vector<std::string> requestedTests;
    };

    struct STestsRunResult {
        unsigned long long       total;
        std::vector<std::string> failedNames;
    };
}

static SP<CProcess> hyprlandProc;

static bool         launchHyprland(Path configPath, Path binaryPath) {
    NLog::yellow("Launching Hyprland");
    hyprlandProc = makeShared<CProcess>(binaryPath, std::vector<std::string>{"--config", configPath});
    hyprlandProc->addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    NLog::yellow("Launched async process");

    return hyprlandProc->runAsync();
}

static bool hyprlandAlive() {
    NLog::yellow("hyprlandAlive");
    return kill(hyprlandProc->pid(), 0) == 0 || errno != ESRCH;
}

[[noreturn]] static void helpAndDie(int exit_code) {
    NLog::log("usage: hyprtester [--OPTION [VALUE]]... [TEST_NAMES].\n");
    NLog::log(R"(Arguments:
    --help              -h         - Show this message again
    --config FILE       -c FILE    - Specify config file to use (default: './test.lua')
    --binary FILE       -b FILE    - Specify Hyprland binary to use (default: '../build/Hyprland')
    --plugin FILE       -p FILE    - Specify the location of the test plugin (default: './')
    [TEST_NAMES]                   - Specify list of tests to run (separated by spaces).
                                     If omitted, all tests will run.)");

    std::exit(exit_code);
}

static Path validatePathOrDie(Path path) {
    try {
        if (!std::filesystem::is_regular_file(path)) {
            throw std::exception();
        }
    } catch (...) {
        std::println(stderr, "[ ERROR ] File '{}' is not accessible or not a regular file", path.string());
        helpAndDie(EXIT_FAILURE);
    }
    return path;
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
        } else if (!value.starts_with("-")) {
            settings.requestedTests.emplace_back(value);
        } else {
            std::println(stderr, "[ ERROR ] Unknown option '{}' !", *it);
            helpAndDie(EXIT_FAILURE);
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

static bool preTestCleanup() {
    bool failed = false;

    if (!Tests::killAllWindows()) {
        NLog::red("Internal failure: failed to kill all windows");
        failed = true;
    }
    if (!Tests::killAllLayers()) {
        NLog::red("Internal failure: failed to kill all layers");
        failed = true;
    }
    if (getFromSocket("/reload") != "ok") {
        NLog::red("Internal failure: failed to reload");
        failed = true;
    }
    if (!getFromSocket("/activeworkspace").contains("workspace ID 1 (1)")) {
        if (getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })") != "ok") {
            NLog::red("Internal failure: failed to switch to workspace 1");
            failed = true;
        }
    }
    if (getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })") != "ok") {
        NLog::red("Internal failure: failed to reset cursor position");
        failed = true;
    }

    return !failed;
}

static STestsRunResult runTests(std::vector<std::shared_ptr<CTestCase>>& testCases) {
    struct STestsRunResult res{.total = testCases.size(), .failedNames = {}};

    for (auto& tc : testCases) {
        // Clean up before every test
        NLog::yellow("Cleaning up");

        if (!preTestCleanup()) // damn it, something really went wrong
            std::exit(1);

        NLog::log("{}Running test {}", Colors::BLUE, tc->name());
        tc->test();

        if (tc->failed) {
            NLog::red("Test failed!: {}", tc->name());
            res.failedNames.emplace_back(std::format("{}:{}", tc->groupName(), tc->name()));
        } else
            NLog::green("Test passed: {}", tc->name());
    }

    return res;
}

static void cleanupAndReport(const STestsRunResult& tInfo) {
    NLog::green("dispatching exit");
    getFromSocket("/dispatch hl.dsp.exit()");

    NLog::log("\nSummary:\n\tPASSED: {}{}{}/{}", Colors::GREEN, tInfo.total - tInfo.failedNames.size(), Colors::RESET, tInfo.total);
    NLog::log("\tFAILED: {}{}{}/{}", Colors::RED, tInfo.failedNames.size(), Colors::RESET, tInfo.total);
    if (!tInfo.failedNames.empty()) {
        NLog::red("Failed tests:");
        for (const auto& name : tInfo.failedNames) {
            NLog::red("\t- {}", name);
        }
    }

    kill(hyprlandProc->pid(), SIGKILL);
    hyprlandProc.reset();
}

int main(int argc, char** argv, char** envp) {

    std::span<const char*>                  args{const_cast<const char**>(argv + 1), sc<std::size_t>(argc - 1)};
    const SSettings                         settings = parseSettings(args);

    std::vector<std::shared_ptr<CTestCase>> requestedTestCases;
    for (auto& test : settings.requestedTests) {
        if (testCases.contains(test)) {
            requestedTestCases.push_back(testCases.at(test));
        } else {
            NLog::red("ERROR: Unknown test name '{}'", Colors::RED, test);
            return EXIT_FAILURE;
        }
    }
    if (requestedTestCases.empty()) {
        // When no tests are explicitly requested, run all tests.
        // For convenience of log inspection, run tests group by group.
        requestedTestCases = miscTestCases;
        std::ranges::copy(clientTestCases, std::back_inserter(requestedTestCases));
        std::ranges::copy(mainTestCases, std::back_inserter(requestedTestCases));
    }

    NLog::yellow("launching hl");
    if (!launchHyprland(settings.configPath, settings.binaryPath)) {
        NLog::red("well it failed");
        return 1;
    }

    // hyprland has launched, let's check if it's alive after 10s
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    NLog::yellow("slept for 10s");
    if (!hyprlandAlive()) {
        NLog::red("Hyprland failed to launch!");
        return 1;
    }

    // wonderful, we are in. Let's get the instance signature.
    NLog::yellow("trying to get INSTANCES");
    const auto INSTANCES = instances();
    if (INSTANCES.empty()) {
        NLog::red("Hyprland failed to launch (2)");
        return 1;
    }

    HIS       = INSTANCES.back().id;
    WLDISPLAY = INSTANCES.back().wlSocket;

    NLog::yellow("trying to get create headless output");
    const auto CREATE_HEADLESS_2 = getFromSocket("/output create headless HEADLESS-2");
    if (CREATE_HEADLESS_2 != "ok" && CREATE_HEADLESS_2 != "Name already taken") {
        NLog::red("Failed to create HEADLESS-2: {}", CREATE_HEADLESS_2);
        getFromSocket("/dispatch hl.dsp.exit()");
        return 1;
    }

    NLog::yellow("trying to load plugin");
    if (const auto R = getFromSocket(std::format("/plugin load {}", settings.pluginPath.string())); R != "ok") {
        NLog::red("Failed to load the test plugin: {}", R);
        getFromSocket("/dispatch hl.dsp.exit()");
        return 1;
    }

    NLog::yellow("Loaded plugin");

    STestsRunResult result = runTests(requestedTestCases);

    cleanupAndReport(result);

    return result.failedNames.size() > 0;
}
