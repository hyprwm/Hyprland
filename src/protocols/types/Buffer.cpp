#include "Buffer.hpp"
#include "WLBuffer.hpp"

SDMABUFAttrs IWLBuffer::dmabuf() {
    return SDMABUFAttrs{};
}

SSHMAttrs IWLBuffer::shm() {
    return SSHMAttrs{};
}

std::tuple<uint8_t*, uint32_t, size_t> IWLBuffer::beginDataPtr(uint32_t flags) {
    return {nullptr, 0, 0};
}

void IWLBuffer::endDataPtr() {
    ; // empty
}

void IWLBuffer::sendRelease() {
    if (!resource || !resource->resource)
        return;
    resource->resource->sendRelease();
}

void IWLBuffer::lock() {
    locks++;
}

void IWLBuffer::unlock() {
    locks--;

    ASSERT(locks >= 0);

    if (locks <= 0)
        sendRelease();
}

bool IWLBuffer::locked() {
    return locks;
}
