#include "Shm.hpp"
#include <algorithm>
#include <sys/mman.h>
#include <drm_fourcc.h>
#include "../../render/Texture.hpp"
#include "../types/WLBuffer.hpp"
#include "../../Compositor.hpp"
#include "../../helpers/Format.hpp"

CWLSHMBuffer::CWLSHMBuffer(SP<CWLSHMPoolResource> pool_, uint32_t id, int32_t offset_, const Vector2D& size_, int32_t stride_, uint32_t fmt_) {
    if (!pool_->pool->data)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    size   = size_;
    pool   = pool_->pool;
    stride = stride_;
    fmt    = fmt_;
    offset = offset_;
    opaque = FormatUtils::isFormatOpaque(FormatUtils::shmToDRM(fmt_));

    texture = makeShared<CTexture>(FormatUtils::shmToDRM(fmt), (uint8_t*)pool->data + offset, stride, size_);

    resource = CWLBufferResource::create(makeShared<CWlBuffer>(pool_->resource->client(), 1, id));

    listeners.bufferResourceDestroy = events.destroy.registerListener([this](std::any d) {
        listeners.bufferResourceDestroy.reset();
        PROTO::shm->destroyResource(this);
    });

    success = texture->m_iTexID;

    if (!success)
        Debug::log(ERR, "Failed creating a shm texture: null texture id");
}

CWLSHMBuffer::~CWLSHMBuffer() {
    ;
}

Aquamarine::eBufferCapability CWLSHMBuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CWLSHMBuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_SHM;
}

bool CWLSHMBuffer::isSynchronous() {
    return true;
}

Aquamarine::SSHMAttrs CWLSHMBuffer::shm() {
    Aquamarine::SSHMAttrs attrs;
    attrs.success = true;
    attrs.fd      = pool->fd;
    attrs.format  = FormatUtils::shmToDRM(fmt);
    attrs.size    = size;
    attrs.stride  = stride;
    attrs.offset  = offset;
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CWLSHMBuffer::beginDataPtr(uint32_t flags) {
    return {(uint8_t*)pool->data + offset, fmt, size.x * size.y * 4};
}

void CWLSHMBuffer::endDataPtr() {
    ;
}

bool CWLSHMBuffer::good() {
    return success;
}

void CWLSHMBuffer::update(const CRegion& damage) {
    texture->update(FormatUtils::shmToDRM(fmt), (uint8_t*)pool->data + offset, stride, damage);
}

CSHMPool::CSHMPool(int fd_, size_t size_) : fd(fd_), size(size_) {
    data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
}

CSHMPool::~CSHMPool() {
    munmap(data, size);
    close(fd);
}

void CSHMPool::resize(size_t size_) {
    LOGM(LOG, "Resizing a SHM pool from {} to {}", size, size_);

    if (data)
        munmap(data, size);
    size = size_;
    data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED)
        LOGM(ERR, "Couldn't mmap {} bytes from fd {} of shm client", size, fd);
}

CWLSHMPoolResource::CWLSHMPoolResource(SP<CWlShmPool> resource_, int fd_, size_t size_) : resource(resource_) {
    if (!good())
        return;

    pool = makeShared<CSHMPool>(fd_, size_);

    resource->setDestroy([this](CWlShmPool* r) { PROTO::shm->destroyResource(this); });
    resource->setOnDestroy([this](CWlShmPool* r) { PROTO::shm->destroyResource(this); });

    resource->setResize([this](CWlShmPool* r, int32_t size_) {
        if (size_ < (int32_t)pool->size) {
            r->error(-1, "Shrinking a shm pool is illegal");
            return;
        }
        pool->resize(size_);
    });

    resource->setCreateBuffer([this](CWlShmPool* r, uint32_t id, int32_t offset, int32_t w, int32_t h, int32_t stride, uint32_t fmt) {
        if (!pool || !pool->data) {
            r->error(-1, "The provided shm pool failed to allocate properly");
            return;
        }

        if (std::find(PROTO::shm->shmFormats.begin(), PROTO::shm->shmFormats.end(), fmt) == PROTO::shm->shmFormats.end()) {
            r->error(WL_SHM_ERROR_INVALID_FORMAT, "Format invalid");
            return;
        }

        if (offset < 0 || w <= 0 || h <= 0 || stride <= 0) {
            r->error(WL_SHM_ERROR_INVALID_STRIDE, "Invalid stride, w, h, or offset");
            return;
        }

        const auto RESOURCE = PROTO::shm->m_vBuffers.emplace_back(makeShared<CWLSHMBuffer>(self.lock(), id, offset, Vector2D{w, h}, stride, fmt));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::shm->m_vBuffers.pop_back();
            return;
        }

        // append instance so that buffer knows its owner
        RESOURCE->resource->buffer = RESOURCE;
    });

    if (pool->data == MAP_FAILED)
        resource->error(WL_SHM_ERROR_INVALID_FD, "Couldn't mmap from fd");
}

bool CWLSHMPoolResource::good() {
    return resource->resource();
}

CWLSHMResource::CWLSHMResource(SP<CWlShm> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlShm* r) { PROTO::shm->destroyResource(this); });

    resource->setCreatePool([](CWlShm* r, uint32_t id, int32_t fd, int32_t size) {
        const auto RESOURCE = PROTO::shm->m_vPools.emplace_back(makeShared<CWLSHMPoolResource>(makeShared<CWlShmPool>(r->client(), r->version(), id), fd, size));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::shm->m_vPools.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });

    // send a few supported formats. No need for any other I think?
    for (auto& s : PROTO::shm->shmFormats) {
        resource->sendFormat((wl_shm_format)s);
    }
}

bool CWLSHMResource::good() {
    return resource->resource();
}

CWLSHMProtocol::CWLSHMProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSHMProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    if (shmFormats.empty()) {
        shmFormats.push_back(WL_SHM_FORMAT_ARGB8888);
        shmFormats.push_back(WL_SHM_FORMAT_XRGB8888);

        static const std::array<DRMFormat, 6> supportedShmFourccFormats = {
            DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_XRGB2101010, DRM_FORMAT_ARGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_ABGR2101010,
        };

        for (auto& fmt : g_pHyprOpenGL->getDRMFormats()) {
            if (std::find(supportedShmFourccFormats.begin(), supportedShmFourccFormats.end(), fmt.drmFormat) == supportedShmFourccFormats.end())
                continue;

            shmFormats.push_back(fmt.drmFormat);
        }
    }

    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CWLSHMResource>(makeShared<CWlShm>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CWLSHMProtocol::destroyResource(CWLSHMResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSHMProtocol::destroyResource(CWLSHMPoolResource* resource) {
    std::erase_if(m_vPools, [&](const auto& other) { return other.get() == resource; });
}

void CWLSHMProtocol::destroyResource(CWLSHMBuffer* resource) {
    std::erase_if(m_vBuffers, [&](const auto& other) { return other.get() == resource; });
}
