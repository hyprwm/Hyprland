#include "Seat.hpp"
#include "Compositor.hpp"
#include "DataDevice.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../devices/IHID.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../helpers/time/Time.hpp"
#include "../../config/ConfigValue.hpp"
#include <algorithm>

#include <fcntl.h>

CWLTouchResource::CWLTouchResource(SP<CWlTouch> resource_, SP<CWLSeatResource> owner_) : m_owner(owner_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](CWlTouch* r) { PROTO::seat->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlTouch* r) { PROTO::seat->destroyResource(this); });
}

bool CWLTouchResource::good() {
    return m_resource->resource();
}

void CWLTouchResource::sendDown(SP<CWLSurfaceResource> surface, uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!m_owner || !surface || !surface->getResource()->resource())
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    ASSERT(surface->client() == m_owner->client());

    m_currentSurface           = surface;
    m_listeners.destroySurface = surface->m_events.destroy.listen([this, timeMs, id] { sendUp(timeMs + 10 /* hack */, id); });

    m_resource->sendDown(g_pSeatManager->nextSerial(m_owner.lock()), timeMs, surface->getResource().get(), id, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));

    m_fingers++;
}

void CWLTouchResource::sendUp(uint32_t timeMs, int32_t id) {
    if (!m_owner)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendUp(g_pSeatManager->nextSerial(m_owner.lock()), timeMs, id);
    m_fingers--;
    if (m_fingers <= 0) {
        m_currentSurface.reset();
        m_listeners.destroySurface.reset();
        m_fingers = 0;
    }
}

void CWLTouchResource::sendMotion(uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!m_owner)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendMotion(timeMs, id, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLTouchResource::sendFrame() {
    if (!m_owner)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendFrame();
}

void CWLTouchResource::sendCancel() {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendCancel();
}

void CWLTouchResource::sendShape(int32_t id, const Vector2D& shape) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 6)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendShape(id, wl_fixed_from_double(shape.x), wl_fixed_from_double(shape.y));
}

void CWLTouchResource::sendOrientation(int32_t id, double angle) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 6)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    m_resource->sendOrientation(id, wl_fixed_from_double(angle));
}

CWLPointerResource::CWLPointerResource(SP<CWlPointer> resource_, SP<CWLSeatResource> owner_) : m_owner(owner_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setRelease([this](CWlPointer* r) { PROTO::seat->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlPointer* r) { PROTO::seat->destroyResource(this); });

    m_resource->setSetCursor([this](CWlPointer* r, uint32_t serial, wl_resource* surf, int32_t hotX, int32_t hotY) {
        if (!m_owner) {
            LOGM(ERR, "Client bug: setCursor when seatClient is already dead");
            return;
        }

        auto surfResource = surf ? CWLSurfaceResource::fromResource(surf) : nullptr;

        if (surfResource && surfResource->m_role->role() != SURFACE_ROLE_CURSOR && surfResource->m_role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Cursor surface already has a different role");
            return;
        }

        if (surfResource && surfResource->m_role->role() != SURFACE_ROLE_CURSOR) {
            surfResource->m_role = makeShared<CCursorSurfaceRole>();
            surfResource->updateCursorShm();
        }

        g_pSeatManager->onSetCursor(m_owner.lock(), serial, surfResource, {hotX, hotY});
    });

    if (g_pSeatManager->m_state.pointerFocus && g_pSeatManager->m_state.pointerFocus->client() == m_resource->client())
        sendEnter(g_pSeatManager->m_state.pointerFocus.lock(), {-1, -1} /* Coords don't really matter that much, they will be updated next move */);
}

int CWLPointerResource::version() {
    return m_resource->version();
}

bool CWLPointerResource::good() {
    return m_resource->resource();
}

