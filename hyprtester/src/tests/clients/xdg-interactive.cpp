#include "../../Log.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <array>
#include <chrono>
#include <csignal>
#include <deque>
#include <optional>
#include <string>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

namespace {
    struct SGeometry {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    struct SStatus {
        int      leave              = 0;
        int      motionAfterRequest = 0;
        uint32_t lastButton         = 0;
        int      buttonPresses      = 0;
        bool     resizing           = false;
    };

    class CClient {
        SP<CProcess>               proc;
        std::array<char, 2048>     readBuf;
        CFileDescriptor            readFd, writeFd;
        std::string                pending;
        std::deque<std::string>    lines;
        bool                       disconnected = false;

        void                       parseLines();
        bool                       readOnce(int timeoutMs);
        std::optional<std::string> takeLineContaining(const std::string& marker);

      public:
        CClient();
        ~CClient();

        bool                       sendCommand(const std::string& command);
        std::optional<std::string> waitLineContaining(const std::string& marker, int timeoutMs = 1500);
        std::optional<uint32_t>    waitButtonSerial();
        std::optional<SStatus>     status();
        bool                       requestMove();
        bool                       requestResize(const std::string& edge);
        bool                       requestResizeWithSerial(uint32_t serial, const std::string& edge);
        bool                       requestRawResize(uint32_t edge);
        bool                       waitResizing(bool resizing);
        bool                       waitForDisconnect(int timeoutMs = 1500);
        pid_t                      pid() const;
    };
}

static std::pair<int, int> parsePair(const std::string& str) {
    const auto COMMA = str.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};

    return {std::stoi(str.substr(0, COMMA)), std::stoi(str.substr(COMMA + 1))};
}

static SGeometry activeGeometry() {
    const auto RESPONSE = getFromSocket("/activewindow");
    const auto AT       = parsePair(Tests::getAttribute(RESPONSE, "at"));
    const auto SIZE     = parsePair(Tests::getAttribute(RESPONSE, "size"));
    return {
        .x = AT.first,
        .y = AT.second,
        .w = SIZE.first,
        .h = SIZE.second,
    };
}

static bool focusClient(pid_t pid) {
    return getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", pid)) == "ok";
}

static bool setClientGeometry(pid_t pid, const SGeometry& geometry) {
    if (!focusClient(pid)) {
        NLog::log("{}Failed to focus xdg-interactive pid {}", Colors::RED, pid);
        return false;
    }

    if (const auto RESPONSE = getFromSocket(std::format("/dispatch hl.dsp.window.float({{ action = 'enable', window = 'pid:{}' }})", pid)); RESPONSE != "ok") {
        NLog::log("{}Failed to float xdg-interactive pid {}: {}", Colors::RED, pid, RESPONSE);
        return false;
    }

    if (const auto RESPONSE = getFromSocket(std::format("/dispatch hl.dsp.window.resize({{ x = {}, y = {}, window = 'pid:{}' }})", geometry.w, geometry.h, pid));
        RESPONSE != "ok") {
        NLog::log("{}Failed to resize xdg-interactive pid {}: {}", Colors::RED, pid, RESPONSE);
        return false;
    }

    if (const auto RESPONSE = getFromSocket(std::format("/dispatch hl.dsp.window.move({{ x = {}, y = {}, window = 'pid:{}' }})", geometry.x, geometry.y, pid)); RESPONSE != "ok") {
        NLog::log("{}Failed to move xdg-interactive pid {}: {}", Colors::RED, pid, RESPONSE);
        return false;
    }

    for (size_t i = 0; i < 50; ++i) {
        if (!focusClient(pid))
            return false;

        const auto CURRENT = activeGeometry();
        if (CURRENT.x == geometry.x && CURRENT.y == geometry.y && CURRENT.w == geometry.w && CURRENT.h == geometry.h)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const auto CURRENT = activeGeometry();
    NLog::log("{}Timed out waiting for xdg-interactive geometry, expected {},{} {}x{}, got {},{} {}x{}", Colors::RED, geometry.x, geometry.y, geometry.w, geometry.h, CURRENT.x,
              CURRENT.y, CURRENT.w, CURRENT.h);
    return false;
}

static bool moveCursor(int x, int y) {
    return getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x, y)) == "ok";
}

