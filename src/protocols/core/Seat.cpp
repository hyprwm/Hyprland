#include "Seat.hpp"
#include "Compositor.hpp"
#include "DataDevice.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../devices/IHID.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../config/ConfigValue.hpp"
#include <algorithm>

#include <fcntl.h>

CWLTouchResource::CWLTouchResource(SP<CWlTouch> resource_, SP<CWLSeatResource> owner_) : owner(owner_), resource(resource_) {
    if (!good())
        return;

    resource->setRelease([this](CWlTouch* r) { PROTO::seat->destroyResource(this); });
    resource->setOnDestroy([this](CWlTouch* r) { PROTO::seat->destroyResource(this); });
}

bool CWLTouchResource::good() {
    return resource->resource();
}

void CWLTouchResource::sendDown(SP<CWLSurfaceResource> surface, uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!owner)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    ASSERT(surface->client() == owner->client());

    currentSurface           = surface;
    listeners.destroySurface = surface->events.destroy.registerListener([this, timeMs, id](std::any d) { sendUp(timeMs + 10 /* hack */, id); });

    resource->sendDown(g_pSeatManager->nextSerial(owner.lock()), timeMs, surface->getResource().get(), id, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));

    fingers++;
}

void CWLTouchResource::sendUp(uint32_t timeMs, int32_t id) {
    if (!owner)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendUp(g_pSeatManager->nextSerial(owner.lock()), timeMs, id);
    fingers--;
    if (fingers <= 0) {
        currentSurface.reset();
        listeners.destroySurface.reset();
        fingers = 0;
    }
}

void CWLTouchResource::sendMotion(uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!owner)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendMotion(timeMs, id, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLTouchResource::sendFrame() {
    if (!owner)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendFrame();
}

void CWLTouchResource::sendCancel() {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendCancel();
}

void CWLTouchResource::sendShape(int32_t id, const Vector2D& shape) {
    if (!owner || !currentSurface || resource->version() < 6)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendShape(id, wl_fixed_from_double(shape.x), wl_fixed_from_double(shape.y));
}

void CWLTouchResource::sendOrientation(int32_t id, double angle) {
    if (!owner || !currentSurface || resource->version() < 6)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH))
        return;

    resource->sendOrientation(id, wl_fixed_from_double(angle));
}

CWLPointerResource::CWLPointerResource(SP<CWlPointer> resource_, SP<CWLSeatResource> owner_) : owner(owner_), resource(resource_) {
    if (!good())
        return;

    resource->setRelease([this](CWlPointer* r) { PROTO::seat->destroyResource(this); });
    resource->setOnDestroy([this](CWlPointer* r) { PROTO::seat->destroyResource(this); });

    resource->setSetCursor([this](CWlPointer* r, uint32_t serial, wl_resource* surf, int32_t hotX, int32_t hotY) {
        if (!owner) {
            LOGM(ERR, "Client bug: setCursor when seatClient is already dead");
            return;
        }

        auto surfResource = surf ? CWLSurfaceResource::fromResource(surf) : nullptr;

        if (surfResource && surfResource->role->role() != SURFACE_ROLE_CURSOR && surfResource->role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Cursor surface already has a different role");
            return;
        }

        if (surfResource) {
            surfResource->role = makeShared<CCursorSurfaceRole>();
            surfResource->updateCursorShm();
        }

        g_pSeatManager->onSetCursor(owner.lock(), serial, surfResource, {hotX, hotY});
    });

    if (g_pSeatManager->state.pointerFocus && g_pSeatManager->state.pointerFocus->client() == resource->client())
        sendEnter(g_pSeatManager->state.pointerFocus.lock(), {-1, -1} /* Coords don't really matter that much, they will be updated next move */);
}

bool CWLPointerResource::good() {
    return resource->resource();
}

