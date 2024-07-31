#include "Buffer.hpp"

IHLBuffer::~IHLBuffer() {
    if (locked() && resource)
        sendRelease();
}

void IHLBuffer::sendRelease() {
    resource->sendRelease();
}

void IHLBuffer::sendReleaseWithSurface(SP<CWLSurfaceResource> surf) {
    if (resource && resource->good())
        resource->sendReleaseWithSurface(surf);
}

void IHLBuffer::lock() {
    nLocks++;
}

void IHLBuffer::unlock() {
    nLocks--;

    ASSERT(nLocks >= 0);

    if (nLocks == 0)
        sendRelease();
}

void IHLBuffer::unlockWithSurface(SP<CWLSurfaceResource> surf) {
    nLocks--;

    ASSERT(nLocks >= 0);

    if (nLocks == 0)
        sendReleaseWithSurface(surf);
}

bool IHLBuffer::locked() {
    return nLocks > 0;
}

void IHLBuffer::unlockOnBufferRelease(WP<CWLSurfaceResource> surf) {
    unlockSurface           = surf;
    hlEvents.backendRelease = events.backendRelease.registerListener([this](std::any data) {
        if (unlockSurface.expired())
            unlock();
        else
            unlockWithSurface(unlockSurface.lock());
        hlEvents.backendRelease.reset();
    });
}

CHLBufferReference::CHLBufferReference(SP<IHLBuffer> buffer_, SP<CWLSurfaceResource> surface_) : buffer(buffer_), surface(surface_) {
    buffer->lock();
}

CHLBufferReference::~CHLBufferReference() {
    if (buffer.expired())
        return;

    if (surface)
        buffer->unlockWithSurface(surface.lock());
    else
        buffer->unlock();
}
