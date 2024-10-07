#include "InputCapture.hpp"

#include "../devices/IKeyboard.hpp"
#include "managers/SeatManager.hpp"
#include <fcntl.h>

CInputCaptureProtocol::CInputCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CInputCaptureProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_vManagers.emplace_back(std::make_unique<CHyprlandInputCaptureManagerV1>(client, ver, id));

    RESOURCE->setOnDestroy([this](CHyprlandInputCaptureManagerV1* p) { std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == p->resource(); }); });

    RESOURCE->setCapture([this](CHyprlandInputCaptureManagerV1* p) {
        Debug::log(LOG, "[input-capture] Input captured");
        active = true;
    });
    RESOURCE->setRelease([this](CHyprlandInputCaptureManagerV1* p) {
        Debug::log(LOG, "[input-capture] Input released");
        active = false;
    });

    sendKeymap(g_pSeatManager->keyboard.lock(), RESOURCE);
}

bool CInputCaptureProtocol::isCaptured() {
    return active;
}

void CInputCaptureProtocol::updateKeymap() {
    for (const auto& manager : m_vManagers)
        sendKeymap(g_pSeatManager->keyboard.lock(), manager);
}

void CInputCaptureProtocol::sendMotion(const Vector2D& absolutePosition, const Vector2D& delta) {
    for (const auto& manager : m_vManagers) {
        manager->sendMotion(wl_fixed_from_double(absolutePosition.x), wl_fixed_from_double(absolutePosition.y), wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y));
    }
}

void CInputCaptureProtocol::sendKeymap(SP<IKeyboard> keyboard, const std::unique_ptr<CHyprlandInputCaptureManagerV1>& manager) {
    if (!keyboard)
        return;

    hyprlandInputCaptureManagerV1KeymapFormat format;
    int                                       fd;
    uint32_t                                  size;
    if (keyboard) {
        format = HYPRLAND_INPUT_CAPTURE_MANAGER_V1_KEYMAP_FORMAT_XKB_V1;
        fd     = keyboard->xkbKeymapFD;
        size   = keyboard->xkbKeymapString.length() + 1;
    } else {
        format = HYPRLAND_INPUT_CAPTURE_MANAGER_V1_KEYMAP_FORMAT_NO_KEYMAP;
        fd     = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            LOGM(ERR, "Failed to open /dev/null");
            return;
        }
        size = 0;
    }

    manager->sendKeymap(format, fd, size);

    if (!keyboard)
        close(fd);
}

void CInputCaptureProtocol::sendKey(uint32_t keyCode, hyprlandInputCaptureManagerV1KeyState state) {
    for (const auto& manager : m_vManagers)
        manager->sendKey(keyCode, state);
}

void CInputCaptureProtocol::sendButton(uint32_t button, hyprlandInputCaptureManagerV1ButtonState state) {
    for (const auto& manager : m_vManagers)
        manager->sendButton(button, state);
}

void CInputCaptureProtocol::sendAxis(hyprlandInputCaptureManagerV1Axis axis, double value) {
    for (const auto& manager : m_vManagers)
        manager->sendAxis(axis, value);
}

void CInputCaptureProtocol::sendAxisValue120(hyprlandInputCaptureManagerV1Axis axis, int32_t value120) {
    for (const auto& manager : m_vManagers)
        manager->sendAxisValue120(axis, value120);
}

void CInputCaptureProtocol::sendAxisStop(hyprlandInputCaptureManagerV1Axis axis) {
    for (const auto& manager : m_vManagers)
        manager->sendAxisStop(axis);
}

void CInputCaptureProtocol::sendFrame() {
    for (const auto& manager : m_vManagers)
        manager->sendFrame();
}
