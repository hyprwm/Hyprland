#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <array>
#include <optional>
#include <sys/poll.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <vector>
#include <sstream>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

namespace {
    class CKeyboardEnterClient {
        SP<CProcess>               proc;
        CFileDescriptor            readFd, writeFd;
        struct pollfd              fd;
        std::string                pending;

        std::optional<std::string> readLine(int timeoutMs);

      public:
        explicit CKeyboardEnterClient(const std::string& appId);
        ~CKeyboardEnterClient();

        bool                            focus() const;
        void                            drain();
        std::optional<std::vector<int>> waitForEnter(int timeoutMs);
    };
}

std::optional<std::string> CKeyboardEnterClient::readLine(int timeoutMs) {
    const auto hasBufferedLine = [&]() { return pending.find('\n') != std::string::npos; };

    while (!hasBufferedLine()) {
        fd.revents = 0;
        if (poll(&fd, 1, timeoutMs) != 1 || !(fd.revents & POLLIN))
            return std::nullopt;

        std::array<char, 512> buf       = {};
        const auto            bytesRead = read(readFd.get(), buf.data(), buf.size() - 1);
        if (bytesRead <= 0)
            return std::nullopt;

        pending.append(buf.data(), static_cast<size_t>(bytesRead));
    }

    const auto newlinePos = pending.find('\n');
    auto       line       = pending.substr(0, newlinePos);
    pending.erase(0, newlinePos + 1);
    return line;
}

CKeyboardEnterClient::CKeyboardEnterClient(const std::string& appId) {
    proc = makeShared<CProcess>(binaryDir + "/keyboard-enter", std::vector<std::string>{appId});
    proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int stdoutPipe[2], stdinPipe[2];
    if (pipe(stdoutPipe) != 0 || pipe(stdinPipe) != 0)
        throw std::exception();

    readFd  = CFileDescriptor(stdoutPipe[0]);
    writeFd = CFileDescriptor(stdinPipe[1]);
    proc->setStdoutFD(stdoutPipe[1]);
    proc->setStdinFD(stdinPipe[0]);

    const auto countBefore = Tests::windowCount();
    proc->runAsync();

    close(stdoutPipe[1]);
    close(stdinPipe[0]);

    fd = {.fd = readFd.get(), .events = POLLIN};

    const auto startedLine = readLine(1500);
    if (!startedLine || *startedLine != "started")
        throw std::exception();

    int counter = 0;
    while (Tests::processAlive(proc->pid()) && Tests::windowCount() == countBefore) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 100)
            throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = 'pid:{}', prop = 'no_anim', value = '1' }})", proc->pid())) != "ok")
        throw std::exception();
}

CKeyboardEnterClient::~CKeyboardEnterClient() {
    if (writeFd.isValid()) {
        const std::string cmd = "exit\n";
        (void)write(writeFd.get(), cmd.c_str(), cmd.length());
    }

    if (proc) {
        kill(proc->pid(), SIGKILL);
        proc.reset();
    }
}

bool CKeyboardEnterClient::focus() const {
    return getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", proc->pid())) == "ok";
}

void CKeyboardEnterClient::drain() {
    while (true) {
        fd.revents = 0;
        if (poll(&fd, 1, 20) != 1 || !(fd.revents & POLLIN))
            return;

        if (!readLine(20).has_value())
            return;
    }
}

std::optional<std::vector<int>> CKeyboardEnterClient::waitForEnter(int timeoutMs) {
    const auto line = readLine(timeoutMs);
    if (!line || !line->starts_with("enter "))
        return std::nullopt;

    std::stringstream ss(line->substr(6));
    size_t            count = 0;
    ss >> count;

    std::vector<int> keys;
    keys.reserve(count);

    int key = 0;
    while (ss >> key)
        keys.emplace_back(key);

    if (keys.size() != count)
        return std::nullopt;

    return keys;
}

TEST_CASE(keyboardEnterPressedKeysAfterDeviceDestroy) {
    std::optional<CKeyboardEnterClient> clientA;
    std::optional<CKeyboardEnterClient> clientB;

    try {
        clientA.emplace("keyboard-enter-a");
        clientB.emplace("keyboard-enter-b");
    } catch (...) { FAIL_TEST("Couldn't start keyboard-enter clients"); }

    clientA->drain();
    clientB->drain();

    OK(getFromSocket("/eval hl.plugin.test.stale_enter_setup()"));

    if (!clientA->focus())
        FAIL_TEST("Couldn't focus client A");
    const auto initialEnter = clientA->waitForEnter(1500);
    if (!initialEnter.has_value())
        FAIL_TEST("Client A didn't receive initial keyboard enter");
    ASSERT(initialEnter->empty(), true);

    OK(getFromSocket("/eval hl.plugin.test.stale_enter_key(0, 1, 1)"));

    if (!clientB->focus())
        FAIL_TEST("Couldn't focus client B");
    const auto staleEnter = clientB->waitForEnter(1500);
    if (!staleEnter.has_value())
        FAIL_TEST("Client B didn't receive keyboard enter while key was pressed");
    ASSERT(staleEnter->size(), 1);
    ASSERT((*staleEnter)[0], 1);

    OK(getFromSocket("/eval hl.plugin.test.stale_enter_destroy(0)"));

    if (!clientA->focus())
        FAIL_TEST("Couldn't refocus client A");
    const auto cleanedEnter = clientA->waitForEnter(1500);
    if (!cleanedEnter.has_value())
        FAIL_TEST("Client A didn't receive keyboard enter after keyboard destroy");
    ASSERT(cleanedEnter->empty(), true);

    OK(getFromSocket("/eval hl.plugin.test.stale_enter_cleanup()"));
}