SP<CWLPointerResource> CWLPointerResource::fromResource(wl_resource* res) {
    auto data = sc<CWLPointerResource*>(sc<CWlPointer*>(wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

void CWLPointerResource::sendEnter(SP<CWLSurfaceResource> surface, const Vector2D& local) {
    if (!m_owner || m_currentSurface == surface || !surface->getResource()->resource())
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    if (m_currentSurface) {
        LOGM(WARN, "requested CWLPointerResource::sendEnter without sendLeave first.");
        sendLeave();
    }

    ASSERT(surface->client() == m_owner->client());

    m_currentSurface           = surface;
    m_listeners.destroySurface = surface->m_events.destroy.listen([this] { sendLeave(); });

    m_resource->sendEnter(g_pSeatManager->nextSerial(m_owner.lock()), surface->getResource().get(), wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLPointerResource::sendLeave() {
    if (!m_owner || !m_currentSurface || !m_currentSurface->getResource()->resource())
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    // release all buttons unless we have a dnd going on in which case
    // the events shall be lost.
    if (!PROTO::data->dndActive()) {
        for (auto const& b : m_pressedButtons) {
            sendButton(Time::millis(Time::steadyNow()), b, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    m_pressedButtons.clear();

    m_resource->sendLeave(g_pSeatManager->nextSerial(m_owner.lock()), m_currentSurface->getResource().get());
    m_currentSurface.reset();
    m_listeners.destroySurface.reset();
}

void CWLPointerResource::sendMotion(uint32_t timeMs, const Vector2D& local) {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendMotion(timeMs, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLPointerResource::sendButton(uint32_t timeMs, uint32_t button, wl_pointer_button_state state) {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    if (state == WL_POINTER_BUTTON_STATE_RELEASED && std::ranges::find(m_pressedButtons, button) == m_pressedButtons.end()) {
        LOGM(ERR, "sendButton release on a non-pressed button");
        return;
    } else if (state == WL_POINTER_BUTTON_STATE_PRESSED && std::ranges::find(m_pressedButtons, button) != m_pressedButtons.end()) {
        LOGM(ERR, "sendButton press on a non-pressed button");
        return;
    }

    if (state == WL_POINTER_BUTTON_STATE_RELEASED)
        std::erase(m_pressedButtons, button);
    else if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        m_pressedButtons.emplace_back(button);

    m_resource->sendButton(g_pSeatManager->nextSerial(m_owner.lock()), timeMs, button, state);
}

void CWLPointerResource::sendAxis(uint32_t timeMs, wl_pointer_axis axis, double value) {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxis(timeMs, axis, wl_fixed_from_double(value));
}

void CWLPointerResource::sendFrame() {
    if (!m_owner || m_resource->version() < 5)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendFrame();
}

void CWLPointerResource::sendAxisSource(wl_pointer_axis_source source) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 5)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxisSource(source);
}

void CWLPointerResource::sendAxisStop(uint32_t timeMs, wl_pointer_axis axis) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 5)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxisStop(timeMs, axis);
}

void CWLPointerResource::sendAxisDiscrete(wl_pointer_axis axis, int32_t discrete) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 5)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxisDiscrete(axis, discrete);
}

void CWLPointerResource::sendAxisValue120(wl_pointer_axis axis, int32_t value120) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 8)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxisValue120(axis, value120);
}

void CWLPointerResource::sendAxisRelativeDirection(wl_pointer_axis axis, wl_pointer_axis_relative_direction direction) {
    if (!m_owner || !m_currentSurface || m_resource->version() < 9)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    m_resource->sendAxisRelativeDirection(axis, direction);
}

CWLKeyboardResource::CWLKeyboardResource(SP<CWlKeyboard> resource_, SP<CWLSeatResource> owner_) : m_owner(owner_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](CWlKeyboard* r) { PROTO::seat->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlKeyboard* r) { PROTO::seat->destroyResource(this); });

    if (!g_pSeatManager->m_keyboard) {
        LOGM(ERR, "No keyboard on bound wl_keyboard??");
        return;
    }

    sendKeymap(g_pSeatManager->m_keyboard.lock());
    repeatInfo(g_pSeatManager->m_keyboard->m_repeatRate, g_pSeatManager->m_keyboard->m_repeatDelay);

    if (g_pSeatManager->m_state.keyboardFocus && g_pSeatManager->m_state.keyboardFocus->client() == m_resource->client()) {
        wl_array keys;
        wl_array_init(&keys);

        sendEnter(g_pSeatManager->m_state.keyboardFocus.lock(), &keys);

        wl_array_release(&keys);
    }
}

bool CWLKeyboardResource::good() {
    return m_resource->resource();
}

void CWLKeyboardResource::sendKeymap(SP<IKeyboard> keyboard) {
    if (!keyboard)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    std::string_view                keymap = keyboard->m_xkbKeymapV1String;
    Hyprutils::OS::CFileDescriptor& fd     = keyboard->m_xkbKeymapV1FD;
    uint32_t                        size   = keyboard->m_xkbKeymapV1String.length() + 1;

    if (keymap == m_lastKeymap)
        return;

    m_lastKeymap = keymap;

    const wl_keyboard_keymap_format format = keyboard ? WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 : WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP;

    m_resource->sendKeymap(format, fd.get(), size);
}

void CWLKeyboardResource::sendEnter(SP<CWLSurfaceResource> surface, wl_array* keys) {
    ASSERT(keys);

    if (!m_owner || m_currentSurface == surface || !surface->getResource()->resource())
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    if (m_currentSurface) {
        LOGM(WARN, "requested CWLKeyboardResource::sendEnter without sendLeave first.");
        sendLeave();
    }

    ASSERT(surface->client() == m_owner->client());

    m_currentSurface           = surface;
    m_listeners.destroySurface = surface->m_events.destroy.listen([this] { sendLeave(); });

    m_resource->sendEnter(g_pSeatManager->nextSerial(m_owner.lock()), surface->getResource().get(), keys);
}

