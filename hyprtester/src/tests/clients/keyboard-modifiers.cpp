#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <optional>
#include <sys/poll.h>
#include <csignal>
#include <sstream>
#include <thread>
#include <utility>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

namespace {

    class CClient {
        SP<CProcess>           proc;
        std::array<char, 1024> readBuf;
        CFileDescriptor        readFd, writeFd;
        struct pollfd          fds;

      public:
        CClient();
        ~CClient();
        uint32_t getLockedMods();
        // total (pressCount, releaseCount) of wl_keyboard.key events this window has received
        std::pair<uint32_t, uint32_t> getReceivedKeyCounts();
        pid_t                         pid();
    };
}

CClient::CClient() {
    Tests::killAllWindows();
    this->proc = makeShared<CProcess>(binaryDir + "/keyboard-modifiers", std::vector<std::string>{});

    this->proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        throw std::exception();
    }

    this->writeFd = CFileDescriptor(pipeFds1[1]);
    this->proc->setStdinFD(pipeFds1[0]);

    this->readFd = CFileDescriptor(pipeFds2[0]);
    this->proc->setStdoutFD(pipeFds2[1]);

    const int COUNT_BEFORE = Tests::windowCount();
    this->proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    this->fds = {.fd = this->readFd.get(), .events = POLLIN};
    if (poll(&this->fds, 1, 1000) != 1 || !(this->fds.revents & POLLIN)) {
        NLog::log("{}keyboard-modifiers client failed poll", Colors::RED);
        throw std::exception();
    }

    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1) {
        NLog::log("{}keyboard-modifiers client read failed", Colors::RED);
        throw std::exception();
    }

    std::string ret = std::string{this->readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start keyboard-modifiers client, read {}", Colors::RED, ret);
        throw std::exception();
    }

    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}keyboard-modifiers client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (!Tests::processAlive(this->proc->pid())) {
        NLog::log("{}keyboard-modifiers client not alive", Colors::RED);
        throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to focus keyboard-modifiers client", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started keyboard-modifiers client", Colors::YELLOW);
}

CClient::~CClient() {
    getFromSocket("/eval hl.plugin.test.set_mods(0, 0, 0, 0, 0)");
    getFromSocket("/eval hl.plugin.test.set_mods(1, 0, 0, 0, 0)");

    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

uint32_t CClient::getLockedMods() {
    std::string cmd = "get\n";
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&this->fds, 1, 1500) != 1 || !(this->fds.revents & POLLIN))
        return false;
    ssize_t bytesRead = read(this->fds.fd, this->readBuf.data(), 1023);
    if (bytesRead == -1)
        return false;

    this->readBuf[bytesRead] = 0;
    std::string received     = std::string{this->readBuf.data()};
    received.pop_back();

    try {
        return std::stoul(received);
    } catch (...) { return 0; }
}

std::pair<uint32_t, uint32_t> CClient::getReceivedKeyCounts() {
    std::string cmd = "keys\n";
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return {0, 0};

    if (poll(&this->fds, 1, 1500) != 1 || !(this->fds.revents & POLLIN))
        return {0, 0};
    ssize_t bytesRead = read(this->fds.fd, this->readBuf.data(), 1023);
    if (bytesRead == -1)
        return {0, 0};

    this->readBuf[bytesRead] = 0;
    std::istringstream iss{std::string{this->readBuf.data()}};
    uint32_t           pressed = 0, released = 0;
    iss >> pressed >> released;
    return {pressed, released};
}

pid_t CClient::pid() {
    return this->proc->pid();
}

// modifier index understood by hl.plugin.test.keybind (see eKeyboardModifierIndex in tests/main/keybinds.cpp)
static constexpr uint32_t MOD_CTRL = 3;

// inject a synthetic key press/release through the test plugin's virtual keyboard. `modifier` is the
// modifier index reported as held while the event is processed (0 = none); `key` is the xkb keycode.
static std::string keyCmd(bool pressed, uint32_t modifier, uint32_t key) {
    return "/eval hl.plugin.test.keybind(" + std::to_string(pressed ? 1 : 0) + ", " + std::to_string(modifier) + ", " + std::to_string(key) + ")";
}

TEST_CASE(keyboardModifiersMergedOnFocus) {
    NLog::log("{}Testing keyboard modifiers merged on focus", Colors::GREEN);

    std::optional<CClient> client;

    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    EXPECT(client->getLockedMods(), 0u);

    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OK(getFromSocket("/eval hl.plugin.test.set_mods(0, 0, 0, 2, 0)"));
    OK(getFromSocket("/eval hl.plugin.test.set_mods(1, 0, 0, 16, 0)"));

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", client->pid())) != "ok") {
        FAIL_TEST("Failed to refocus keyboard-modifiers client");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const uint32_t locked = client->getLockedMods();
    NLog::log("{}Client reports locked mods: {}", Colors::BLUE, locked);
    EXPECT(locked, 18u);
}

