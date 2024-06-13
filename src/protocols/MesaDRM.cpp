#include "MesaDRM.hpp"
#include <algorithm>
#include <xf86drm.h>
#include "../Compositor.hpp"
#include <wlr/render/drm_format_set.h>
#include "types/WLBuffer.hpp"

#define LOGM PROTO::mesaDRM->protoLog

CMesaDRMBufferResource::CMesaDRMBufferResource(uint32_t id, wl_client* client, SDMABUFAttrs attrs_) {
    LOGM(LOG, "Creating a Mesa dmabuf, with id {}: size {}, fmt {}, planes {}", id, attrs_.size, attrs_.format, attrs_.planes);
    for (int i = 0; i < attrs_.planes; ++i) {
        LOGM(LOG, " | plane {}: mod {} fd {} stride {} offset {}", i, attrs_.modifier, attrs_.fds[i], attrs_.strides[i], attrs_.offsets[i]);
    }

    buffer                   = makeShared<CDMABuffer>(id, client, attrs_);
    buffer->resource->buffer = buffer;

    listeners.bufferResourceDestroy = buffer->events.destroy.registerListener([this](std::any d) {
        listeners.bufferResourceDestroy.reset();
        PROTO::mesaDRM->destroyResource(this);
    });

    if (!buffer->success)
        LOGM(ERR, "Possibly compositor bug: buffer failed to create");
}

CMesaDRMBufferResource::~CMesaDRMBufferResource() {
    if (buffer && buffer->resource)
        buffer->resource->sendRelease();
    buffer.reset();
    listeners.bufferResourceDestroy.reset();
}

bool CMesaDRMBufferResource::good() {
    return buffer && buffer->good();
}

CMesaDRMResource::CMesaDRMResource(SP<CWlDrm> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlDrm* r) { PROTO::mesaDRM->destroyResource(this); });

    resource->setAuthenticate([this](CWlDrm* r, uint32_t token) {
        // we don't need this
        resource->sendAuthenticated();
    });

    resource->setCreateBuffer([](CWlDrm* r, uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t) { r->error(WL_DRM_ERROR_INVALID_NAME, "Not supported, use prime instead"); });

    resource->setCreatePlanarBuffer([](CWlDrm* r, uint32_t, uint32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) {
        r->error(WL_DRM_ERROR_INVALID_NAME, "Not supported, use prime instead");
    });

    resource->setCreatePrimeBuffer(
        [this](CWlDrm* r, uint32_t id, int32_t nameFd, int32_t w, int32_t h, uint32_t fmt, int32_t off0, int32_t str0, int32_t off1, int32_t str1, int32_t off2, int32_t str2) {
            if (off0 < 0 || w <= 0 || h <= 0) {
                r->error(WL_DRM_ERROR_INVALID_FORMAT, "Invalid w, h, or offset");
                return;
            }

            SDMABUFAttrs attrs;
            attrs.success    = true;
            attrs.size       = {w, h};
            attrs.modifier   = DRM_FORMAT_MOD_INVALID;
            attrs.planes     = 1;
            attrs.offsets[0] = off0;
            attrs.strides[0] = str0;
            attrs.fds[0]     = nameFd;
            attrs.format     = fmt;

            const auto RESOURCE = PROTO::mesaDRM->m_vBuffers.emplace_back(makeShared<CMesaDRMBufferResource>(id, resource->client(), attrs));

            if (!RESOURCE->good()) {
                r->noMemory();
                PROTO::mesaDRM->m_vBuffers.pop_back();
                return;
            }

            // append instance so that buffer knows its owner
            RESOURCE->buffer->resource->buffer = RESOURCE->buffer;
        });

    resource->sendDevice(PROTO::mesaDRM->nodeName.c_str());
    resource->sendCapabilities(WL_DRM_CAPABILITY_PRIME);

    auto fmts = g_pHyprOpenGL->getDRMFormats();
    for (auto& fmt : fmts) {
        resource->sendFormat(fmt.format);
    }
}

bool CMesaDRMResource::good() {
    return resource->resource();
}

CMesaDRMProtocol::CMesaDRMProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    drmDevice* dev   = nullptr;
    int        drmFD = wlr_renderer_get_drm_fd(g_pCompositor->m_sWLRRenderer);
    if (drmGetDevice2(drmFD, 0, &dev) != 0) {
        LOGM(ERR, "Failed to get device");
        PROTO::mesaDRM.reset();
        return;
    }

    if (dev->available_nodes & (1 << DRM_NODE_RENDER)) {
        nodeName = dev->nodes[DRM_NODE_RENDER];
    } else {
        ASSERT(dev->available_nodes & (1 << DRM_NODE_PRIMARY));
        LOGM(WARN, "No DRM render node, falling back to primary {}", dev->nodes[DRM_NODE_PRIMARY]);
        nodeName = dev->nodes[DRM_NODE_PRIMARY];
    }
    drmFreeDevice(&dev);
}

void CMesaDRMProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CMesaDRMResource>(makeShared<CWlDrm>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CMesaDRMProtocol::destroyResource(CMesaDRMResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CMesaDRMProtocol::destroyResource(CMesaDRMBufferResource* resource) {
    std::erase_if(m_vBuffers, [&](const auto& other) { return other.get() == resource; });
}
