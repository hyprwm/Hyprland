#include "ContentType.hpp"
#include "content-type-v1.hpp"
#include "protocols/types/ContentType.hpp"

CContentTypeManager::CContentTypeManager(SP<CWpContentTypeManagerV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([](CWpContentTypeManagerV1* r) {});
    resource->setOnDestroy([this](CWpContentTypeManagerV1* r) { PROTO::contentType->destroyResource(this); });

    resource->setGetSurfaceContentType([](CWpContentTypeManagerV1* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        if (SURF->colorManagement) {
            r->error(WP_CONTENT_TYPE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED, "CT manager already exists");
            return;
        }

        const auto RESOURCE = PROTO::contentType->m_vContentTypes.emplace_back(makeShared<CContentType>(makeShared<CWpContentTypeV1>(r->client(), r->version(), id)));
        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::contentType->m_vContentTypes.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        SURF->contentType = RESOURCE;
    });
}

bool CContentTypeManager::good() {
    return m_resource->resource();
}

CContentType::CContentType(WP<CWLSurfaceResource> surface) {
    destroy = surface->events.destroy.registerListener([this](std::any d) { PROTO::contentType->destroyResource(this); });
}

CContentType::CContentType(SP<CWpContentTypeV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_pClient = resource->client();

    resource->setDestroy([this](CWpContentTypeV1* r) { PROTO::contentType->destroyResource(this); });
    resource->setOnDestroy([this](CWpContentTypeV1* r) { PROTO::contentType->destroyResource(this); });

    resource->setSetContentType([this](CWpContentTypeV1* r, wpContentTypeV1Type type) { value = NContentType::fromWP(type); });
}

bool CContentType::good() {
    return m_resource && m_resource->resource();
}

wl_client* CContentType::client() {
    return m_pClient;
}

CContentTypeProtocol::CContentTypeProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CContentTypeProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CContentTypeManager>(makeShared<CWpContentTypeManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

SP<CContentType> CContentTypeProtocol::getContentType(WP<CWLSurfaceResource> surface) {
    if (surface->contentType.valid())
        return surface->contentType.lock();

    return m_vContentTypes.emplace_back(makeShared<CContentType>(surface));
}

void CContentTypeProtocol::destroyResource(CContentTypeManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CContentTypeProtocol::destroyResource(CContentType* resource) {
    std::erase_if(m_vContentTypes, [&](const auto& other) { return other.get() == resource; });
}
