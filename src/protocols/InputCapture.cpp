#include "InputCapture.hpp"
#include "hyprland-input-capture-v1.hpp"
#include <memory>
#include <vector>
#include <wayland-util.h>

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

void CInputCaptureProtocol::onCapture(CHyprlandInputCaptureManagerV1* pMgr) {}

void CInputCaptureProtocol::onRelease(CHyprlandInputCaptureManagerV1* pMgr) {}

void CInputCaptureProtocol::sendAbsoluteMotion(const Vector2D& absolutePosition, const Vector2D& delta) {
    for (const UP<CHyprlandInputCaptureManagerV1>& manager : m_vManagers) {
        manager->sendAbsoluteMotion(wl_fixed_from_double(absolutePosition.x), wl_fixed_from_double(absolutePosition.y), wl_fixed_from_double(delta.x),
                                    wl_fixed_from_double(delta.y));
    }
}
