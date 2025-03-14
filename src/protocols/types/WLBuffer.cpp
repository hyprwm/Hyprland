#include "WLBuffer.hpp"
#include "Buffer.hpp"
#include "../core/Compositor.hpp"
#include "../DRMSyncobj.hpp"
#include "../../helpers/sync/SyncTimeline.hpp"
#include <xf86drm.h>

CWLBufferResource::CWLBufferResource(WP<CWlBuffer> resource_) : resource(resource_.lock()) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWlBuffer* r) {
        if (buffer.expired())
            return;
        buffer->events.destroy.emit();
    });
    resource->setDestroy([this](CWlBuffer* r) {
        if (buffer.expired())
            return;
        buffer->events.destroy.emit();
    });

    resource->setData(this);
}

bool CWLBufferResource::good() {
    return resource->resource();
}

void CWLBufferResource::sendRelease() {
    resource->sendRelease();
}

wl_resource* CWLBufferResource::getResource() {
    return resource->resource();
}

SP<CWLBufferResource> CWLBufferResource::fromResource(wl_resource* res) {
    auto data = (CWLBufferResource*)(((CWlBuffer*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

SP<CWLBufferResource> CWLBufferResource::create(WP<CWlBuffer> resource) {
    auto p  = SP<CWLBufferResource>(new CWLBufferResource(resource));
    p->self = p;
    return p;
}
