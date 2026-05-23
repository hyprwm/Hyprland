
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
#undef TEST_CASES_STORAGE // Prevent redefinition warning
#include "tests/clients/tests.hpp"
#undef TEST_CASES_STORAGE // Prevent redefinition warning
#include "tests/misc/tests.hpp"
#include "tests/shared.hpp"

#include <filesystem>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/Casts.hpp>

#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <print>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include "Log.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using Path = std::filesystem::path;

#define SP CSharedPointer

namespace {
    struct SSettings {
        Path configPath;
        Path binaryPath;
        Path pluginPath;
    };

    struct STestsInfo {
        unsigned long long       failed, total;
        std::vector<std::string> failedNames;
    };
}

static SP<CProcess> hyprlandProc;

static bool launchHyprland(Path configPath, Path binaryPath) {
    NLog::info("Launching Hyprland");
    hyprlandProc = makeShared<CProcess>(binaryPath, std::vector<std::string>{"--config", configPath});
    hyprlandProc->addEnv("HYPRLAND_HEADLESS_ONLY", "1");

    NLog::info("Launched async process");

    return hyprlandProc->runAsync();
}

static bool hyprlandAlive() {
    NLog::info("hyprlandAlive");
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

static bool preTestCleanup() {
    bool failed = false;

    if (!Tests::killAllWindows()) {
        NLog::error("Internal failure: failed to kill all windows");
        failed = true;
    }
    if (!Tests::killAllLayers()) {
        NLog::error("Internal failure: failed to kill all layers");
        failed = true;
    }
    if (getFromSocket("/reload") != "ok") {
        NLog::error("Internal failure: failed to reload");
        failed = true;
    }
    if (!getFromSocket("/activeworkspace").contains("workspace ID 1 (1)")) {
        if (getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })") != "ok") {
            NLog::error("Internal failure: failed to switch to workspace 1");
            failed = true;
        }
    }
    if (getFromSocket("/dispatch hl.dsp.cursor.move({ x = 960, y = 540 })") != "ok") {
        NLog::error("Internal failure: failed to reset cursor position");
        failed = true;
    }

    return !failed;
}

static void runTests(std::map<const char*, CTestCase&>& testCases, std::string suiteName, struct STestsInfo& testsInfo) {
    for (auto& [name, tc] : testCases) {
        // Clean up before every test
        NLog::info("Cleaning up"); 
            
        if (!preTestCleanup()) // damn it, something really went wrong
            std::exit(1);

        NLog::log("{}Running test {}", Colors::BLUE, name);
        tc.test();

        if (tc.failed) {
            NLog::error("Test failed!: {}", name);
            testsInfo.failedNames.emplace_back(suiteName + "/" + name);
            testsInfo.failed += 1;
        } else
            NLog::log("{}Test passed: {}", Colors::GREEN, name);
    }

    testsInfo.total += testCases.size();
}

int main(int argc, char** argv, char** envp) {

    std::span<const char*> args{const_cast<const char**>(argv + 1), sc<std::size_t>(argc - 1)};
    const SSettings        settings = parseSettings(args);

    NLog::info("launching hl");
    if (!launchHyprland(settings.configPath, settings.binaryPath)) {
        NLog::error("well it failed");
        return 1;
    }

    // hyprland has launched, let's check if it's alive after 10s
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    NLog::info("slept for 10s");
    if (!hyprlandAlive()) {
        NLog::error("Hyprland failed to launch!");
        return 1;
    }

    // wonderful, we are in. Let's get the instance signature.
    NLog::info("trying to get INSTANCES");
    const auto INSTANCES = instances();
    if (INSTANCES.empty()) {
        NLog::error("Hyprland failed to launch (2)");
        return 1;
    }

    HIS       = INSTANCES.back().id;
    WLDISPLAY = INSTANCES.back().wlSocket;

    NLog::info("trying to get create headless output");
    getFromSocket("/output create headless");

    NLog::info("trying to load plugin");
    if (const auto R = getFromSocket(std::format("/plugin load {}", settings.pluginPath.string())); R != "ok") {
        NLog::error("Failed to load the test plugin: {}", R);
        getFromSocket("/dispatch hl.dsp.exit()");
        return 1;
    }

    NLog::info("Loaded plugin");

    struct STestsInfo tInfo = {0};

    NLog::info("Running misc tests");
    runTests(miscTestCases, "misc", tInfo);

    NLog::info("Running main tests");
    runTests(mainTestCases, "main", tInfo);

    NLog::info("Running protocol client tests");
    runTests(clientTestCases, "clients", tInfo);

    // kill hyprland
    NLog::info("dispatching exit");
    getFromSocket("/dispatch hl.dsp.exit()");

    NLog::log("\nSummary:\n\tPASSED: {}{}{}/{}", Colors::GREEN, tInfo.total - tInfo.failed, Colors::RESET, tInfo.total);
    NLog::log("\tFAILED: {}{}{}/{}", Colors::RED, tInfo.failed, Colors::RESET, tInfo.total);
    if (!tInfo.failedNames.empty()) {
        NLog::log("{}Failed tests:", Colors::RED);
        for (const auto& name : tInfo.failedNames) {
            NLog::log("{}\t- {}", Colors::RED, name);
        }
    }

    kill(hyprlandProc->pid(), SIGKILL);
    hyprlandProc.reset();

    return tInfo.failed > 0;
}