void CWLKeyboardResource::sendLeave() {
    if (!m_owner || !m_currentSurface || !m_currentSurface->getResource()->resource())
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    m_resource->sendLeave(g_pSeatManager->nextSerial(m_owner.lock()), m_currentSurface->getResource().get());
    m_currentSurface.reset();
    m_listeners.destroySurface.reset();
}

void CWLKeyboardResource::sendKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state) {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    m_resource->sendKey(g_pSeatManager->nextSerial(m_owner.lock()), timeMs, key, state);
}

void CWLKeyboardResource::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!m_owner || !m_currentSurface)
        return;

    if (!(PROTO::seat->m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    m_resource->sendModifiers(g_pSeatManager->nextSerial(m_owner.lock()), depressed, latched, locked, group);
}

void CWLKeyboardResource::repeatInfo(uint32_t rate, uint32_t delayMs) {
    if (!m_owner || m_resource->version() < 4 || (rate == m_lastRate && delayMs == m_lastDelayMs))
        return;
    m_lastRate    = rate;
    m_lastDelayMs = delayMs;

    m_resource->sendRepeatInfo(rate, delayMs);
}

CWLSeatResource::CWLSeatResource(SP<CWlSeat> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlSeat* r) {
        m_events.destroy.emit();
        PROTO::seat->destroyResource(this);
    });
    m_resource->setRelease([this](CWlSeat* r) {
        m_events.destroy.emit();
        PROTO::seat->destroyResource(this);
    });

    m_client = m_resource->client();

    m_resource->setGetKeyboard([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_keyboards.emplace_back(makeShared<CWLKeyboardResource>(makeShared<CWlKeyboard>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_keyboards.pop_back();
            return;
        }

        m_keyboards.emplace_back(RESOURCE);
    });

    m_resource->setGetPointer([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_pointers.emplace_back(makeShared<CWLPointerResource>(makeShared<CWlPointer>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_pointers.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        m_pointers.emplace_back(RESOURCE);
    });

    m_resource->setGetTouch([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_touches.emplace_back(makeShared<CWLTouchResource>(makeShared<CWlTouch>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_touches.pop_back();
            return;
        }

        m_touches.emplace_back(RESOURCE);
    });

    if (m_resource->version() >= 2)
        m_resource->sendName(HL_SEAT_NAME);

    sendCapabilities(PROTO::seat->m_currentCaps);
}

CWLSeatResource::~CWLSeatResource() {
    m_events.destroy.emit();
}

void CWLSeatResource::sendCapabilities(uint32_t caps) {
    uint32_t wlCaps = 0;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD)
        wlCaps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER)
        wlCaps |= WL_SEAT_CAPABILITY_POINTER;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH)
        wlCaps |= WL_SEAT_CAPABILITY_TOUCH;

    m_resource->sendCapabilities(sc<wl_seat_capability>(wlCaps));
}

bool CWLSeatResource::good() {
    return m_resource->resource();
}

wl_client* CWLSeatResource::client() {
    return m_client;
}

CWLSeatProtocol::CWLSeatProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSeatProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_seatResources.emplace_back(makeShared<CWLSeatResource>(makeShared<CWlSeat>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_seatResources.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;

    LOGM(LOG, "New seat resource bound at {:x}", (uintptr_t)RESOURCE.get());

    m_events.newSeatResource.emit(RESOURCE);
}

void CWLSeatProtocol::destroyResource(CWLSeatResource* seat) {
    std::erase_if(m_seatResources, [&](const auto& other) { return other.get() == seat; });
}

void CWLSeatProtocol::destroyResource(CWLKeyboardResource* resource) {
    std::erase_if(m_keyboards, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::destroyResource(CWLPointerResource* resource) {
    std::erase_if(m_pointers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::destroyResource(CWLTouchResource* resource) {
    std::erase_if(m_touches, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::updateCapabilities(uint32_t caps) {
    if (caps == m_currentCaps)
        return;

    m_currentCaps = caps;

    for (auto const& s : m_seatResources) {
        s->sendCapabilities(caps);
    }
}

void CWLSeatProtocol::updateKeymap() {
    if (!(m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    for (auto const& k : m_keyboards) {
        k->sendKeymap(g_pSeatManager->m_keyboard.lock());
    }
}

void CWLSeatProtocol::updateRepeatInfo(uint32_t rate, uint32_t delayMs) {
    if (!(m_currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    for (auto const& k : m_keyboards) {
        k->repeatInfo(rate, delayMs);
    }
}

SP<CWLSeatResource> CWLSeatProtocol::seatResourceForClient(wl_client* client) {
    for (auto const& r : m_seatResources) {
        if (r->client() == client)
            return r;
    }

    return nullptr;
}

std::vector<uint8_t>& CCursorSurfaceRole::cursorPixelData(SP<CWLSurfaceResource> surface) {
    RASSERT(surface->m_role->role() == SURFACE_ROLE_CURSOR, "cursorPixelData called on a non-cursor surface");

    auto role = sc<CCursorSurfaceRole*>(surface->m_role.get());
    return role->m_cursorShmPixelData;
}
