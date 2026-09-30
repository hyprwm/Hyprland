#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <linux/input-event-codes.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

static constexpr std::string_view SOURCE = "xdg-activation-source";
static constexpr std::string_view TARGET = "xdg-activation-target";

template <typename F>
static bool waitUntil(F&& condition) {
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
        if (condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < DEADLINE);
    return false;
}

static bool focused(std::string_view appID) {
    return Tests::getAttribute(getFromSocket("/activewindow"), "class") == appID &&
        getFromSocket(std::format("/eval hl.plugin.test.check_keyboard_focus_window('{}')", appID)) == "ok";
}

static bool focusSource() {
    return getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:^{}$' }})", SOURCE)) == "ok" && waitUntil([] { return focused(SOURCE); });
}

static bool moveCursorToSource() {
    const auto RESPONSE = getFromSocket("/activewindow");
    if (Tests::getAttribute(RESPONSE, "class") != SOURCE)
        return false;

    std::istringstream position{Tests::getAttribute(RESPONSE, "at")};
    std::istringstream size{Tests::getAttribute(RESPONSE, "size")};
    int                x = 0, y = 0, width = 0, height = 0;
    char               separator = 0;
    if (!(position >> x >> separator >> y) || separator != ',' || !(size >> width >> separator >> height) || separator != ',' || width <= 0 || height <= 0)
        return false;

    return getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x + width / 2, y + height / 2)) == "ok";
}

class CActivationButtonPress {
  public:
    ~CActivationButtonPress();
    bool press();
    bool release();

  private:
    bool m_pressed = false;
};

CActivationButtonPress::~CActivationButtonPress() {
    if (m_pressed)
        release();
}

bool CActivationButtonPress::press() {
    m_pressed = getFromSocket(std::format("/eval hl.plugin.test.click({}, 1)", BTN_LEFT)) == "ok";
    return m_pressed;
}

bool CActivationButtonPress::release() {
    if (getFromSocket(std::format("/eval hl.plugin.test.click({}, 0)", BTN_LEFT)) != "ok")
        return false;
    m_pressed = false;
    return true;
}

struct SActivationResult {
    bool emptyToken = false;
};

class CActivationClient {
  public:
    CActivationClient();
    ~CActivationClient();
    bool                             start();
    std::optional<uint32_t>          buttonSerial();
    std::optional<SActivationResult> activate(std::optional<uint32_t> serial = std::nullopt);

  private:
    bool                       sendCommand(const std::string& command);
    std::optional<std::string> waitLine(std::string_view prefix);

    CProcess                   m_process;
    CFileDescriptor            m_socket;
    std::string                m_pending;
    std::deque<std::string>    m_lines;
    uint32_t                   m_requestID = 0;
    bool                       m_started   = false;
};

CActivationClient::CActivationClient() : m_process(std::format("{}/xdg-activation", binaryDir), std::vector<std::string>{}) {
    ;
}

CActivationClient::~CActivationClient() {
    m_socket.reset();
    if (m_started)
        kill(m_process.pid(), SIGKILL);
}

bool CActivationClient::start() {
    std::array<int, 2> sockets = {
        -1,
        -1,
    };
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) < 0)
        return false;

    m_socket = CFileDescriptor{sockets[0]};
    CFileDescriptor childSocket{sockets[1]};
    auto            childOutput = childSocket.duplicate();
    if (!childOutput.isValid())
        return false;
    m_process.addEnv("WAYLAND_DISPLAY", WLDISPLAY);
    m_process.setStdinFD(childSocket.get());
    // CProcess closes each source descriptor after redirecting it.
    m_process.setStdoutFD(childOutput.get());
    m_started = m_process.runAsync();
    childSocket.reset();
    childOutput.reset();
    return m_started && waitLine("ready").has_value() && waitUntil([] {
               const auto CLIENTS = getFromSocket("/clients");
               return Tests::windowCount() == 2 && CLIENTS.contains(std::format("class: {}\n", SOURCE)) && CLIENTS.contains(std::format("class: {}\n", TARGET));
           });
}

bool CActivationClient::sendCommand(const std::string& command) {
    size_t sent = 0;
    while (sent < command.size()) {
        const auto SIZE = send(m_socket.get(), command.data() + sent, command.size() - sent, MSG_NOSIGNAL);
        if (SIZE < 0 && errno == EINTR)
            continue;
        if (SIZE <= 0)
            return false;
        sent += sc<size_t>(SIZE);
    }
    return true;
}

