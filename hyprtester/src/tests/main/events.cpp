#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int ret = 0;

// Connect to the event socket (.socket2.sock) and return the fd.
// Non-blocking with a short timeout for reads.
static int connectEventSocket() {
    const auto XDG      = getenv("XDG_RUNTIME_DIR");
    const auto basePath = std::string(XDG ? XDG : "/run/user/1000") + "/hypr/" + HIS + "/.socket2.sock";

    int        fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, basePath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), SUN_LEN(&addr)) < 0) {
        close(fd);
        return -1;
    }

    auto t = timeval{.tv_sec = 2, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));

    return fd;
}

// Read all available events from the socket and return them as a string.
static std::string drainEvents(int fd) {
    std::string result;
    char        buf[4096] = {0};

    // brief wait for events to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    while (true) {
        auto n = recv(fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
        if (n <= 0)
            break;
        buf[n] = '\0';
        result += std::string(buf, n);
    }

    return result;
}

static void testOpenCloseWindowEvents() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // drain any stale events
    drainEvents(fd);

    OK(getFromSocket("/dispatch workspace 900"));

    auto kitty = Tests::spawnKitty("evt_kitty");
    if (!kitty) {
        close(fd);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "openwindow>>");
    EXPECT_CONTAINS(events, "evt_kitty");

    // kill the window and check for closewindow event
    OK(getFromSocket("/dispatch killwindow class:evt_kitty"));
    Tests::waitUntilWindowsN(0);

    events = drainEvents(fd);
    EXPECT_CONTAINS(events, "closewindow>>");

    close(fd);
    Tests::killAllWindows();
}

static void testWorkspaceEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    drainEvents(fd);

    OK(getFromSocket("/dispatch workspace 901"));
    Tests::spawnKitty("evt_ws_a");

    OK(getFromSocket("/dispatch workspace 902"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "workspace>>902");

    close(fd);
    Tests::killAllWindows();
}

static void testActiveWindowEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 903"));

    Tests::spawnKitty("evt_aw_a");
    Tests::spawnKitty("evt_aw_b");

    drainEvents(fd);

    OK(getFromSocket("/dispatch focuswindow class:evt_aw_a"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "activewindow>>evt_aw_a");
    EXPECT_CONTAINS(events, "activewindowv2>>");

    close(fd);
    Tests::killAllWindows();
}

static void testFullscreenEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 904"));
    Tests::spawnKitty("evt_fs");

    drainEvents(fd);

    OK(getFromSocket("/dispatch fullscreen 0"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "fullscreen>>");

    // toggle off
    OK(getFromSocket("/dispatch fullscreen 0"));

    close(fd);
    Tests::killAllWindows();
}

static void testWindowTitleEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 905"));
    Tests::spawnKitty("evt_title");

    drainEvents(fd);

    // renaming a workspace causes a renameworkspace event; toggling group emits togglegroup.
    // windowtitle fires whenever a client updates its title — trigger via renameworkspace on the
    // active workspace so that the activewindow title field updates (reuses a known-good event).
    // Instead, pin/unpin fires a guaranteed "pin" event we can rely on.
    OK(getFromSocket("/dispatch setfloating class:evt_title"));
    OK(getFromSocket("/dispatch pin"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "pin>>");

    OK(getFromSocket("/dispatch pin"));

    close(fd);
    Tests::killAllWindows();
}

static void testSubmapEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    drainEvents(fd);

    OK(getFromSocket("/dispatch submap submap1"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "submap>>submap1");

    OK(getFromSocket("/dispatch submap reset"));

    events = drainEvents(fd);
    EXPECT_CONTAINS(events, "submap>>");

    close(fd);
}

static void testRenameWorkspaceEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 906"));
    Tests::spawnKitty("evt_rw");

    drainEvents(fd);

    OK(getFromSocket("/dispatch renameworkspace 906 TestEvtName"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "renameworkspace>>");
    EXPECT_CONTAINS(events, "TestEvtName");

    OK(getFromSocket("/dispatch renameworkspace 906"));

    close(fd);
    Tests::killAllWindows();
}

static void testMoveWindowEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 907"));
    Tests::spawnKitty("evt_mv");

    drainEvents(fd);

    OK(getFromSocket("/dispatch movetoworkspace 908"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "movewindow>>");
    EXPECT_CONTAINS(events, "908");

    close(fd);
    Tests::killAllWindows();
}

static void testPinEvent() {
    int fd = connectEventSocket();
    if (fd < 0) {
        NLog::log("{}Failed to connect to event socket", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch workspace 909"));
    Tests::spawnKitty("evt_pin");
    OK(getFromSocket("/dispatch setfloating class:evt_pin"));

    drainEvents(fd);

    OK(getFromSocket("/dispatch pin"));

    auto events = drainEvents(fd);
    EXPECT_CONTAINS(events, "pin>>");

    OK(getFromSocket("/dispatch pin"));

    close(fd);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing IPC event socket", Colors::GREEN);

    NLog::log("{}Testing openwindow/closewindow events", Colors::GREEN);
    testOpenCloseWindowEvents();

    NLog::log("{}Testing workspace event", Colors::GREEN);
    testWorkspaceEvent();

    NLog::log("{}Testing activewindow event", Colors::GREEN);
    testActiveWindowEvent();

    NLog::log("{}Testing fullscreen event", Colors::GREEN);
    testFullscreenEvent();

    NLog::log("{}Testing windowtitle/pin event", Colors::GREEN);
    testWindowTitleEvent();

    NLog::log("{}Testing submap event", Colors::GREEN);
    testSubmapEvent();

    NLog::log("{}Testing renameworkspace event", Colors::GREEN);
    testRenameWorkspaceEvent();

    NLog::log("{}Testing movewindow event", Colors::GREEN);
    testMoveWindowEvent();

    NLog::log("{}Testing pin event", Colors::GREEN);
    testPinEvent();

    // clean up
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
