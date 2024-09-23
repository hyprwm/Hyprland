#include "Eis.hpp"

#include "Compositor.hpp"
#include "devices/IKeyboard.hpp"
#include "helpers/Monitor.hpp"
#include "managers/SeatManager.hpp"
#include <alloca.h>
#include <cstdint>
#include <hyprutils/os/FileDescriptor.hpp>
#include <libeis.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-core.h>

CEis::CEis(std::string socketName) {
    Log::logger->log(Log::INFO, "[EIS] Init socket: {}", socketName);

    const char* xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg)
        socketPath = std::string(xdg) + "/" + socketName;

    if (socketPath.empty()) {
        Log::logger->log(Log::ERR, "[EIS] Socket path is empty");
        return;
    }

    m_eisCtx = eis_new(nullptr);

    if (eis_setup_backend_socket(m_eisCtx, socketPath.c_str())) {
        Log::logger->log(Log::ERR, "[EIS] Cannot init eis socket on {}", socketPath);
        return;
    }
    Log::logger->log(Log::INFO, "[EIS] Listening on {}", socketPath);

    m_eventSource = wl_event_loop_add_fd(
        g_pCompositor->m_wlEventLoop, eis_get_fd(m_eisCtx), WL_EVENT_READABLE, [](int fd, uint32_t mask, void* data) { return ((CEis*)data)->pollEvents(); }, this);
}

CEis::~CEis() {
    wl_event_source_remove(m_eventSource);
    Log::logger->log(Log::INFO, "[EIS] Server fd {} destroyed", eis_get_fd(m_eisCtx));
    eis_unref(m_eisCtx);
}

int CEis::pollEvents() {
    eis_dispatch(m_eisCtx);

    //Pull every available events
    while (true) {
        eis_event* e = eis_get_event(m_eisCtx);

        if (!e) {
            eis_event_unref(e);
            break;
        }

        int rc = onEvent(e);
        eis_event_unref(e);
        if (rc != 0)
            break;
    }

    return 0;
}

int CEis::onEvent(eis_event* e) {
    eis_client* eisClient = nullptr;
    eis_seat*   seat      = nullptr;
    eis_device* device    = nullptr;

    switch (eis_event_get_type(e)) {
        case EIS_EVENT_CLIENT_CONNECT:
            eisClient = eis_event_get_client(e);
            Log::logger->log(Log::INFO, "[EIS] {} client connected: {}", eis_client_is_sender(eisClient) ? "Sender" : "Receiver", eis_client_get_name(eisClient));

            if (eis_client_is_sender(eisClient)) {
                Log::logger->log(Log::WARN, "[EIS] Unexpected sender client {} connected to input capture session", eis_client_get_name(eisClient));
                eis_client_disconnect(eisClient);
                return 0;
            }

            if (m_client.handle) {
                Log::logger->log(Log::WARN, "[EIS] Unexpected additional client {} connected to input capture session", eis_client_get_name(eisClient));
                eis_client_disconnect(eisClient);
                return 0;
            }

            m_client.handle = eisClient;

            eis_client_connect(eisClient);
            Log::logger->log(Log::INFO, "[EIS] Creating new default seat");
            seat = eis_client_new_seat(eisClient, "default");

            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_BUTTON);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_SCROLL);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_KEYBOARD);
            eis_seat_add(seat);
            m_client.seat = seat;
            break;
        case EIS_EVENT_CLIENT_DISCONNECT:
            eisClient = eis_event_get_client(e);
            Log::logger->log(Log::INFO, "[EIS] {} disconnected", eis_client_get_name(eisClient));
            eis_client_disconnect(eisClient);

            eis_seat_unref(m_client.seat);
            clearPointer();
            clearKeyboard();
            m_client.handle = nullptr;
            break;
        case EIS_EVENT_SEAT_BIND:
            Log::logger->log(Log::INFO, "[EIS] Binding seats...");

            if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_POINTER) && eis_event_seat_has_capability(e, EIS_DEVICE_CAP_BUTTON) &&
                eis_event_seat_has_capability(e, EIS_DEVICE_CAP_SCROLL))
                ensurePointer();
            else
                clearPointer();

            if (eis_event_seat_has_capability(e, EIS_DEVICE_CAP_KEYBOARD))
                ensureKeyboard();
            else
                clearKeyboard();
            break;
        case EIS_EVENT_DEVICE_CLOSED:
            device = eis_event_get_device(e);
            if (device == m_client.pointer)
                clearPointer();
            else if (device == m_client.keyboard) {
                clearKeyboard();
            } else
                Log::logger->log(Log::WARN, "[EIS] Unknown device to close");
            break;
        default: return 0;
    }
    return 0;
}

void CEis::ensurePointer() {
    if (m_client.pointer)
        return;

    Log::logger->log(Log::INFO, "[EIS] Creating pointer");
    eis_device* pointer = eis_seat_new_device(m_client.seat);
    eis_device_configure_name(pointer, "captured relative pointer");
    eis_device_configure_capability(pointer, EIS_DEVICE_CAP_POINTER);
    eis_device_configure_capability(pointer, EIS_DEVICE_CAP_BUTTON);
    eis_device_configure_capability(pointer, EIS_DEVICE_CAP_SCROLL);

    for (auto& o : g_pCompositor->m_monitors) {
        eis_region* r = eis_device_new_region(pointer);

        eis_region_set_offset(r, o->m_position.x, o->m_position.y);
        eis_region_set_size(r, o->m_pixelSize.x, o->m_pixelSize.y);
        eis_region_set_physical_scale(r, o->m_scale);
        eis_region_add(r);
        eis_region_unref(r);
    }

    eis_device_add(pointer);
    eis_device_resume(pointer);

    m_client.pointer = pointer;
}