std::optional<std::string> CActivationClient::waitLine(std::string_view prefix) {
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < DEADLINE) {
        for (auto it = m_lines.begin(); it != m_lines.end(); ++it) {
            if (!it->starts_with(prefix))
                continue;
            auto line = *it;
            m_lines.erase(it);
            return line;
        }

        pollfd fd = {
            .fd      = m_socket.get(),
            .events  = POLLIN,
            .revents = 0,
        };
        const auto RESULT = poll(&fd, 1, 50);
        if (RESULT < 0 && errno == EINTR)
            continue;
        if (RESULT < 0)
            break;
        if (RESULT == 0)
            continue;
        if (!(fd.revents & POLLIN))
            break;

        std::array<char, 4096> buffer{};
        const auto             SIZE = read(m_socket.get(), buffer.data(), buffer.size());
        if (SIZE < 0 && errno == EINTR)
            continue;
        if (SIZE <= 0)
            break;
        m_pending.append(buffer.data(), sc<size_t>(SIZE));
        for (auto end = m_pending.find('\n'); end != std::string::npos; end = m_pending.find('\n')) {
            m_lines.push_back(m_pending.substr(0, end));
            m_pending.erase(0, end + 1);
        }
    }

    NLog::red("xdg-activation: did not receive '{}' (pending output: '{}')", prefix, m_pending);
    return std::nullopt;
}

std::optional<uint32_t> CActivationClient::buttonSerial() {
    const auto LINE = waitLine("button ");
    if (!LINE)
        return std::nullopt;

    uint32_t   serial = 0;
    const auto RESULT = std::from_chars(LINE->data() + std::string_view("button ").size(), LINE->data() + LINE->size(), serial);
    if (RESULT.ec != std::errc{} || RESULT.ptr != LINE->data() + LINE->size() || serial == 0)
        return std::nullopt;
    return serial;
}

std::optional<SActivationResult> CActivationClient::activate(std::optional<uint32_t> serial) {
    const auto ID = ++m_requestID;
    if (!sendCommand(std::format("activate {} {}\n", ID, serial ? std::format("serial {}", *serial) : "missing")))
        return std::nullopt;

    // The helper acknowledges only after the activate request's Wayland sync.
    const auto PREFIX = std::format("activated {} empty=", ID);
    const auto LINE   = waitLine(PREFIX);
    if (LINE == PREFIX + "1")
        return SActivationResult{
            .emptyToken = true,
        };
    if (LINE == PREFIX + "0")
        return SActivationResult{
            .emptyToken = false,
        };
    return std::nullopt;
}

TEST_CASE(xdgActivationSerial) {
    OK(getFromSocket("/eval hl.config({ misc = { focus_on_activate = true }, input = { follow_mouse = 0 } })"));

    CActivationClient client;
    ASSERT(client.start(), true);
    ASSERT(focusSource(), true);

    NLog::green("Testing activation without set_serial");
    const auto MISSING = client.activate();
    ASSERT(MISSING.has_value(), true);
    EXPECT(focused(SOURCE), true);
    EXPECT(MISSING->emptyToken, true);
    EXPECT(Tests::windowCount(), 2);

    NLog::green("Testing activation with a real pointer-button serial");
    ASSERT(focusSource(), true);
    ASSERT(moveCursorToSource(), true);
    CActivationButtonPress button;
    ASSERT(button.press(), true);
    const auto SERIAL = client.buttonSerial();
    ASSERT(SERIAL.has_value(), true);
    ASSERT(button.release(), true);
    ASSERT(focused(SOURCE), true);

    const auto VALID = client.activate(*SERIAL);
    ASSERT(VALID.has_value(), true);
    EXPECT(VALID->emptyToken, false);
    ASSERT(waitUntil([] { return focused(TARGET); }), true);

    NLog::green("Testing activation with an already-consumed serial");
    ASSERT(focusSource(), true);
    const auto CONSUMED = client.activate(*SERIAL);
    ASSERT(CONSUMED.has_value(), true);
    EXPECT(focused(SOURCE), true);
    EXPECT(CONSUMED->emptyToken, true);
    EXPECT(Tests::windowCount(), 2);
}
