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

void IHLBuffer::onBackendRelease(const std::function<void()>& fn) {
    if (hlEvents.backendRelease) {
        hlEvents.backendRelease->emit(nullptr);
        Debug::log(LOG, "backendRelease emitted early");
    }

    hlEvents.backendRelease = events.backendRelease.registerListener([this, fn](std::any) {
        fn();
        hlEvents.backendRelease.reset();
    });
}

CHLBufferReference::CHLBufferReference(WP<IHLBuffer> buffer_, SP<CWLSurfaceResource> surface_) : buffer(buffer_), surface(surface_) {
    buffer->lock();
}

CHLBufferReference::~CHLBufferReference() {
    if (buffer.expired())
        return;

    buffer->unlock();
}
