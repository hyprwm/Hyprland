#include "Buffer.hpp"

void IHLBuffer::sendRelease() {
    resource->sendRelease();
}

void IHLBuffer::sendReleaseWithSurface(SP<CWLSurfaceResource> surf) {
    resource->sendReleaseWithSurface(surf);
}
