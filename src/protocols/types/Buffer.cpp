#include "Buffer.hpp"

IHLBuffer::~IHLBuffer() {
    if (locked() && resource)
        sendRelease();
}

void IHLBuffer::sendRelease() {
    resource->sendRelease();
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

bool IHLBuffer::locked() {
    return nLocks > 0;
}

void IHLBuffer::unlockOnBufferRelease(WP<CWLSurfaceResource> surf) {
    hlEvents.backendRelease = events.backendRelease.registerListener([this](std::any data) {
        unlock();
        hlEvents.backendRelease.reset();
    });
}

CHLBufferReference::CHLBufferReference(SP<IHLBuffer> buffer_, SP<CWLSurfaceResource> surface_) : buffer(buffer_), surface(surface_) {
    buffer->lock();
}

CHLBufferReference::~CHLBufferReference() {
    if (buffer.expired())
        return;

    buffer->unlock();
}
