#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <chrono>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static std::string execAndGet(const std::string& cmd) {
    CProcess proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync()) {
        return "error";
    }

    return proc.stdOut();
}

static bool test() {
    NLog::log("{}Testing process spawning", Colors::GREEN);

    // Note: POSIX sleep does not support fractional seconds, so
    // can't sleep for less than 1 second.
    OK(getFromSocket("/dispatch exec sleep 1"));

    // Ensure that sleep is our child
    const std::string sleepPidS = execAndGet("pgrep sleep");
    pid_t             sleepPid;
    try {
        sleepPid = std::stoull(sleepPidS);
    } catch (...) {
        NLog::log("{}Sleep was not spawned or several sleeps are running: pgrep returned '{}'", Colors::RED, sleepPidS);
        return false;
    }

    const std::string sleepParentComm = execAndGet("cat \"/proc/$(ps -o ppid:1= -p " + sleepPidS + ")/comm\"");
    NLog::log("{}Expecting that sleep's parent is Hyprland", Colors::YELLOW);
    EXPECT_CONTAINS(sleepParentComm, "Hyprland");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Ensure that sleep did not become a zombie
    EXPECT(Tests::processAlive(sleepPid), false);

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
