#include "InputCapture.hpp"

CInputCaptureProtocol::CInputCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CInputCaptureProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CHyprlandInputCaptureManagerV1>(client, ver, id)).get();

    RESOURCE->setOnDestroy([this](CHyprlandInputCaptureManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCapture([this](CHyprlandInputCaptureManagerV1* p) { this->onCapture(p); });
    RESOURCE->setRelease([this](CHyprlandInputCaptureManagerV1* p) { this->onRelease(p); });
}

void CInputCaptureProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CInputCaptureProtocol::onCapture(CHyprlandInputCaptureManagerV1* pMgr) {
    Debug::log(LOG, "[input-capture] Input captured");
    active = true;
}

void CInputCaptureProtocol::onRelease(CHyprlandInputCaptureManagerV1* pMgr) {
    Debug::log(LOG, "[input-capture] Input released");
    active = false;
}

bool CInputCaptureProtocol::isCaptured() {
    return active;
}

void CInputCaptureProtocol::sendMotion(const Vector2D& absolutePosition, const Vector2D& delta) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendMotion(wl_fixed_from_double(absolutePosition.x), wl_fixed_from_double(absolutePosition.y), wl_fixed_from_double(delta.x),
                                    wl_fixed_from_double(delta.y));
    }
}

void CInputCaptureProtocol::sendKey(uint32_t keyCode, hyprlandInputCaptureManagerV1KeyState state) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendKey(keyCode, state);
    }
}

void CInputCaptureProtocol::sendButton(uint32_t button, hyprlandInputCaptureManagerV1ButtonState state) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendButton(button, state);
    }
}

void CInputCaptureProtocol::sendAxis(hyprlandInputCaptureManagerV1Axis axis, double value) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendAxis(axis, value);
    }
}

void CInputCaptureProtocol::sendAxisValue120(hyprlandInputCaptureManagerV1Axis axis, int32_t value120) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendAxisValue120(axis, value120);
    }
}

void CInputCaptureProtocol::sendAxisStop(hyprlandInputCaptureManagerV1Axis axis) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendAxisStop(axis);
    }
}

void CInputCaptureProtocol::sendFrame() {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendFrame();
    }
}
