#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <array>
#include <chrono>
#include <csignal>
#include <format>
#include <linux/input-event-codes.h>
#include <optional>
#include <stdexcept>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

#define SP CSharedPointer

namespace {
    // A test client driven over stdin/stdout, see clients/pass-target.cpp
    class CPipeClient {
      public:
        CPipeClient(const std::string& binary, const std::string& arg, const std::string& envName, const std::string& envValue);
        ~CPipeClient();

        std::string request(const std::string& cmd);

      private:
        std::string     readLine(int timeoutMs);

        SP<CProcess>    m_proc;
        CFileDescriptor m_readFd, m_writeFd;
        std::string     m_buf;
    };
}

CPipeClient::CPipeClient(const std::string& binary, const std::string& arg, const std::string& envName, const std::string& envValue) {
    m_proc = makeShared<CProcess>(std::format("{}/{}", binaryDir, binary), std::vector<std::string>{arg});
    m_proc->addEnv(envName, envValue);

    int toClient[2] = {-1, -1};
    if (pipe(toClient) != 0)
        throw std::runtime_error("unable to open pipes");

    CFileDescriptor clientStdin(toClient[0]);
    m_writeFd = CFileDescriptor(toClient[1]);

    int fromClient[2] = {-1, -1};
    if (pipe(fromClient) != 0)
        throw std::runtime_error("unable to open pipes");

    m_readFd = CFileDescriptor(fromClient[0]);
    CFileDescriptor clientStdout(fromClient[1]);

    m_proc->setStdinFD(clientStdin.get());
    m_proc->setStdoutFD(clientStdout.get());

    const int COUNT_BEFORE = Tests::windowCount();
    m_proc->runAsync();

    // the client has its own copies now
    clientStdin.reset();
    clientStdout.reset();

    if (readLine(5000) != "started")
        throw std::runtime_error(std::format("{} didn't start", binary));

    for (int i = 0; i < 50 && Tests::windowCount() == COUNT_BEFORE; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (Tests::windowCount() == COUNT_BEFORE)
        throw std::runtime_error(std::format("{} didn't map", binary));
}

CPipeClient::~CPipeClient() {
    const std::string CMD = "exit\n";
    write(m_writeFd.get(), CMD.c_str(), CMD.length());
    kill(m_proc->pid(), SIGKILL);
}

std::string CPipeClient::readLine(int timeoutMs) {
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (!m_buf.contains('\n')) {
        const auto LEFT = std::chrono::duration_cast<std::chrono::milliseconds>(DEADLINE - std::chrono::steady_clock::now()).count();
        if (LEFT <= 0)
            return "";

        pollfd fd = {.fd = m_readFd.get(), .events = POLLIN};
        if (poll(&fd, 1, LEFT) != 1 || !(fd.revents & POLLIN))
            return "";

        std::array<char, 1024> buf = {};
        const auto             LEN = read(m_readFd.get(), buf.data(), buf.size());
        if (LEN <= 0)
            return "";

        m_buf.append(buf.data(), LEN);
    }

    const auto  POS  = m_buf.find('\n');
    std::string line = m_buf.substr(0, POS);
    m_buf.erase(0, POS + 1);
    return line;
}

std::string CPipeClient::request(const std::string& cmd) {
    const std::string CMD = cmd + "\n";
    if (write(m_writeFd.get(), CMD.c_str(), CMD.length()) != sc<ssize_t>(CMD.length()))
        return "";

    return readLine(1500);
}

// focuses the window and moves the cursor over it, so the pass target has neither focus
static bool focusWindow(const std::string& cls) {
    getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:^{}$' }})", cls));

    const auto ACTIVE = getFromSocket("/activewindow");
    if (Tests::getAttribute(ACTIVE, "class") != cls)
        return false;

    const auto AT   = Tests::getAttribute(ACTIVE, "at");
    const auto SIZE = Tests::getAttribute(ACTIVE, "size");

    try {
        const int X = std::stoi(AT.substr(0, AT.find(','))) + (std::stoi(SIZE.substr(0, SIZE.find(','))) / 2);
        const int Y = std::stoi(AT.substr(AT.find(',') + 1)) + (std::stoi(SIZE.substr(SIZE.find(',') + 1)) / 2);
        if (getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", X, Y)) != "ok")
            return false;
    } catch (std::exception& e) {
        NLog::log("{}Couldn't parse the geometry of {}: {}", Colors::RED, cls, e.what());
        return false;
    }

    Tests::sync();
    return true;
}

static std::string clickCmd(uint32_t button, bool pressed) {
    return std::format("/eval hl.plugin.test.click({}, {})", button, pressed ? 1 : 0);
}

// events reach the client asynchronously
static std::string targetEvents(CPipeClient& client) {
    Tests::sync();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto EVENTS = client.request("events");
    NLog::log("{}pass-target events: {}", Colors::BLUE, EVENTS);
    return EVENTS;
}

namespace {
    // the modifiers held with a bind's key or button, to test single keys and modifier + key combinations
    struct SMods {
        std::string name;        // as in a bind
        uint32_t    xkbMask = 0; // as set on the test keyboard
        uint16_t    xMask   = 0; // as reported by the X server
    };
}

static const SMods                NO_MODS = {};
static const std::array<SMods, 2> MODS    = {NO_MODS, SMods{.name = "SUPER", .xkbMask = 64, .xMask = 64}};

static std::string                setModsCmd(const SMods& mods) {
    return std::format("/eval hl.plugin.test.set_mods(0, {}, 0, 0, 0)", mods.xkbMask);
}

// as in a bind, e.g. "SUPER + F6"
static std::string bindKey(const SMods& mods, const std::string& key) {
    return mods.name.empty() ? key : std::format("{} + {}", mods.name, key);
}

static const std::string PASS_BUTTON = std::format("mouse:{}", BTN_EXTRA);

static void              unbindButtons() {
    for (const auto& mods : MODS) {
        getFromSocket(std::format("/eval hl.unbind('{}')", bindKey(mods, PASS_BUTTON)));
    }
}

TEST_CASE(passHeldMouseButton) {
    // Restoring pointer focus after pass() used to release the passed button right away, so the
    // target saw a click instead of a hold.
    CScopeGuard cleanup([] {
        unbindButtons();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, other;
    try {
        target.emplace("pass-target", "pass-target", "WAYLAND_DISPLAY", WLDISPLAY);
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        NLog::log("{}Passing {}", Colors::YELLOW, bindKey(mods, PASS_BUTTON));
        OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^pass-target$' }}))", bindKey(mods, PASS_BUTTON))));

        ASSERT(focusWindow("pass-other"), true);
        OK(getFromSocket(setModsCmd(mods)));
        targetEvents(*target);

        OK(getFromSocket(clickCmd(BTN_EXTRA, true)));
        auto events = targetEvents(*target);
        EXPECT_CONTAINS(events, std::format("button:{}:1", BTN_EXTRA));
        EXPECT_NOT_CONTAINS(events, std::format("button:{}:0", BTN_EXTRA));

        OK(getFromSocket(clickCmd(BTN_EXTRA, false)));
        events = targetEvents(*target);
        EXPECT_CONTAINS(events, std::format("button:{}:0", BTN_EXTRA));

        OK(getFromSocket(setModsCmd(NO_MODS)));
        unbindButtons();
    }
}

TEST_CASE(passHeldMouseButtonTargetLost) {
    // If the target stops matching during the hold, the release can't be passed. The target must still
    // get it with the physical release, and the next passed press to it must not be dropped.
    CScopeGuard cleanup([] {
        unbindButtons();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, other;
    try {
        target.emplace("pass-target", "pass-target", "WAYLAND_DISPLAY", WLDISPLAY);
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        NLog::log("{}Passing {}", Colors::YELLOW, bindKey(mods, PASS_BUTTON));
        OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^pass-target$' }}))", bindKey(mods, PASS_BUTTON))));

        ASSERT(focusWindow("pass-other"), true);
        OK(getFromSocket(setModsCmd(mods)));

        targetEvents(*target);

        OK(getFromSocket(clickCmd(BTN_EXTRA, true)));
        ASSERT(target->request("appid pass-gone"), "ok");
        Tests::sync();
        EXPECT_CONTAINS(getFromSocket(clickCmd(BTN_EXTRA, false)), "window not found");
        EXPECT_CONTAINS(targetEvents(*target), std::format("button:{}:0", BTN_EXTRA));

        ASSERT(target->request("appid pass-target"), "ok");

        OK(getFromSocket(clickCmd(BTN_EXTRA, true)));
        EXPECT_CONTAINS(targetEvents(*target), std::format("button:{}:1", BTN_EXTRA));

        OK(getFromSocket(clickCmd(BTN_EXTRA, false)));
        EXPECT_CONTAINS(targetEvents(*target), std::format("button:{}:0", BTN_EXTRA));

        OK(getFromSocket(setModsCmd(NO_MODS)));
        unbindButtons();
    }
}
