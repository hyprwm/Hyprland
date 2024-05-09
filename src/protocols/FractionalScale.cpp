#include "FractionalScale.hpp"

#define LOGM PROTO::fractional->protoLog

static void onWlrSurfaceDestroy(void* owner, void* data) {
    const auto SURF = (wlr_surface*)owner;

    PROTO::fractional->onSurfaceDestroy(SURF);
}

CFractionalScaleProtocol::CFractionalScaleProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFractionalScaleProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpFractionalScaleManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpFractionalScaleManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpFractionalScaleManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetFractionalScale(
        [this](CWpFractionalScaleManagerV1* pMgr, uint32_t id, wl_resource* surface) { this->onGetFractionalScale(pMgr, id, wlr_surface_from_resource(surface)); });
}

void CFractionalScaleProtocol::removeAddon(CFractionalScaleAddon* addon) {
    m_mAddons.erase(addon->surf());
}

void CFractionalScaleProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [res](const auto& other) { return other->resource() == res; });
}

void CFractionalScaleProtocol::onGetFractionalScale(CWpFractionalScaleManagerV1* pMgr, uint32_t id, wlr_surface* surface) {
    if (m_mAddons.contains(surface)) {
        LOGM(ERR, "Surface {:x} already has a fractionalScale addon", (uintptr_t)surface);
        pMgr->error(WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS, "Fractional scale already exists");
        return;
    }

    const auto PADDON =
        m_mAddons.emplace(surface, std::make_unique<CFractionalScaleAddon>(makeShared<CWpFractionalScaleV1>(pMgr->client(), pMgr->version(), id), surface)).first->second.get();

    if (!PADDON->good()) {
        m_mAddons.erase(surface);
        pMgr->noMemory();
        return;
    }

    PADDON->resource->setOnDestroy([this, PADDON](CWpFractionalScaleV1* self) { this->removeAddon(PADDON); });
    PADDON->resource->setDestroy([this, PADDON](CWpFractionalScaleV1* self) { this->removeAddon(PADDON); });

    if (!m_mSurfaceScales.contains(surface))
        m_mSurfaceScales[surface] = 1.F;

    PADDON->setScale(m_mSurfaceScales[surface]);
    registerSurface(surface);
}

void CFractionalScaleProtocol::sendScale(wlr_surface* surf, const float& scale) {
    m_mSurfaceScales[surf] = scale;
    if (m_mAddons.contains(surf))
        m_mAddons[surf]->setScale(scale);
    registerSurface(surf);
}

void CFractionalScaleProtocol::registerSurface(wlr_surface* surf) {
    if (m_mSurfaceDestroyListeners.contains(surf))
        return;

    m_mSurfaceDestroyListeners[surf].hyprListener_surfaceDestroy.initCallback(&surf->events.destroy, ::onWlrSurfaceDestroy, surf, "FractionalScale");
}

void CFractionalScaleProtocol::onSurfaceDestroy(wlr_surface* surf) {
    m_mSurfaceDestroyListeners.erase(surf);
    m_mSurfaceScales.erase(surf);
    if (m_mAddons.contains(surf))
        m_mAddons[surf]->onSurfaceDestroy();
}

CFractionalScaleAddon::CFractionalScaleAddon(SP<CWpFractionalScaleV1> resource_, wlr_surface* surf_) : resource(resource_), surface(surf_) {
    resource->setDestroy([this](CWpFractionalScaleV1* self) { PROTO::fractional->removeAddon(this); });
    resource->setOnDestroy([this](CWpFractionalScaleV1* self) { PROTO::fractional->removeAddon(this); });
}

void CFractionalScaleAddon::onSurfaceDestroy() {
    surfaceGone = true;
}

void CFractionalScaleAddon::setScale(const float& scale) {
    resource->sendPreferredScale(std::round(scale * 120.0));
}

bool CFractionalScaleAddon::good() {
    return resource->resource();
}

wlr_surface* CFractionalScaleAddon::surf() {
    return surface;
}