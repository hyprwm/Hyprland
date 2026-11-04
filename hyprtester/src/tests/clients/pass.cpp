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
    // A test client driven over stdin/stdout, see clients/pass-target.cpp and clients/x11-keys.cpp
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

// F6, as an xkb / X keycode
static constexpr uint32_t PASS_KEY = KEY_F6 + 8;

// presses or releases the key, with the modifiers held
static std::string keyCmd(uint32_t key, bool pressed, const SMods& mods) {
    return std::format("/eval hl.plugin.test.keybind_modmask({}, {}, {})", pressed ? 1 : 0, mods.xkbMask, key);
}

static void unbindKeys() {
    for (const auto& mods : MODS) {
        getFromSocket(std::format("/eval hl.unbind('{}')", bindKey(mods, "F6")));
    }
}

static std::string xDisplay() {
    auto display = getFromSocket("/repl os.getenv('DISPLAY')");
    while (!display.empty() && (display.back() == '\n' || display.back() == ' '))
        display.pop_back();
    return display;
}

// Xwayland handles key state asynchronously, wait for it to settle
static bool waitX11(CPipeClient& client, const std::string& cmd, const std::string& expected) {
    std::string got;
    for (int i = 0; i < 20; ++i) {
        got = client.request(cmd);
        if (got == expected)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    NLog::log("{}x11-keys: {} is {}, expected {}", Colors::RED, cmd, got, expected);
    return false;
}

static bool waitKeyHeld(CPipeClient& client, bool held) {
    return waitX11(client, std::format("held {}", PASS_KEY), held ? "1" : "0");
}

static bool waitXMods(CPipeClient& client, uint16_t mask) {
    return waitX11(client, "mods", std::to_string(mask));
}

// a release would also reach Xwayland asynchronously, so give it time to arrive before checking
static bool keyStaysHeld(CPipeClient& client) {
    Tests::sync();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    return client.request(std::format("held {}", PASS_KEY)) == "1";
}

// The focused window, e.g. a game, must keep focus during a pass, and see neither the bind's key nor
// the passed one.
static bool keptFocus(CPipeClient& client, const std::string& cls) {
    const auto ACTIVE = Tests::getAttribute(getFromSocket("/activewindow"), "class");
    if (ACTIVE != cls) {
        NLog::log("{}The active window is {}, expected {}", Colors::RED, ACTIVE, cls);
        return false;
    }

    const auto EVENTS = targetEvents(client);
    const auto LEAVE  = EVENTS.rfind("kleave");
    const auto ENTER  = EVENTS.rfind("kenter");
    if (LEAVE != std::string::npos && (ENTER == std::string::npos || ENTER < LEAVE)) {
        NLog::log("{}{} lost keyboard focus", Colors::RED, cls);
        return false;
    }

    if (EVENTS.contains("key:")) {
        NLog::log("{}{} got key events", Colors::RED, cls);
        return false;
    }

    return true;
}

TEST_CASE(passHeldKeyXwayland) {
    // Xwayland releases all keys on wl_keyboard.leave, so restoring keyboard focus after pass(), or
    // moving focus between Wayland windows, used to release the passed key during the hold.
    CScopeGuard cleanup([] {
        unbindKeys();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, other, other2;
    try {
        target.emplace("x11-keys", "x11-keys-a", "DISPLAY", xDisplay());
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
        other2.emplace("pass-target", "pass-other2", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        NLog::log("{}Passing {}", Colors::YELLOW, bindKey(mods, "F6"));
        OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^x11-keys-a$' }}))", bindKey(mods, "F6"))));

        ASSERT(focusWindow("pass-other"), true);
        EXPECT(waitKeyHeld(*target, false), true);
        targetEvents(*other);

        OK(getFromSocket(keyCmd(PASS_KEY, true, mods)));
        EXPECT(waitKeyHeld(*target, true), true);
        EXPECT(waitXMods(*target, mods.xMask), true);
        EXPECT(keptFocus(*other, "pass-other"), true);

        ASSERT(focusWindow("pass-other2"), true);
        EXPECT(keyStaysHeld(*target), true);
        targetEvents(*other2);

        OK(getFromSocket(keyCmd(PASS_KEY, false, mods)));
        EXPECT(waitKeyHeld(*target, false), true);
        EXPECT(keptFocus(*other2, "pass-other2"), true);

        OK(getFromSocket(setModsCmd(NO_MODS)));
        EXPECT(waitXMods(*target, 0), true);

        unbindKeys();
    }
}

TEST_CASE(passHeldKeyXwaylandModsReleasedFirst) {
    // Releasing the modifier before the key must keep the key held until it's released too.
    CScopeGuard cleanup([] {
        unbindKeys();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, other;
    try {
        target.emplace("x11-keys", "x11-keys-a", "DISPLAY", xDisplay());
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    const auto& MODS_HELD = MODS[1];
    OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^x11-keys-a$' }}))", bindKey(MODS_HELD, "F6"))));

    ASSERT(focusWindow("pass-other"), true);
    targetEvents(*other);

    OK(getFromSocket(keyCmd(PASS_KEY, true, MODS_HELD)));
    EXPECT(waitKeyHeld(*target, true), true);

    OK(getFromSocket(setModsCmd(NO_MODS)));
    EXPECT(keyStaysHeld(*target), true);

    OK(getFromSocket(keyCmd(PASS_KEY, false, NO_MODS)));
    EXPECT(waitKeyHeld(*target, false), true);
    EXPECT(waitXMods(*target, 0), true);
    EXPECT(keptFocus(*other, "pass-other"), true);
}

TEST_CASE(passHeldKeyXwaylandFocusX11) {
    // Focus entering an Xwayland surface during the hold used to release the passed key, as the
    // enter's key array didn't contain it.
    CScopeGuard cleanup([] {
        unbindKeys();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, otherX11, other;
    try {
        target.emplace("x11-keys", "x11-keys-a", "DISPLAY", xDisplay());
        otherX11.emplace("x11-keys", "x11-keys-b", "DISPLAY", xDisplay());
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        NLog::log("{}Passing {}", Colors::YELLOW, bindKey(mods, "F6"));
        OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^x11-keys-a$' }}))", bindKey(mods, "F6"))));

        ASSERT(focusWindow("pass-other"), true);

        OK(getFromSocket(keyCmd(PASS_KEY, true, mods)));
        EXPECT(waitKeyHeld(*target, true), true);

        ASSERT(focusWindow("x11-keys-b"), true);
        EXPECT(keyStaysHeld(*target), true);

        OK(getFromSocket(keyCmd(PASS_KEY, false, mods)));
        EXPECT(waitKeyHeld(*target, false), true);

        OK(getFromSocket(setModsCmd(NO_MODS)));
        EXPECT(waitXMods(*target, 0), true);

        unbindKeys();
    }
}

TEST_CASE(passHeldKeyXwaylandTargetClosed) {
    // Passing from one X11 window to another sends the key to the focused one. Closing the target during
    // the hold must not stop the withheld leave from being sent to Xwayland on release.
    CScopeGuard cleanup([] {
        unbindKeys();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> focused, other;
    try {
        focused.emplace("x11-keys", "x11-keys-b", "DISPLAY", xDisplay());
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        NLog::log("{}Passing {}", Colors::YELLOW, bindKey(mods, "F6"));
        OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^x11-keys-a$' }}))", bindKey(mods, "F6"))));

        std::optional<CPipeClient> target;
        try {
            target.emplace("x11-keys", "x11-keys-a", "DISPLAY", xDisplay());
        } catch (std::exception& e) { FAIL_TEST("Couldn't start the target: {}", e.what()); }

        ASSERT(focusWindow("x11-keys-b"), true);

        OK(getFromSocket(keyCmd(PASS_KEY, true, mods)));
        EXPECT(waitKeyHeld(*focused, true), true);

        ASSERT(focusWindow("pass-other"), true);
        EXPECT(keyStaysHeld(*focused), true);

        const int COUNT_BEFORE = Tests::windowCount();
        target.reset();
        for (int i = 0; i < 50 && Tests::windowCount() == COUNT_BEFORE; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT(Tests::windowCount(), COUNT_BEFORE - 1);

        EXPECT_CONTAINS(getFromSocket(keyCmd(PASS_KEY, false, mods)), "window not found");
        EXPECT(waitKeyHeld(*focused, false), true);

        OK(getFromSocket(setModsCmd(NO_MODS)));
        EXPECT(waitXMods(*focused, 0), true);

        unbindKeys();
    }
}

TEST_CASE(passHeldKeyXwaylandTargetLost) {
    // If the target stops matching during the hold, the release can't be passed. The key must still
    // be released on the physical release, also when an XWayland window has focus by then, which
    // gets no leave to release it.
    CScopeGuard cleanup([] {
        unbindKeys();
        getFromSocket(setModsCmd(NO_MODS));
        Tests::killAllWindows();
    });

    Tests::killAllWindows();

    std::optional<CPipeClient> target, otherX11, other;
    try {
        target.emplace("x11-keys", "x11-keys-a", "DISPLAY", xDisplay());
        otherX11.emplace("x11-keys", "x11-keys-b", "DISPLAY", xDisplay());
        other.emplace("pass-target", "pass-other", "WAYLAND_DISPLAY", WLDISPLAY);
    } catch (std::exception& e) { FAIL_TEST("Couldn't start the clients: {}", e.what()); }

    for (const auto& mods : MODS) {
        for (const auto& FOCUSED : {"pass-other", "x11-keys-b"}) {
            NLog::log("{}Passing {}, {} focused on release", Colors::YELLOW, bindKey(mods, "F6"), FOCUSED);
            OK(getFromSocket(std::format("/eval hl.bind('{}', hl.dsp.pass({{ window = 'class:^x11-keys-a$' }}))", bindKey(mods, "F6"))));

            ASSERT(focusWindow("pass-other"), true);

            OK(getFromSocket(keyCmd(PASS_KEY, true, mods)));
            EXPECT(waitKeyHeld(*target, true), true);

            ASSERT(focusWindow(FOCUSED), true);
            ASSERT(target->request("class x11-keys-gone"), "ok");
            Tests::sync();
            EXPECT_CONTAINS(getFromSocket(keyCmd(PASS_KEY, false, mods)), "window not found");
            EXPECT(waitKeyHeld(*target, false), true);

            OK(getFromSocket(setModsCmd(NO_MODS)));
            EXPECT(waitXMods(*target, 0), true);

            // and passing works normally again afterwards
            ASSERT(target->request("class x11-keys-a"), "ok");
            ASSERT(focusWindow("pass-other"), true);

            OK(getFromSocket(keyCmd(PASS_KEY, true, mods)));
            EXPECT(waitKeyHeld(*target, true), true);

            OK(getFromSocket(keyCmd(PASS_KEY, false, mods)));
            EXPECT(waitKeyHeld(*target, false), true);

            OK(getFromSocket(setModsCmd(NO_MODS)));
            EXPECT(waitXMods(*target, 0), true);

            unbindKeys();
        }
    }
}