static bool click(uint32_t button, bool pressed) {
    return getFromSocket(std::format("/eval hl.plugin.test.click({}, {})", button, pressed ? 1 : 0)) == "ok";
}

static std::optional<int> statusValue(const std::string& line, const std::string& key) {
    const auto KEY = key + "=";
    auto       pos = line.find(KEY);
    if (pos == std::string::npos)
        return std::nullopt;

    pos += KEY.size();
    auto end = line.find(' ', pos);
    if (end == std::string::npos)
        end = line.size();

    try {
        return std::stoi(line.substr(pos, end - pos));
    } catch (...) { return std::nullopt; }
}

void CClient::parseLines() {
    auto pos = pending.find('\n');
    while (pos != std::string::npos) {
        lines.emplace_back(pending.substr(0, pos));
        pending.erase(0, pos + 1);
        pos = pending.find('\n');
    }
}

bool CClient::readOnce(int timeoutMs) {
    if (disconnected)
        return false;

    pollfd    fd  = {.fd = readFd.get(), .events = POLLIN};
    const int RET = poll(&fd, 1, timeoutMs);
    if (RET <= 0)
        return false;

    if (!(fd.revents & POLLIN)) {
        if (fd.revents & (POLLERR | POLLHUP | POLLNVAL))
            disconnected = true;
        return false;
    }

    const ssize_t BYTES = read(readFd.get(), readBuf.data(), readBuf.size() - 1);
    if (BYTES <= 0) {
        disconnected = true;
        return false;
    }

    readBuf[BYTES] = 0;
    pending += readBuf.data();
    parseLines();
    if (fd.revents & (POLLERR | POLLHUP | POLLNVAL))
        disconnected = true;
    return true;
}

std::optional<std::string> CClient::takeLineContaining(const std::string& marker) {
    for (auto it = lines.begin(); it != lines.end(); ++it) {
        if (!it->contains(marker))
            continue;

        auto line = *it;
        lines.erase(it);
        return line;
    }

    return std::nullopt;
}

