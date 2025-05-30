#include "Shm.hpp"
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <drm_fourcc.h>
#include "../../render/Texture.hpp"
#include "../types/WLBuffer.hpp"
#include "../../helpers/Format.hpp"
#include "../../render/Renderer.hpp"
using namespace Hyprutils::OS;

CWLSHMBuffer::CWLSHMBuffer(SP<CWLSHMPoolResource> pool_, uint32_t id, int32_t offset_, const Vector2D& size_, int32_t stride_, uint32_t fmt_) {
    if UNLIKELY (!pool_->m_pool->m_data)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    size     = size_;
    m_pool   = pool_->m_pool;
    m_stride = stride_;
    m_fmt    = fmt_;
    m_offset = offset_;
    m_opaque = NFormatUtils::isFormatOpaque(NFormatUtils::shmToDRM(fmt_));

    m_resource = CWLBufferResource::create(makeShared<CWlBuffer>(pool_->m_resource->client(), 1, id));

    m_listeners.bufferResourceDestroy = events.destroy.registerListener([this](std::any d) {
        m_listeners.bufferResourceDestroy.reset();
        PROTO::shm->destroyResource(this);
    });
}

CWLSHMBuffer::~CWLSHMBuffer() {
    if (m_resource)
        m_resource->sendRelease();
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
    attrs.fd      = m_pool->m_fd.get();
    attrs.format  = NFormatUtils::shmToDRM(m_fmt);
    attrs.size    = size;
    attrs.stride  = m_stride;
    attrs.offset  = m_offset;
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CWLSHMBuffer::beginDataPtr(uint32_t flags) {
    return {(uint8_t*)m_pool->m_data + m_offset, m_fmt, m_stride * size.y};
}

void CWLSHMBuffer::endDataPtr() {
    ;
}

bool CWLSHMBuffer::good() {
    return true;
}

void CWLSHMBuffer::update(const CRegion& damage) {
    ;
}

CSHMPool::CSHMPool(CFileDescriptor fd_, size_t size_) : m_fd(std::move(fd_)), m_size(size_), m_data(mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd.get(), 0)) {
    ;
}

CSHMPool::~CSHMPool() {
    munmap(m_data, m_size);
}

void CSHMPool::resize(size_t size_) {
    LOGM(LOG, "Resizing a SHM pool from {} to {}", m_size, size_);

    if (m_data)
        munmap(m_data, m_size);
    m_size = size_;
    m_data = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd.get(), 0);

    if UNLIKELY (m_data == MAP_FAILED)
        LOGM(ERR, "Couldn't mmap {} bytes from fd {} of shm client", m_size, m_fd.get());
}

static int shmIsSizeValid(CFileDescriptor& fd, size_t size) {
    struct stat st;
    if UNLIKELY (fstat(fd.get(), &st) == -1) {
        LOGM(ERR, "Couldn't get stat for fd {} of shm client", fd.get());
        return 0;
    }

    return (size_t)st.st_size >= size;
}

CWLSHMPoolResource::CWLSHMPoolResource(SP<CWlShmPool> resource_, CFileDescriptor fd_, size_t size_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    if UNLIKELY (!shmIsSizeValid(fd_, size_)) {
        resource_->error(-1, "The size of the file is not big enough for the shm pool");
        return;
    }

    m_pool = makeShared<CSHMPool>(std::move(fd_), size_);

    m_resource->setDestroy([this](CWlShmPool* r) { PROTO::shm->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlShmPool* r) { PROTO::shm->destroyResource(this); });

    m_resource->setResize([this](CWlShmPool* r, int32_t size_) {
        if UNLIKELY (size_ < (int32_t)m_pool->m_size) {
            r->error(-1, "Shrinking a shm pool is illegal");
            return;
        }
        if UNLIKELY (!shmIsSizeValid(m_pool->m_fd, size_)) {
            r->error(-1, "The size of the file is not big enough for the shm pool");
            return;
        }

        m_pool->resize(size_);
    });

    m_resource->setCreateBuffer([this](CWlShmPool* r, uint32_t id, int32_t offset, int32_t w, int32_t h, int32_t stride, uint32_t fmt) {
        if UNLIKELY (!m_pool || !m_pool->m_data) {
            r->error(-1, "The provided shm pool failed to allocate properly");
            return;
        }

        if UNLIKELY (std::find(PROTO::shm->m_shmFormats.begin(), PROTO::shm->m_shmFormats.end(), fmt) == PROTO::shm->m_shmFormats.end()) {
            r->error(WL_SHM_ERROR_INVALID_FORMAT, "Format invalid");
            return;
        }

        if UNLIKELY (offset < 0 || w <= 0 || h <= 0 || stride <= 0) {
            r->error(WL_SHM_ERROR_INVALID_STRIDE, "Invalid stride, w, h, or offset");
            return;
        }

        const auto RESOURCE = PROTO::shm->m_buffers.emplace_back(makeShared<CWLSHMBuffer>(m_self.lock(), id, offset, Vector2D{w, h}, stride, fmt));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::shm->m_buffers.pop_back();
            return;
        }

        // append instance so that buffer knows its owner
        RESOURCE->m_resource->m_buffer = RESOURCE;
    });

    if UNLIKELY (m_pool->m_data == MAP_FAILED)
        m_resource->error(WL_SHM_ERROR_INVALID_FD, "Couldn't mmap from fd");
}

bool CWLSHMPoolResource::good() {
    return m_resource->resource();
}

CWLSHMResource::CWLSHMResource(SP<CWlShm> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlShm* r) { PROTO::shm->destroyResource(this); });

    m_resource->setCreatePool([](CWlShm* r, uint32_t id, int32_t fd, int32_t size) {
        CFileDescriptor poolFd{fd};
        const auto      RESOURCE = PROTO::shm->m_pools.emplace_back(makeShared<CWLSHMPoolResource>(makeShared<CWlShmPool>(r->client(), r->version(), id), std::move(poolFd), size));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::shm->m_pools.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });

    // send a few supported formats. No need for any other I think?
    for (auto const& s : PROTO::shm->m_shmFormats) {
        m_resource->sendFormat((wl_shm_format)s);
    }
}

bool CWLSHMResource::good() {
    return m_resource->resource();
}

CWLSHMProtocol::CWLSHMProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSHMProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    if (m_shmFormats.empty()) {
        m_shmFormats.push_back(WL_SHM_FORMAT_ARGB8888);
        m_shmFormats.push_back(WL_SHM_FORMAT_XRGB8888);

        static const std::array<DRMFormat, 6> supportedShmFourccFormats = {
            DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_XRGB2101010, DRM_FORMAT_ARGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_ABGR2101010,
        };

        for (auto const& fmt : supportedShmFourccFormats) {
            m_shmFormats.push_back(fmt);
        }
    }

    const auto RESOURCE = m_managers.emplace_back(makeShared<CWLSHMResource>(makeShared<CWlShm>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CWLSHMProtocol::destroyResource(CWLSHMResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSHMProtocol::destroyResource(CWLSHMPoolResource* resource) {
    std::erase_if(m_pools, [&](const auto& other) { return other.get() == resource; });
}

void CWLSHMProtocol::destroyResource(CWLSHMBuffer* resource) {
    std::erase_if(m_buffers, [&](const auto& other) { return other.get() == resource; });
}