void CWLPointerResource::sendEnter(SP<CWLSurfaceResource> surface, const Vector2D& local) {
    if (!owner || currentSurface == surface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    if (currentSurface) {
        LOGM(WARN, "requested CWLPointerResource::sendEnter without sendLeave first.");
        sendLeave();
    }

    ASSERT(surface->client() == owner->client());

    currentSurface           = surface;
    listeners.destroySurface = surface->events.destroy.registerListener([this](std::any d) { sendLeave(); });

    resource->sendEnter(g_pSeatManager->nextSerial(owner.lock()), surface->getResource().get(), wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLPointerResource::sendLeave() {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    // release all buttons unless we have a dnd going on in which case
    // the events shall be lost.
    if (!PROTO::data->dndActive()) {
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        for (auto& b : pressedButtons) {
            sendButton(now.tv_sec * 1000 + now.tv_nsec / 1000000, b, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    pressedButtons.clear();

    resource->sendLeave(g_pSeatManager->nextSerial(owner.lock()), currentSurface->getResource().get());
    currentSurface.reset();
    listeners.destroySurface.reset();
}

void CWLPointerResource::sendMotion(uint32_t timeMs, const Vector2D& local) {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendMotion(timeMs, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLPointerResource::sendButton(uint32_t timeMs, uint32_t button, wl_pointer_button_state state) {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    if (state == WL_POINTER_BUTTON_STATE_RELEASED && std::find(pressedButtons.begin(), pressedButtons.end(), button) == pressedButtons.end()) {
        LOGM(ERR, "sendButton release on a non-pressed button");
        return;
    } else if (state == WL_POINTER_BUTTON_STATE_PRESSED && std::find(pressedButtons.begin(), pressedButtons.end(), button) != pressedButtons.end()) {
        LOGM(ERR, "sendButton press on a non-pressed button");
        return;
    }

    if (state == WL_POINTER_BUTTON_STATE_RELEASED)
        std::erase(pressedButtons, button);
    else if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        pressedButtons.emplace_back(button);

    resource->sendButton(g_pSeatManager->nextSerial(owner.lock()), timeMs, button, state);
}

void CWLPointerResource::sendAxis(uint32_t timeMs, wl_pointer_axis axis, double value) {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxis(timeMs, axis, wl_fixed_from_double(value));
}

void CWLPointerResource::sendFrame() {
    if (!owner || resource->version() < 5)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendFrame();
}

void CWLPointerResource::sendAxisSource(wl_pointer_axis_source source) {
    if (!owner || !currentSurface || resource->version() < 5)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxisSource(source);
}

void CWLPointerResource::sendAxisStop(uint32_t timeMs, wl_pointer_axis axis) {
    if (!owner || !currentSurface || resource->version() < 5)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxisStop(timeMs, axis);
}

void CWLPointerResource::sendAxisDiscrete(wl_pointer_axis axis, int32_t discrete) {
    if (!owner || !currentSurface || resource->version() < 5)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxisDiscrete(axis, discrete);
}

void CWLPointerResource::sendAxisValue120(wl_pointer_axis axis, int32_t value120) {
    if (!owner || !currentSurface || resource->version() < 8)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxisValue120(axis, value120);
}

void CWLPointerResource::sendAxisRelativeDirection(wl_pointer_axis axis, wl_pointer_axis_relative_direction direction) {
    if (!owner || !currentSurface || resource->version() < 9)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER))
        return;

    resource->sendAxisRelativeDirection(axis, direction);
}

CWLKeyboardResource::CWLKeyboardResource(SP<CWlKeyboard> resource_, SP<CWLSeatResource> owner_) : owner(owner_), resource(resource_) {
    if (!good())
        return;

    resource->setRelease([this](CWlKeyboard* r) { PROTO::seat->destroyResource(this); });
    resource->setOnDestroy([this](CWlKeyboard* r) { PROTO::seat->destroyResource(this); });

    if (!g_pSeatManager->keyboard) {
        LOGM(ERR, "No keyboard on bound wl_keyboard??");
        return;
    }

    sendKeymap(g_pSeatManager->keyboard.lock());
    repeatInfo(g_pSeatManager->keyboard->repeatRate, g_pSeatManager->keyboard->repeatDelay);

    if (g_pSeatManager->state.keyboardFocus && g_pSeatManager->state.keyboardFocus->client() == resource->client())
        sendEnter(g_pSeatManager->state.keyboardFocus.lock());
}

bool CWLKeyboardResource::good() {
    return resource->resource();
}

void CWLKeyboardResource::sendKeymap(SP<IKeyboard> keyboard) {
    if (!keyboard)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    wl_keyboard_keymap_format format = keyboard ? WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 : WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP;
    int                       fd;
    uint32_t                  size;
    if (keyboard) {
        fd   = keyboard->xkbKeymapFD;
        size = keyboard->xkbKeymapString.length() + 1;
    } else {
        fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            LOGM(ERR, "Failed to open /dev/null");
            return;
        }
        size = 0;
    }

    resource->sendKeymap(format, fd, size);

    if (!keyboard)
        close(fd);
}

void CWLKeyboardResource::sendEnter(SP<CWLSurfaceResource> surface) {
    if (!owner || currentSurface == surface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    if (currentSurface) {
        LOGM(WARN, "requested CWLKeyboardResource::sendEnter without sendLeave first.");
        sendLeave();
    }

    ASSERT(surface->client() == owner->client());

    currentSurface           = surface;
    listeners.destroySurface = surface->events.destroy.registerListener([this](std::any d) { sendLeave(); });

    wl_array arr;
    wl_array_init(&arr);

    resource->sendEnter(g_pSeatManager->nextSerial(owner.lock()), surface->getResource().get(), &arr);

    wl_array_release(&arr);
}

void CWLKeyboardResource::sendLeave() {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    resource->sendLeave(g_pSeatManager->nextSerial(owner.lock()), currentSurface->getResource().get());
    currentSurface.reset();
    listeners.destroySurface.reset();
}

void CWLKeyboardResource::sendKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state) {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    resource->sendKey(g_pSeatManager->nextSerial(owner.lock()), timeMs, key, state);
}

void CWLKeyboardResource::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!owner || !currentSurface)
        return;

    if (!(PROTO::seat->currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    resource->sendModifiers(g_pSeatManager->nextSerial(owner.lock()), depressed, latched, locked, group);
}

void CWLKeyboardResource::repeatInfo(uint32_t rate, uint32_t delayMs) {
    if (!owner || resource->version() < 4)
        return;

    resource->sendRepeatInfo(rate, delayMs);
}

CWLSeatResource::CWLSeatResource(SP<CWlSeat> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlSeat* r) {
        events.destroy.emit();
        PROTO::seat->destroyResource(this);
    });
    resource->setRelease([this](CWlSeat* r) {
        events.destroy.emit();
        PROTO::seat->destroyResource(this);
    });

    pClient = resource->client();

    resource->setGetKeyboard([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_vKeyboards.emplace_back(makeShared<CWLKeyboardResource>(makeShared<CWlKeyboard>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_vKeyboards.pop_back();
            return;
        }

        keyboards.push_back(RESOURCE);
    });

    resource->setGetPointer([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_vPointers.emplace_back(makeShared<CWLPointerResource>(makeShared<CWlPointer>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_vPointers.pop_back();
            return;
        }

        pointers.push_back(RESOURCE);
    });

    resource->setGetTouch([this](CWlSeat* r, uint32_t id) {
        const auto RESOURCE = PROTO::seat->m_vTouches.emplace_back(makeShared<CWLTouchResource>(makeShared<CWlTouch>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::seat->m_vTouches.pop_back();
            return;
        }

        touches.push_back(RESOURCE);
    });

    if (resource->version() >= 2)
        resource->sendName(HL_SEAT_NAME);

    sendCapabilities(PROTO::seat->currentCaps);
}

CWLSeatResource::~CWLSeatResource() {
    events.destroy.emit();
}

void CWLSeatResource::sendCapabilities(uint32_t caps) {
    uint32_t wlCaps = 0;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD)
        wlCaps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_POINTER)
        wlCaps |= WL_SEAT_CAPABILITY_POINTER;
    if (caps & eHIDCapabilityType::HID_INPUT_CAPABILITY_TOUCH)
        wlCaps |= WL_SEAT_CAPABILITY_TOUCH;

    resource->sendCapabilities((wl_seat_capability)wlCaps);
}

bool CWLSeatResource::good() {
    return resource->resource();
}

wl_client* CWLSeatResource::client() {
    return pClient;
}

CWLSeatProtocol::CWLSeatProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSeatProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vSeatResources.emplace_back(makeShared<CWLSeatResource>(makeShared<CWlSeat>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vSeatResources.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;

    LOGM(LOG, "New seat resource bound at {:x}", (uintptr_t)RESOURCE.get());

    events.newSeatResource.emit(RESOURCE);
}

void CWLSeatProtocol::destroyResource(CWLSeatResource* seat) {
    std::erase_if(m_vSeatResources, [&](const auto& other) { return other.get() == seat; });
}

void CWLSeatProtocol::destroyResource(CWLKeyboardResource* resource) {
    std::erase_if(m_vKeyboards, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::destroyResource(CWLPointerResource* resource) {
    std::erase_if(m_vPointers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::destroyResource(CWLTouchResource* resource) {
    std::erase_if(m_vTouches, [&](const auto& other) { return other.get() == resource; });
}

void CWLSeatProtocol::updateCapabilities(uint32_t caps) {
    if (caps == currentCaps)
        return;

    currentCaps = caps;

    for (auto& s : m_vSeatResources) {
        s->sendCapabilities(caps);
    }
}

void CWLSeatProtocol::updateKeymap() {
    if (!(currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    for (auto& k : m_vKeyboards) {
        k->sendKeymap(g_pSeatManager->keyboard.lock());
    }
}

void CWLSeatProtocol::updateRepeatInfo(uint32_t rate, uint32_t delayMs) {
    if (!(currentCaps & eHIDCapabilityType::HID_INPUT_CAPABILITY_KEYBOARD))
        return;

    for (auto& k : m_vKeyboards) {
        k->repeatInfo(rate, delayMs);
    }
}

SP<CWLSeatResource> CWLSeatProtocol::seatResourceForClient(wl_client* client) {
    for (auto& r : m_vSeatResources) {
        if (r->client() == client)
            return r;
    }

    return nullptr;
}

std::vector<uint8_t>& CCursorSurfaceRole::cursorPixelData(SP<CWLSurfaceResource> surface) {
    RASSERT(surface->role->role() == SURFACE_ROLE_CURSOR, "cursorPixelData called on a non-cursor surface");

    auto role = (CCursorSurfaceRole*)surface->role.get();
    return role->cursorShmPixelData;
}
