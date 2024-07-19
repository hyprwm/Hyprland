#include "Buffer.hpp"

void IHLBuffer::sendRelease() {
    resource->sendRelease();
}

void IHLBuffer::sendReleaseWithSurface(SP<CWLSurfaceResource> surf) {
    if (resource && resource->good())
        resource->sendReleaseWithSurface(surf);
}
