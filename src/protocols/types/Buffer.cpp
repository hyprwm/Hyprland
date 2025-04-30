#include "Buffer.hpp"

IHLBuffer::~IHLBuffer() {
    if (locked() && resource)
        sendRelease();
}

void IHLBuffer::sendRelease() {
    resource->sendRelease();
    syncReleasers.clear();
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

void IHLBuffer::addReleasePoint(CDRMSyncPointState& point) {
    ASSERT(locked());
    if (point)
        syncReleasers.emplace_back(point.createSyncRelease());
}

CHLBufferReference::CHLBufferReference() : buffer(nullptr) {
    ;
}

CHLBufferReference::CHLBufferReference(const CHLBufferReference& other) : buffer(other.buffer) {
    if (buffer)
        buffer->lock();
}

CHLBufferReference::CHLBufferReference(SP<IHLBuffer> buffer_) : buffer(buffer_) {
    if (buffer)
        buffer->lock();
}

CHLBufferReference::~CHLBufferReference() {
    if (buffer)
        buffer->unlock();
}

CHLBufferReference& CHLBufferReference::operator=(const CHLBufferReference& other) {
    if (other.buffer)
        other.buffer->lock();
    if (buffer)
        buffer->unlock();
    buffer = other.buffer;
    return *this;
}

bool CHLBufferReference::operator==(const CHLBufferReference& other) const {
    return buffer == other.buffer;
}

bool CHLBufferReference::operator==(const SP<IHLBuffer>& other) const {
    return buffer == other;
}

bool CHLBufferReference::operator==(const SP<Aquamarine::IBuffer>& other) const {
    return buffer == other;
}

SP<IHLBuffer> CHLBufferReference::operator->() const {
    return buffer;
}

CHLBufferReference::operator bool() const {
    return buffer;
}

void CHLBufferReference::drop() {
    if (!buffer)
        return;

    buffer->nLocks--;
    ASSERT(buffer->nLocks >= 0);

    buffer = nullptr;
}
