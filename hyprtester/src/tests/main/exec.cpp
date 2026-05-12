#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <chrono>
#include <format>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

const static auto SLEEP_DURATIONS = std::array{1, 10};

TEST_CASE(processSpawning) {
    for (const auto duration : SLEEP_DURATIONS) {
        // Note: POSIX sleep does not support fractional seconds, so
        // can't sleep for less than 1 second.
        OK(getFromSocket(std::format("/dispatch hl.dsp.exec_cmd('sleep {}')", duration)));

        // Ensure that sleep is our child
        const std::string sleepPidS = Tests::execAndGet("pgrep sleep");
        pid_t             sleepPid;
        try {
            sleepPid = std::stoull(sleepPidS);
        } catch (...) {
            NLog::log("{}Sleep was not spawned or several sleeps are running: pgrep returned '{}'", Colors::RED, sleepPidS);
            continue;
        }

        const std::string sleepParentComm = Tests::execAndGet("cat \"/proc/$(ps -o ppid:1= -p " + sleepPidS + ")/comm\"");
        NLog::log("{}Expecting that sleep's parent is Hyprland", Colors::YELLOW);
        EXPECT_CONTAINS(sleepParentComm, "Hyprland");

        std::this_thread::sleep_for(std::chrono::seconds(duration));

        // Ensure that sleep did not become a zombie
        EXPECT(Tests::processAlive(sleepPid), false);

        // Test succeeded
        return;
    }

    FAIL_TEST_SILENT();
}
