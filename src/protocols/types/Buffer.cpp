#include "Buffer.hpp"

IHLBuffer::~IHLBuffer() {
    if (locked() && m_resource)
        sendRelease();
}

void IHLBuffer::sendRelease() {
    m_resource->sendRelease();
    m_syncReleasers.clear();
}

void IHLBuffer::lock() {
    m_locks++;
}

void IHLBuffer::unlock() {
    m_locks--;

    ASSERT(m_locks >= 0);

    if (m_locks == 0)
        sendRelease();
}

bool IHLBuffer::locked() {
    return m_locks > 0;
}

void IHLBuffer::onBackendRelease(const std::function<void()>& fn) {
    if (m_hlEvents.backendRelease) {
        m_hlEvents.backendRelease->emit(nullptr);
        Debug::log(LOG, "backendRelease emitted early");
    }

    m_hlEvents.backendRelease = events.backendRelease.registerListener([this, fn](std::any) {
        fn();
        m_hlEvents.backendRelease.reset();
    });
}

void IHLBuffer::addReleasePoint(CDRMSyncPointState& point) {
    ASSERT(locked());
    if (point)
        m_syncReleasers.emplace_back(point.createSyncRelease());
}

CHLBufferReference::CHLBufferReference() : m_buffer(nullptr) {
    ;
}

CHLBufferReference::CHLBufferReference(const CHLBufferReference& other) : m_buffer(other.m_buffer) {
    if (m_buffer)
        m_buffer->lock();
}

CHLBufferReference::CHLBufferReference(SP<IHLBuffer> buffer_) : m_buffer(buffer_) {
    if (m_buffer)
        m_buffer->lock();
}

CHLBufferReference::~CHLBufferReference() {
    if (m_buffer)
        m_buffer->unlock();
}

CHLBufferReference& CHLBufferReference::operator=(const CHLBufferReference& other) {
    if (other.m_buffer)
        other.m_buffer->lock();
    if (m_buffer)
        m_buffer->unlock();
    m_buffer = other.m_buffer;
    return *this;
}

bool CHLBufferReference::operator==(const CHLBufferReference& other) const {
    return m_buffer == other.m_buffer;
}

bool CHLBufferReference::operator==(const SP<IHLBuffer>& other) const {
    return m_buffer == other;
}

bool CHLBufferReference::operator==(const SP<Aquamarine::IBuffer>& other) const {
    return m_buffer == other;
}

SP<IHLBuffer> CHLBufferReference::operator->() const {
    return m_buffer;
}

CHLBufferReference::operator bool() const {
    return m_buffer;
}

void CHLBufferReference::drop() {
    if (!m_buffer)
        return;

    m_buffer->m_locks--;
    ASSERT(m_buffer->m_locks >= 0);

    m_buffer = nullptr;
}
