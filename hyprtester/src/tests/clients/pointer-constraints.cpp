#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <format>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

class CPointerConstraintsClient {
  public:
    CPointerConstraintsClient();
    ~CPointerConstraintsClient();
    std::string command(const std::string& command);

  private:
    std::string              readLine();
    CSharedPointer<CProcess> m_proc;
    CFileDescriptor          m_readFd, m_writeFd;
};

CPointerConstraintsClient::CPointerConstraintsClient() {
    m_proc = makeShared<CProcess>(std::format("{}/pointer-constraints", binaryDir), std::vector<std::string>{});
    m_proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);
    int input[2] = {}, output[2] = {};
    if (pipe(input) != 0)
        throw std::runtime_error("client input pipe failed");
    CFileDescriptor inputRead(input[0]);
    m_writeFd = CFileDescriptor(input[1]);
    if (pipe(output) != 0)
        throw std::runtime_error("client output pipe failed");
    CFileDescriptor outputWrite(output[1]);
    m_readFd = CFileDescriptor(output[0]);
    m_proc->setStdinFD(inputRead.get());
    m_proc->setStdoutFD(outputWrite.get());
    if (!m_proc->runAsync())
        throw std::runtime_error("pointer-constraints launch failed");
    bool        started = false;
    CScopeGuard failedStartup([&] {
        if (!started && m_proc->pid() > 0)
            kill(m_proc->pid(), SIGKILL);
    });
    const auto  reply = readLine();
    if (reply != "started")
        throw std::runtime_error("pointer-constraints startup: " + reply);
    started = true;
}

CPointerConstraintsClient::~CPointerConstraintsClient() {
    if (m_proc && m_proc->pid() > 0)
        kill(m_proc->pid(), SIGKILL);
}

std::string CPointerConstraintsClient::readLine() {
    std::string result;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd     fd  = {.fd = m_readFd.get(), .events = POLLIN, .revents = 0};
        const auto ret = poll(&fd, 1, 100);
        if (ret < 0 && errno == EINTR)
            continue;
        if (ret < 0 || (fd.revents & (POLLERR | POLLNVAL)))
            break;
        if (!(fd.revents & POLLIN)) {
            if (fd.revents & POLLHUP)
                break;
            continue;
        }
        std::array<char, 1024> buffer = {};
        const auto             size   = read(m_readFd.get(), buffer.data(), buffer.size());
        if (size <= 0)
            break;
        result.append(buffer.data(), size);
        if (result.ends_with('\n')) {
            result.pop_back();
            return result;
        }
    }
    return "error client response: " + result;
}

std::string CPointerConstraintsClient::command(const std::string& command) {
    const auto line = command + '\n';
    if (write(m_writeFd.get(), line.data(), line.size()) != static_cast<ssize_t>(line.size()))
        return "error client write";
    return readLine();
}

static int count(const std::string& report, const std::string& key) {
    std::istringstream input(report);
    std::string        field;
    while (input >> field) {
        if (field.starts_with(key + '='))
            return std::stoi(field.substr(key.size() + 1));
    }
    return -1;
}

static bool matches(const std::string& report, std::initializer_list<std::string> fields) {
    for (const auto& field : fields) {
        if (!(" " + report + " ").contains(" " + field + " "))
            return false;
    }
    return true;
}

static std::string waitReport(CPointerConstraintsClient& client, std::initializer_list<std::string> fields) {
    std::string report;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        report = client.command("report");
        if (matches(report, fields))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } while (std::chrono::steady_clock::now() < deadline);
    return report;
}