// Regression: a Lua `pass` bound to a *modifier* keycode must forward BOTH the press and the release
// to the target window. Before the back-fill fix in CKeybindManager, the press was forwarded but the
// release was dropped (the modmask guard skips a modifier key on release unless the bind is tracked in
// m_pressedSpecialBinds), leaving a key bound to a modifier (e.g. a push-to-talk key) latched "on" forever.
TEST_CASE(luaPassForwardsModifierRelease) {
    NLog::log("{}Testing Lua pass forwards a held modifier's release", Colors::GREEN);

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    OK(getFromSocket("/eval hl.bind('code:105', hl.dsp.pass({ window = 'class:keyboard-modifiers' }))"));

    // drop keyboard focus so a dropped release cannot reach the recorder via normal event delivery -
    // the recorder must only ever observe events that `pass` explicitly forwards to it.
    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OK(getFromSocket(keyCmd(true, 0, 105)));         // press (modifier's own bit not yet applied)
    OK(getFromSocket(keyCmd(false, MOD_CTRL, 105))); // release (Ctrl still applied)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto [pressed, released] = client->getReceivedKeyCounts();
    EXPECT(pressed, 1u);
    EXPECT(released, 1u); // 0 before the fix: the release is never forwarded

    OK(getFromSocket("/eval hl.unbind('code:105')"));
}

// Guard against over-correction: a Lua `pass` on a *non-modifier* key (which already worked before the
// fix, since the modmask guard does not skip non-modifier keys) must still forward both edges.
TEST_CASE(luaPassForwardsNonModifierRelease) {
    NLog::log("{}Testing Lua pass still forwards a non-modifier key's release", Colors::GREEN);

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    // keycode 31 is a normal (non-modifier) key; no modifier is held for either edge.
    OK(getFromSocket("/eval hl.bind('code:31', hl.dsp.pass({ window = 'class:keyboard-modifiers' }))"));

    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OK(getFromSocket(keyCmd(true, 0, 31)));
    OK(getFromSocket(keyCmd(false, 0, 31)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto [pressed, released] = client->getReceivedKeyCounts();
    EXPECT(pressed, 1u);
    EXPECT(released, 1u);

    OK(getFromSocket("/eval hl.unbind('code:31')"));
}

// The fix is not pass-specific: every Lua dispatcher that marks itself special mid-dispatch via
// releasePending benefits from the same back-fill. Exercise that path through `send_shortcut`, which
// forwards a configured key ('a') to the target. Bound to a modifier keycode it has the identical
// dropped-release bug before the fix.
TEST_CASE(luaSendShortcutForwardsModifierRelease) {
    NLog::log("{}Testing Lua send_shortcut forwards its release when bound to a modifier", Colors::GREEN);

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    OK(getFromSocket("/eval hl.bind('code:105', hl.dsp.send_shortcut({ mods = '', key = 'a', window = 'class:keyboard-modifiers' }))"));

    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OK(getFromSocket(keyCmd(true, 0, 105)));
    OK(getFromSocket(keyCmd(false, MOD_CTRL, 105)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto [pressed, released] = client->getReceivedKeyCounts();
    EXPECT(pressed, 1u);
    EXPECT(released, 1u); // 0 before the fix: the shortcut key's release is never sent and it sticks down

    OK(getFromSocket("/eval hl.unbind('code:105')"));
}

// Held-key path: while any other key stays down, m_pressedKeys never empties, so releasePending is not
// reset between cycles and leaks into the next press. The `!SPECIALDISPATCHER` guard keeps the back-fill
// and the top-of-loop add mutually exclusive there. Black-box check: each press/release cycle must still
// forward exactly one press and one release - never zero (dropped) and never two (double-forwarded).
TEST_CASE(luaPassModifierReleaseWithHeldKey) {
    NLog::log("{}Testing Lua pass modifier release with an unrelated key held down", Colors::GREEN);

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    OK(getFromSocket("/eval hl.bind('code:105', hl.dsp.pass({ window = 'class:keyboard-modifiers' }))"));

    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // hold an unrelated, unbound key (keycode 40) down for the whole test so m_pressedKeys never empties
    OK(getFromSocket(keyCmd(true, 0, 40)));

    for (uint32_t i = 1; i <= 3; ++i) {
        OK(getFromSocket(keyCmd(true, 0, 105)));
        OK(getFromSocket(keyCmd(false, MOD_CTRL, 105)));
        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        const auto [pressed, released] = client->getReceivedKeyCounts();
        EXPECT(pressed, i);
        EXPECT(released, i);
    }

    OK(getFromSocket(keyCmd(false, 0, 40))); // release the held key
    OK(getFromSocket("/eval hl.unbind('code:105')"));
}