CClient::CClient() {
    NLog::log("{}Attempting to start xdg-interactive client", Colors::YELLOW);

    proc = makeShared<CProcess>(binaryDir + "/xdg-interactive", std::vector<std::string>{});
    proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int procInPipeFd[2], procOutPipeFd[2];
    if (pipe(procInPipeFd) != 0 || pipe(procOutPipeFd) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        throw std::exception();
    }

    writeFd = CFileDescriptor(procInPipeFd[1]);
    proc->setStdinFD(procInPipeFd[0]);

    readFd = CFileDescriptor(procOutPipeFd[0]);
    proc->setStdoutFD(procOutPipeFd[1]);

    const int COUNT_BEFORE = Tests::windowCount();
    if (!proc->runAsync()) {
        NLog::log("{}Failed to run xdg-interactive client", Colors::RED);
        throw std::exception();
    }

    close(procInPipeFd[0]);
    close(procOutPipeFd[1]);

    if (!waitLineContaining("started", 2000)) {
        NLog::log("{}xdg-interactive client did not report startup", Colors::RED);
        throw std::exception();
    }

    int counter = 0;
    while (Tests::processAlive(proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        ++counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}xdg-interactive client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = 'pid:{}', prop = 'no_anim', value = '1' }})", proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for xdg-interactive client", Colors::RED);
        throw std::exception();
    }

    if (!focusClient(proc->pid())) {
        NLog::log("{}Failed to focus xdg-interactive client", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started xdg-interactive client", Colors::YELLOW);
}

CClient::~CClient() {
    if (!disconnected) {
        std::string cmd = "exit\n";
        write(writeFd.get(), cmd.c_str(), cmd.length());
    }

    if (proc)
        kill(proc->pid(), SIGKILL);
    proc.reset();
}

bool CClient::sendCommand(const std::string& command) {
    if (disconnected)
        return false;

    return (size_t)write(writeFd.get(), command.c_str(), command.length()) == command.length();
}

std::optional<std::string> CClient::waitLineContaining(const std::string& marker, int timeoutMs) {
    if (auto line = takeLineContaining(marker); line)
        return line;

    const auto START = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - START).count() < timeoutMs) {
        readOnce(50);
        if (auto line = takeLineContaining(marker); line)
            return line;

        if (disconnected)
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<uint32_t> CClient::waitButtonSerial() {
    const auto LINE = waitLineContaining("button ");
    if (!LINE)
        return std::nullopt;

    try {
        return std::stoul(LINE->substr(LINE->find("button ") + 7));
    } catch (...) { return std::nullopt; }
}

std::optional<SStatus> CClient::status() {
    if (!sendCommand("status\n"))
        return std::nullopt;

    const auto LINE = waitLineContaining("status ");
    if (!LINE)
        return std::nullopt;

    const auto LEAVE    = statusValue(*LINE, "leave");
    const auto MOTION   = statusValue(*LINE, "motion_after_request");
    const auto BUTTON   = statusValue(*LINE, "last_button");
    const auto PRESSES  = statusValue(*LINE, "button_presses");
    const auto RESIZING = statusValue(*LINE, "resizing");

    if (!LEAVE || !MOTION || !BUTTON || !PRESSES || !RESIZING)
        return std::nullopt;

    return SStatus{
        .leave              = *LEAVE,
        .motionAfterRequest = *MOTION,
        .lastButton         = static_cast<uint32_t>(*BUTTON),
        .buttonPresses      = *PRESSES,
        .resizing           = *RESIZING == 1,
    };
}

bool CClient::requestMove() {
    return sendCommand("move\n") && !!waitLineContaining("requested move");
}

bool CClient::requestResize(const std::string& edge) {
    return sendCommand(std::format("resize {}\n", edge)) && !!waitLineContaining("requested resize");
}

bool CClient::requestResizeWithSerial(uint32_t serial, const std::string& edge) {
    return sendCommand(std::format("resize-serial {} {}\n", serial, edge)) && !!waitLineContaining("requested resize");
}

bool CClient::requestRawResize(uint32_t edge) {
    return sendCommand(std::format("resize-raw {}\n", edge)) && !!waitLineContaining("requested resize", 500);
}

bool CClient::waitResizing(bool resizing) {
    return !!waitLineContaining(std::format("configure resizing={}", resizing ? 1 : 0), 2000);
}

bool CClient::waitForDisconnect(int timeoutMs) {
    const auto START = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - START).count() < timeoutMs) {
        if (disconnected)
            return true;

        readOnce(50);
    }

    return disconnected;
}

pid_t CClient::pid() const {
    return proc->pid();
}