SUBTEST(nativeConstraintLifecycle, const std::string& kind, const std::string& target, bool persistent, bool rule) {
    CScopeGuard cleanup([&] {
        EXPECT(Tests::killAllWindows(), true);
        EXPECT_OK(getFromSocket("/reload"));
    });
    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 }, cursor = { warp_on_change_workspace = 0 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:native_constraints' })"));
    OK(getFromSocket(std::format("/eval hl.window_rule({{ match = {{ class = '^pointer-constraints$' }}, float = true, size = {{320, 240}}, confine_pointer = {} }})", rule)));

    std::optional<CPointerConstraintsClient> client;
    try {
        client.emplace();
    } catch (const std::exception& e) { FAIL_TEST("{}", e.what()); }
    ASSERT_CONTAINS(waitReport(*client, {"keyboard=root"}), "keyboard=root");
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: pointer-constraints\n");

    int                x = 0, y = 0;
    char               comma = 0;
    std::istringstream position(Tests::getAttribute(getFromSocket("/activewindow"), "at"));
    ASSERT(static_cast<bool>(position >> x >> comma >> y), true);
    ASSERT(comma, ',');
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 20, y + 20)));
    const auto before = waitReport(*client, {"pointer=root", "keyboard=root"});
    ASSERT(matches(before, {"pointer=root", "keyboard=root", "active=0"}), true);
    const auto keyboardEnters = count(before, "keyboard_enters");
    const auto keyboardLeaves = count(before, "keyboard_leaves");
    const auto targetEnters   = count(before, target + "_enters");
    const auto activated      = kind == "lock" ? "locked" : "confined";
    const auto deactivated    = kind == "lock" ? "unlocked" : "unconfined";

    ASSERT(client->command(std::format("{} {} {}", kind, target, persistent ? "persistent" : "oneshot")), "ok");
    if (target == "child") {
        ASSERT(matches(client->command("report"), {"active=0", "pointer=root", "keyboard=root"}), true);
        // Acquire on child enter at local (20,20), inside the narrow confinement region.
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 100, y + 80)));
    }
    const auto active = waitReport(*client, {"active=1", "pointer=" + target, std::format("{}=1", activated)});
    ASSERT(matches(active, {"active=1", "pointer=" + target, "keyboard=root"}), true);
    EXPECT(count(active, activated), 1);
    EXPECT(count(active, deactivated), 0);
    EXPECT(count(active, "keyboard_enters"), keyboardEnters);
    EXPECT(count(active, "keyboard_leaves"), keyboardLeaves);
    EXPECT(count(active, target + "_enters"), targetEnters + (target == "child" ? 1 : 0));
    EXPECT(matches(active, {"locked_motions=0", "outside_motions=0", "bad_activations=0"}), true);
    if (kind == "confine") {
        EXPECT(count(active, "confined_motions") > 0, true);
        // Once active, an attempted motion to child-local (60,60) must be clamped.
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 140, y + 120)));
        const auto clamped = client->command("report");
        EXPECT(matches(clamped, {"active=1", "pointer=child", "keyboard=root", "outside_motions=0", "bad_activations=0"}), true);
        EXPECT(count(clamped, "confined_motions") > count(active, "confined_motions"), true);
        EXPECT(count(clamped, "motion_x") >= 10 && count(clamped, "motion_x") < 42, true);
        EXPECT(count(clamped, "motion_y") >= 10 && count(clamped, "motion_y") < 42, true);
    }

    // Same-window refocus must neither churn seat focus nor exhaust a one-shot constraint.
    OK(getFromSocket("/eval hl.plugin.test.window_soft_focus('pointer-constraints')"));
    const auto refocused = client->command("report");
    EXPECT(matches(refocused, {"active=1", "pointer=" + target, "keyboard=root", "locked_motions=0", "outside_motions=0", "bad_activations=0"}), true);
    for (const auto& field : {activated, deactivated, "root_enters", "child_enters", "keyboard_enters", "keyboard_leaves"}) {
        EXPECT(count(refocused, field), count(active, field));
    }

    if (kind == "lock") {
        OK(getFromSocket("/dispatch hl.dsp.window.pin({ action = 'set', window = 'class:pointer-constraints' })"));
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:native_constraints_pinned' })"));
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:native_constraints' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.pin({ action = 'unset', window = 'class:pointer-constraints' })"));
        const auto pinned = client->command("report");
        EXPECT(matches(pinned, {"active=1", "pointer=" + target, "keyboard=root", "locked_motions=0"}), true);
        for (const auto& field : {activated, deactivated, "root_enters", "child_enters", "keyboard_enters", "keyboard_leaves"}) {
            EXPECT(count(pinned, field), count(active, field));
        }
    }

    if (rule) {
        // A rule confines to the window, but a native lock must still pin the pointer within that window.
        const auto lockedPosition = getFromSocket("/cursorpos");
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 40, y + 40)));
        EXPECT(getFromSocket("/cursorpos"), lockedPosition);
        EXPECT(matches(client->command("report"), {"active=1", "locked=1", "pointer=" + target, "keyboard=root"}), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:native_constraints_empty' })"));
    ASSERT(Tests::getAttribute(getFromSocket("/activeworkspace"), "windows"), "0");
    const auto inactive = waitReport(*client, {"active=0", "pointer=none", "keyboard=none", std::format("{}=1", deactivated)});
    ASSERT(matches(inactive, {"active=0", "pointer=none", "keyboard=none"}), true);
    EXPECT(count(inactive, activated), 1);
    EXPECT(count(inactive, deactivated), 1);
    EXPECT(count(inactive, "keyboard_leaves"), keyboardLeaves + 1);

    // Observe restoration only: moving the pointer or explicitly refocusing the window would mask the regression.
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:native_constraints' })"));
    const auto restored =
        waitReport(*client, {"keyboard=root", "pointer=" + target, std::format("active={}", persistent ? 1 : 0), std::format("{}={}", activated, persistent ? 2 : 1)});
    ASSERT(matches(restored, {"keyboard=root", "pointer=" + target}), true);
    EXPECT(count(restored, "active"), persistent ? 1 : 0);
    EXPECT(count(restored, activated), persistent ? 2 : 1);
    EXPECT(count(restored, deactivated), 1);
    EXPECT(count(restored, target + "_enters"), count(active, target + "_enters") + 1);
    EXPECT(count(restored, "keyboard_enters"), keyboardEnters + 1);
    EXPECT(count(restored, "keyboard_leaves"), keyboardLeaves + 1);
    EXPECT(matches(restored, {"locked_motions=0", "outside_motions=0", "bad_activations=0"}), true);
    // Also catch delayed reactivation of a spent one-shot object or duplicate persistent activation events.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto settled = client->command("report");
    EXPECT(matches(settled, {"pointer=" + target, "keyboard=root", "locked_motions=0", "outside_motions=0", "bad_activations=0"}), true);
    for (const auto& field : {activated, deactivated, "active", "root_enters", "child_enters", "keyboard_enters", "keyboard_leaves"}) {
        EXPECT(count(settled, field), count(restored, field));
    }

    if (rule) {
        ASSERT(client->command("destroy"), "ok");
        // Native destruction must reveal the rule without a focus cycle: allow motion inside the window,
        // then clamp an attempted escape to its edge. A leftover lock fails the first check; no rule fails the second.
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 40, y + 40)));
        const auto freed = client->command("report");
        EXPECT(matches(freed, {"active=0", "pointer=root", "keyboard=root", "motion_x=40", "motion_y=40"}), true);
        EXPECT(count(freed, "motions") > count(restored, "motions"), true);
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + 400, y + 20)));
        const auto fallback = client->command("report");
        EXPECT(matches(fallback, {"active=0", "pointer=root", "keyboard=root", "motion_y=20", "locked_motions=0", "bad_activations=0"}), true);
        EXPECT(count(fallback, "motion_x") >= 319 && count(fallback, "motion_x") <= 320, true);
        for (const auto& field : {activated, deactivated, "keyboard_enters", "keyboard_leaves"}) {
            EXPECT(count(fallback, field), count(restored, field));
        }
        EXPECT(count(fallback, "root_enters"), count(freed, "root_enters"));
        EXPECT(count(fallback, "child_enters"), count(freed, "child_enters"));
    }
}

TEST_CASE(nativePointerConstraintsPersistent) {
    CALL_SUBTEST(nativeConstraintLifecycle, "lock", "root", true, false);
    CALL_SUBTEST(nativeConstraintLifecycle, "lock", "child", true, false);
    CALL_SUBTEST(nativeConstraintLifecycle, "confine", "child", true, false);
}

TEST_CASE(nativePointerConstraintsOneshot) {
    CALL_SUBTEST(nativeConstraintLifecycle, "lock", "child", false, false);
    CALL_SUBTEST(nativeConstraintLifecycle, "confine", "child", false, false);
}

TEST_CASE(nativePointerLockOverridesConfineRule) {
    CALL_SUBTEST(nativeConstraintLifecycle, "lock", "root", true, true);
    CALL_SUBTEST(nativeConstraintLifecycle, "lock", "child", true, true);
}