void CEis::ensureKeyboard() {
    if (m_client.keyboard)
        return;

    Log::logger->log(Log::INFO, "[EIS] Creating keyboard");
    eis_device* keyboard = eis_seat_new_device(m_client.seat);
    eis_device_configure_name(keyboard, "captured keyboard");
    eis_device_configure_capability(keyboard, EIS_DEVICE_CAP_KEYBOARD);

    SKeymap _keymap = getKeymap();
    if (_keymap.fd != -1) {
        Log::logger->log(Log::INFO, "[EIS] Using keymap {}", _keymap.fd, _keymap.size);
        eis_keymap* eis_keymap = eis_device_new_keymap(keyboard, EIS_KEYMAP_TYPE_XKB, _keymap.fd, _keymap.size);
        if (eis_keymap) {
            eis_keymap_add(eis_keymap);
            eis_keymap_unref(eis_keymap);
        } else {
            Log::logger->log(Log::INFO, "[EIS] Cannot open keymap");
        }
    }

    eis_device_add(keyboard);
    eis_device_resume(keyboard);

    m_client.keyboard = keyboard;
}

SKeymap CEis::getKeymap() {
    SKeymap       _keymap = {.fd = -1, .size = -1};

    SP<IKeyboard> keyboard = g_pSeatManager->m_keyboard.lock();
    if (!keyboard)
        return _keymap;

    int32_t size = keyboard->m_xkbKeymapString.length() + 1;
    int     fd   = keyboard->m_xkbKeymapFD.get();
    _keymap.size = size;
    _keymap.fd   = fd;
    return _keymap;
}

void CEis::clearPointer() {
    if (!m_client.pointer)
        return;
    Log::logger->log(Log::INFO, "[EIS] Clearing pointer");

    eis_device_remove(m_client.pointer);
    eis_device_unref(m_client.pointer);
    m_client.pointer = nullptr;
}

void CEis::clearKeyboard() {
    if (!m_client.keyboard)
        return;
    Log::logger->log(Log::INFO, "[EIS] Clearing keyboard");

    eis_device_remove(m_client.keyboard);
    eis_device_unref(m_client.keyboard);
    m_client.keyboard = nullptr;
}

int CEis::getFileDescriptor() {
    return eis_backend_fd_add_client(m_eisCtx);
}

void CEis::startEmulating(int sequence) {
    Log::logger->log(Log::INFO, "[EIS] Start Emulating");

    if (m_client.pointer)
        eis_device_start_emulating(m_client.pointer, sequence);

    if (m_client.keyboard)
        eis_device_start_emulating(m_client.keyboard, sequence);
}

void CEis::stopEmulating() {
    Log::logger->log(Log::INFO, "[EIS] Stop Emulating");

    if (m_client.pointer)
        eis_device_stop_emulating(m_client.pointer);

    if (m_client.keyboard)
        eis_device_stop_emulating(m_client.keyboard);
}

void CEis::resetKeyboard() {
    if (!m_client.keyboard) //We don't re-create the keyboard if it doesn't exist
        return;
    clearKeyboard();
    ensureKeyboard();
}

void CEis::resetPointer() {
    if (!m_client.pointer) //We don't re-create the pointer if it doesn't exist
        return;
    clearPointer();
    ensurePointer();
}

void CEis::sendMotion(double x, double y) {
    if (!m_client.pointer)
        return;
    eis_device_pointer_motion(m_client.pointer, x, y);
}

void CEis::sendKey(uint32_t key, bool pressed) {
    if (!m_client.keyboard)
        return;
    uint64_t now = eis_now(m_eisCtx);
    eis_device_keyboard_key(m_client.keyboard, key, pressed);
    eis_device_frame(m_client.keyboard, now);
}

void CEis::sendModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group) {
    if (!m_client.keyboard)
        return;
    uint64_t now = eis_now(m_eisCtx);
    eis_device_keyboard_send_xkb_modifiers(m_client.keyboard, modsDepressed, modsLatched, modsLocked, group);
    eis_device_frame(m_client.keyboard, now);
}

void CEis::sendButton(uint32_t button, bool pressed) {
    if (!m_client.pointer)
        return;
    eis_device_button_button(m_client.pointer, button, pressed);
}

void CEis::sendScrollDiscrete(int32_t x, int32_t y) {
    if (!m_client.pointer)
        return;
    eis_device_scroll_discrete(m_client.pointer, x, y);
}

void CEis::sendScrollDelta(double x, double y) {
    if (!m_client.pointer)
        return;
    eis_device_scroll_delta(m_client.pointer, x, y);
}

void CEis::sendScrollStop(bool x, bool y) {
    if (!m_client.pointer)
        return;
    eis_device_scroll_stop(m_client.pointer, x, y);
}

void CEis::sendPointerFrame() {
    if (!m_client.pointer)
        return;
    uint64_t now = eis_now(m_eisCtx);
    eis_device_frame(m_client.pointer, now);
}