TEST_CASE(xdgInteractive) {
    Tests::killAllWindows();

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the xdg-interactive client"); }

    ASSERT(setClientGeometry(client->pid(), SGeometry{.x = 300, .y = 300, .w = 500, .h = 400}), true);

    NLog::log("{}Testing xdg_toplevel.move", Colors::GREEN);
    const auto START_MOVE_GEOMETRY = activeGeometry();
    ASSERT(moveCursor(START_MOVE_GEOMETRY.x + 100, START_MOVE_GEOMETRY.y + 100), true);
    Tests::sync(1);
    ASSERT(click(272, true), true);
    ASSERT(client->waitButtonSerial().has_value(), true);
    ASSERT(client->requestMove(), true);
    ASSERT(moveCursor(START_MOVE_GEOMETRY.x + 220, START_MOVE_GEOMETRY.y + 180), true);

    const auto MOVE_STATUS = client->status();
    ASSERT(MOVE_STATUS.has_value(), true);
    if (MOVE_STATUS->leave < 1)
        MARK_TEST_FAILED("xdg move should clear pointer focus and send leave, got {} leave events", MOVE_STATUS->leave);
    else
        LOG_OK("xdg move sent {} leave events", MOVE_STATUS->leave);
    EXPECT(MOVE_STATUS->motionAfterRequest, 0);

    ASSERT(click(272, false), true);
    const auto END_MOVE_GEOMETRY = activeGeometry();
    EXPECT_MAX_DELTA(END_MOVE_GEOMETRY.x, START_MOVE_GEOMETRY.x + 120, 5);
    EXPECT_MAX_DELTA(END_MOVE_GEOMETRY.y, START_MOVE_GEOMETRY.y + 80, 5);
    EXPECT_MAX_DELTA(END_MOVE_GEOMETRY.w, START_MOVE_GEOMETRY.w, 2);
    EXPECT_MAX_DELTA(END_MOVE_GEOMETRY.h, START_MOVE_GEOMETRY.h, 2);

    NLog::log("{}Testing xdg_toplevel.resize right", Colors::GREEN);
    ASSERT(setClientGeometry(client->pid(), SGeometry{.x = 400, .y = 300, .w = 500, .h = 400}), true);
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "floating: 1");
    const auto START_RESIZE_GEOMETRY = activeGeometry();
    ASSERT(moveCursor(START_RESIZE_GEOMETRY.x + 250, START_RESIZE_GEOMETRY.y + 200), true);
    Tests::sync(1);
    ASSERT(click(272, true), true);
    ASSERT(client->waitButtonSerial().has_value(), true);
    ASSERT(client->requestResize("right"), true);
    ASSERT(client->waitResizing(true), true);
    ASSERT(moveCursor(START_RESIZE_GEOMETRY.x + 410, START_RESIZE_GEOMETRY.y + 200), true);

    bool resized = false;
    for (size_t i = 0; i < 50; ++i) {
        moveCursor(START_RESIZE_GEOMETRY.x + 410, START_RESIZE_GEOMETRY.y + 200);

        if (activeGeometry().w >= START_RESIZE_GEOMETRY.w + 150) {
            resized = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!resized)
        MARK_TEST_FAILED("xdg resize did not update geometry before release");

    ASSERT(click(272, false), true);
    ASSERT(client->waitResizing(false), true);

    const auto END_RESIZE_GEOMETRY = activeGeometry();
    EXPECT_MAX_DELTA(END_RESIZE_GEOMETRY.x, START_RESIZE_GEOMETRY.x, 2);
    EXPECT_MAX_DELTA(END_RESIZE_GEOMETRY.y, START_RESIZE_GEOMETRY.y, 2);
    EXPECT_MAX_DELTA(END_RESIZE_GEOMETRY.w, START_RESIZE_GEOMETRY.w + 160, 5);
    EXPECT_MAX_DELTA(END_RESIZE_GEOMETRY.h, START_RESIZE_GEOMETRY.h, 2);

    NLog::log("{}Testing stale xdg resize serial is ignored", Colors::GREEN);
    ASSERT(setClientGeometry(client->pid(), SGeometry{.x = 500, .y = 300, .w = 500, .h = 400}), true);
    const auto STALE_GEOMETRY = activeGeometry();
    ASSERT(moveCursor(STALE_GEOMETRY.x + 250, STALE_GEOMETRY.y + 200), true);
    Tests::sync(1);
    ASSERT(click(272, true), true);
    const auto STALE_SERIAL = client->waitButtonSerial();
    ASSERT(STALE_SERIAL.has_value(), true);
    ASSERT(click(272, false), true);
    ASSERT(client->requestResizeWithSerial(*STALE_SERIAL, "right"), true);
    ASSERT(moveCursor(STALE_GEOMETRY.x + 380, STALE_GEOMETRY.y + 200), true);

    const auto AFTER_STALE_GEOMETRY = activeGeometry();
    EXPECT_MAX_DELTA(AFTER_STALE_GEOMETRY.x, STALE_GEOMETRY.x, 2);
    EXPECT_MAX_DELTA(AFTER_STALE_GEOMETRY.y, STALE_GEOMETRY.y, 2);
    EXPECT_MAX_DELTA(AFTER_STALE_GEOMETRY.w, STALE_GEOMETRY.w, 2);
    EXPECT_MAX_DELTA(AFTER_STALE_GEOMETRY.h, STALE_GEOMETRY.h, 2);

    NLog::log("{}Testing invalid xdg resize edge causes protocol disconnect", Colors::GREEN);
    ASSERT(moveCursor(AFTER_STALE_GEOMETRY.x + 250, AFTER_STALE_GEOMETRY.y + 200), true);
    Tests::sync(1);
    ASSERT(click(272, true), true);
    ASSERT(client->waitButtonSerial().has_value(), true);
    ASSERT(client->requestRawResize(123), true);
    ASSERT(client->waitForDisconnect(), true);
    EXPECT_NOT(getFromSocket("/clients"), "");

    client.reset();
    Tests::killAllWindows();
}
