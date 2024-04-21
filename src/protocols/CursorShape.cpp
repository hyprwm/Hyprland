#include "CursorShape.hpp"
#include <algorithm>

// clang-format off
constexpr const char* SHAPE_NAMES[] = {
    "invalid",
    "default",
    "context_menu",
    "help",
    "pointer",
    "progress",
    "wait",
    "cell",
    "crosshair",
    "text",
    "vertical_text",
    "alias",
    "copy",
    "move",
    "no_drop",
    "not_allowed",
    "grab",
    "grabbing",
    "e_resize",
    "n_resize",
    "ne_resize",
    "nw_resize",
    "s_resize",
    "se_resize",
    "sw_resize",
    "w_resize",
    "ew_resize",
    "ns_resize",
    "nesw_resize",
    "nwse_resize",
    "col_resize",
    "row_resize",
    "all_scroll",
    "zoom_in",
    "zoom_out",
};
// clang-format on

CCursorShapeProtocol::CCursorShapeProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CCursorShapeProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CCursorShapeProtocol::onDeviceResourceDestroy(wl_resource* res) {
    m_mDevices.erase(res);
}

void CCursorShapeProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpCursorShapeManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpCursorShapeManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpCursorShapeManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetPointer([this](CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetPointer(pMgr, id, pointer); });
    RESOURCE->setGetTabletToolV2([this](CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* tablet) { this->onGetTabletToolV2(pMgr, id, tablet); });
}

void CCursorShapeProtocol::onGetPointer(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* pointer) {
    createCursorShapeDevice(pMgr, id, pointer);
}

void CCursorShapeProtocol::onGetTabletToolV2(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* tablet) {
    createCursorShapeDevice(pMgr, id, tablet);
}

void CCursorShapeProtocol::createCursorShapeDevice(CWpCursorShapeManagerV1* pMgr, uint32_t id, wl_resource* resource) {
    if (m_mDevices.contains(resource)) {
        wl_resource_post_error(resource, 0, "Device already exists");
        return;
    }

    const auto CLIENT   = wl_resource_get_client(pMgr->resource());
    const auto RESOURCE = m_mDevices.emplace(resource, std::make_shared<CWpCursorShapeDeviceV1>(CLIENT, wl_resource_get_version(pMgr->resource()), id)).first->second.get();
    RESOURCE->setOnDestroy([this](CWpCursorShapeDeviceV1* p) { this->onDeviceResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpCursorShapeDeviceV1* p) { this->onDeviceResourceDestroy(p->resource()); });
    RESOURCE->setSetShape([this](CWpCursorShapeDeviceV1* p, uint32_t serial, wpCursorShapeDeviceV1Shape shape) { this->onSetShape(p, serial, shape); });
}

void CCursorShapeProtocol::onSetShape(CWpCursorShapeDeviceV1* pMgr, uint32_t serial, wpCursorShapeDeviceV1Shape shape) {
    if ((uint32_t)shape == 0 || (uint32_t)shape > sizeof(SHAPE_NAMES)) {
        wl_resource_post_error(pMgr->resource(), ERROR_INVALID_SHAPE, "The shape is invalid");
        return;
    }

    SSetShapeEvent event;
    event.pMgr      = pMgr;
    event.shape     = shape;
    event.shapeName = SHAPE_NAMES[shape];

    events.setShape.emit(event);
}