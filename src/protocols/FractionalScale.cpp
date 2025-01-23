#include "FractionalScale.hpp"
#include <algorithm>
#include "core/Compositor.hpp"

CFractionalScaleProtocol::CFractionalScaleProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFractionalScaleProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CWpFractionalScaleManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpFractionalScaleManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpFractionalScaleManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetFractionalScale(
        [this](CWpFractionalScaleManagerV1* pMgr, uint32_t id, wl_resource* surface) { this->onGetFractionalScale(pMgr, id, CWLSurfaceResource::fromResource(surface)); });
}

void CFractionalScaleProtocol::removeAddon(CFractionalScaleAddon* addon) {
    m_mAddons.erase(addon->surf());
}

void CFractionalScaleProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [res](const auto& other) { return other->resource() == res; });
}

void CFractionalScaleProtocol::onGetFractionalScale(CWpFractionalScaleManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface) {
    for (auto const& [k, v] : m_mAddons) {
        if (k == surface) {
            LOGM(ERR, "Surface {:x} already has a fractionalScale addon", (uintptr_t)surface.get());
            pMgr->error(WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS, "Fractional scale already exists");
            return;
        }
    }

    const auto PADDON =
        m_mAddons.emplace(surface, makeUnique<CFractionalScaleAddon>(makeShared<CWpFractionalScaleV1>(pMgr->client(), pMgr->version(), id), surface)).first->second.get();

    if UNLIKELY (!PADDON->good()) {
        m_mAddons.erase(surface);
        pMgr->noMemory();
        return;
    }

    PADDON->m_resource->setOnDestroy([this, PADDON](CWpFractionalScaleV1* self) { this->removeAddon(PADDON); });
    PADDON->m_resource->setDestroy([this, PADDON](CWpFractionalScaleV1* self) { this->removeAddon(PADDON); });

    if (std::ranges::find_if(m_mSurfaceScales, [surface](const auto& e) { return e.first == surface; }) == m_mSurfaceScales.end())
        m_mSurfaceScales.emplace(surface, 1.F);

    if (surface->mapped)
        PADDON->setScale(m_mSurfaceScales.at(surface));

    // clean old
    std::erase_if(m_mSurfaceScales, [](const auto& e) { return e.first.expired(); });
}

void CFractionalScaleProtocol::sendScale(SP<CWLSurfaceResource> surf, const float& scale) {
    m_mSurfaceScales[surf] = scale;
    if (m_mAddons.contains(surf))
        m_mAddons[surf]->setScale(scale);
}

CFractionalScaleAddon::CFractionalScaleAddon(SP<CWpFractionalScaleV1> resource_, SP<CWLSurfaceResource> surf_) : m_resource(resource_), m_surface(surf_) {
    m_resource->setDestroy([this](CWpFractionalScaleV1* self) { PROTO::fractional->removeAddon(this); });
    m_resource->setOnDestroy([this](CWpFractionalScaleV1* self) { PROTO::fractional->removeAddon(this); });
}

void CFractionalScaleAddon::setScale(const float& scale) {
    if (m_scale == scale)
        return;

    m_scale = scale;
    m_resource->sendPreferredScale(std::round(scale * 120.0));
}

bool CFractionalScaleAddon::good() {
    return m_resource->resource();
}

SP<CWLSurfaceResource> CFractionalScaleAddon::surf() {
    return m_surface.lock();
}
