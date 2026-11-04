#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sys/poll.h>
#include <unistd.h>
#include <print>
#include <format>
#include <string>

#include <xcb/xcb.h>

// A plain X11 window, so tests can check which keys Xwayland considers held when `pass`
// targets an XWayland window.
//
// stdin commands:
//   held <keycode>  prints 1 if the X keycode is held according to the X server, else 0
//   mods            prints the X modifier mask (Shift, Lock, Control, Mod1-5) according to the X server
//   class <class>   sets WM_CLASS
//   exit            exits

// frees xcb replies and events, which libxcb allocates with malloc
template <typename T>
using CXCBPtr = std::unique_ptr<T, decltype([](T* p) { free(p); })>;

static xcb_connection_t* conn       = nullptr;
static xcb_window_t      window     = XCB_WINDOW_NONE;
static bool              started    = false;
static bool              shouldExit = false;

template <typename... Args>
static void clientLog(std::format_string<Args...> fmt, Args&&... args) {
    std::println("{}", std::format(fmt, std::forward<Args>(args)...));
    std::fflush(stdout);
}

static void setClass(const std::string& cls) {
    // WM_CLASS is "instance\0class\0"
    std::string value = cls;
    value += '\0';
    value += cls;
    value += '\0';
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, value.size(), value.data());
    xcb_flush(conn);
}

static bool keyHeld(uint8_t keycode) {
    const CXCBPtr<xcb_query_keymap_reply_t> REPLY{xcb_query_keymap_reply(conn, xcb_query_keymap(conn), nullptr)};
    if (!REPLY)
        return false;

    return REPLY->keys[keycode / 8] & (1U << (keycode % 8));
}

static uint16_t modifierMask() {
    const CXCBPtr<xcb_query_pointer_reply_t> REPLY{xcb_query_pointer_reply(conn, xcb_query_pointer(conn, window), nullptr)};
    if (!REPLY)
        return 0;

    return REPLY->mask & 0xFF;
}

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

static void parseRequest(const std::string& req) {
    if (req.starts_with("held ")) {
        try {
            const auto KEYCODE = std::stoul(trim(req.substr(5)));
            if (KEYCODE > UINT8_MAX)
                throw std::out_of_range("keycode");

            clientLog("{}", keyHeld(KEYCODE) ? 1 : 0);
        } catch (const std::exception&) { clientLog("error"); }
    } else if (req.starts_with("mods"))
        clientLog("{}", modifierMask());
    else if (req.starts_with("class ")) {
        setClass(trim(req.substr(6)));
        clientLog("ok");
    } else if (req.starts_with("exit"))
        shouldExit = true;
}

int main(int argc, char** argv) {
    const std::string CLASS = argc > 1 ? argv[1] : "x11-keys";

    // xcb_connect() always returns a connection that has to be disconnected, even on error
    conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(conn)) {
        clientLog("Failed to connect to X display");
        xcb_disconnect(conn);
        return -1;
    }

    xcb_screen_t*  screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    const uint32_t VALUES[] = {screen->white_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
    window                  = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 640, 480, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, VALUES);

    const std::string TITLE = "x11-keys test client";
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, TITLE.size(), TITLE.data());
    setClass(CLASS);

    xcb_map_window(conn, window);
    xcb_flush(conn);

    std::array<char, 1024> readBuf = {};

    struct pollfd          fds[2] = {{.fd = xcb_get_file_descriptor(conn), .events = POLLIN}, {.fd = STDIN_FILENO, .events = POLLIN}};
    while (!shouldExit && poll(fds, 2, -1) != -1) {
        if (fds[0].revents & POLLIN) {
            while (const CXCBPtr<xcb_generic_event_t> EV{xcb_poll_for_event(conn)}) {
                if ((EV->response_type & 0x7F) == XCB_MAP_NOTIFY && !started) {
                    started = true;
                    clientLog("started");
                }
            }

            if (xcb_connection_has_error(conn))
                break;
        }

        if (fds[1].revents & POLLIN) {
            const ssize_t BYTESREAD = read(fds[1].fd, readBuf.data(), readBuf.size() - 1);
            if (BYTESREAD <= 0)
                break;
            readBuf[BYTESREAD] = 0;

            parseRequest(std::string{readBuf.data()});
        }

        if (fds[0].revents & (POLLERR | POLLHUP) || fds[1].revents & (POLLERR | POLLHUP))
            break;
    }

    xcb_disconnect(conn);
    return 0;
}
