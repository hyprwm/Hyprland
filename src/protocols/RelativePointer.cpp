#include "RelativePointer.hpp"
#include "Compositor.hpp"
#include <algorithm>

CRelativePointer::CRelativePointer(SP<CZwpRelativePointerV1> resource_) : resource(resource_) {
    if (!resource_->resource())
        return;

    pClient = wl_resource_get_client(resource_->resource());

    resource->setDestroy([this](CZwpRelativePointerV1* pMgr) { PROTO::relativePointer->destroyRelativePointer(this); });
    resource->setOnDestroy([this](CZwpRelativePointerV1* pMgr) { PROTO::relativePointer->destroyRelativePointer(this); });
}

bool CRelativePointer::good() {
    return resource->resource();
}

wl_client* CRelativePointer::client() {
    return pClient;
}

void CRelativePointer::sendRelativeMotion(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {
    resource->sendRelativeMotion(time >> 32, time & 0xFFFFFFFF, wl_fixed_from_double(delta.x), wl_fixed_from_double(delta.y), wl_fixed_from_double(deltaUnaccel.x),
                                 wl_fixed_from_double(deltaUnaccel.y));
}

CRelativePointerProtocol::CRelativePointerProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CRelativePointerProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpRelativePointerManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpRelativePointerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpRelativePointerManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetRelativePointer([this](CZwpRelativePointerManagerV1* pMgr, uint32_t id, wl_resource* pointer) { this->onGetRelativePointer(pMgr, id, pointer); });
}

void CRelativePointerProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CRelativePointerProtocol::destroyRelativePointer(CRelativePointer* pointer) {
    std::erase_if(m_vRelativePointers, [&](const auto& other) { return other.get() == pointer; });
}

void CRelativePointerProtocol::onGetRelativePointer(CZwpRelativePointerManagerV1* pMgr, uint32_t id, wl_resource* pointer) {
    const auto CLIENT = wl_resource_get_client(pMgr->resource());
    const auto RESOURCE =
        m_vRelativePointers.emplace_back(std::make_unique<CRelativePointer>(std::make_shared<CZwpRelativePointerV1>(CLIENT, wl_resource_get_version(pMgr->resource()), id))).get();

    if (!RESOURCE->good()) {
        wl_resource_post_no_memory(pMgr->resource());
        m_vRelativePointers.pop_back();
        return;
    }
}

void CRelativePointerProtocol::sendRelativeMotion(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel) {

    if (!g_pCompositor->m_sSeat.seat->pointer_state.focused_client)
        return;

    const auto FOCUSED = g_pCompositor->m_sSeat.seat->pointer_state.focused_client->client;

    for (auto& rp : m_vRelativePointers) {
        if (FOCUSED != rp->client())
            continue;

        rp->sendRelativeMotion(time, delta, deltaUnaccel);
    }
}