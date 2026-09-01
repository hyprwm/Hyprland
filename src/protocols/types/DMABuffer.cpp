#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"
#include "helpers/Drm.hpp"
#include <hyprgraphics/egl/Egl.hpp>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <cstring>
#include <cstdlib>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>

// reading a write-combined dmabuf mapping with normal loads is uncached and extremely slow
// (~0.4 GB/s); MOVNTDQA streaming loads are the intended way to read WC memory.
__attribute__((target("sse4.1"))) static void streamLoadCopySSE41(uint8_t* dst, const uint8_t* src, size_t len) {
    size_t i = 0;
    if (const size_t HEAD = (16 - ((uintptr_t)src & 15)) & 15; HEAD) {
        const size_t N = HEAD < len ? HEAD : len;
        memcpy(dst, src, N);
        i = N;
    }
    for (; i + 64 <= len; i += 64) {
        __m128i a = _mm_stream_load_si128((__m128i*)(src + i));
        __m128i b = _mm_stream_load_si128((__m128i*)(src + i + 16));
        __m128i c = _mm_stream_load_si128((__m128i*)(src + i + 32));
        __m128i d = _mm_stream_load_si128((__m128i*)(src + i + 48));
        _mm_storeu_si128((__m128i*)(dst + i), a);
        _mm_storeu_si128((__m128i*)(dst + i + 16), b);
        _mm_storeu_si128((__m128i*)(dst + i + 32), c);
        _mm_storeu_si128((__m128i*)(dst + i + 48), d);
    }
    if (i < len)
        memcpy(dst + i, src + i, len - i);
}
#endif

static void wcReadCopy(uint8_t* dst, const uint8_t* src, size_t len) {
#if defined(__x86_64__) || defined(__i386__)
    if (__builtin_cpu_supports("sse4.1")) {
        streamLoadCopySSE41(dst, src, len);
        return;
    }
#endif
    memcpy(dst, src, len);
}

using namespace Hyprutils::OS;
using namespace Hyprgraphics::Egl;

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs const& attrs_) : m_attrs(attrs_) {
    m_listeners.resourceDestroy = events.destroy.listen([this] {
        closeFDs();
        m_listeners.resourceDestroy.reset();
    });

    size       = m_attrs.size;
    m_resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));
    m_opaque   = isDrmFormatOpaque(m_attrs.format);
    m_texture  = g_pHyprRenderer->createTexture(m_attrs, m_opaque); // texture takes ownership of the eglImage

    if UNLIKELY (!m_texture) {
        const auto EXPLICITMODIFIER = m_attrs.modifier;
        Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage, retrying as implicit");
        m_attrs.modifier = DRM_FORMAT_MOD_INVALID;
        m_texture        = g_pHyprRenderer->createTexture(m_attrs, m_opaque);

        if UNLIKELY (!m_texture) {
            // a single-plane explicitly-linear buffer the GPU refuses to sample (e.g. a
            // cross-device buffer with a stride the render GPU can't import) can still be
            // read by the CPU: treat it like an shm buffer and upload the pixels each commit.
            if (m_attrs.planes == 1 && EXPLICITMODIFIER == DRM_FORMAT_MOD_LINEAR) {
                Log::logger->log(Log::WARN, "CDMABuffer: EGL import failed, using CPU-copy fallback for linear buffer");
                m_attrs.modifier = EXPLICITMODIFIER;
                m_cpuFallback    = true;
                m_success        = true;
                return;
            }
            Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }
    }

    m_success = m_texture->ok();

    if UNLIKELY (!m_success)
        Log::logger->log(Log::ERR, "Failed to create a dmabuf: texture is null");
}

CDMABuffer::~CDMABuffer() {
    if (m_resource)
        m_resource->sendRelease();

    closeFDs();
}

Aquamarine::eBufferCapability CDMABuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CDMABuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_DMABUF;
}

void CDMABuffer::update(const CRegion& damage) {
    ;
}

bool CDMABuffer::isSynchronous() {
    return m_cpuFallback; // the CPU-copy fallback reads the buffer at commit, like shm
}

Aquamarine::SDMABUFAttrs CDMABuffer::dmabuf() {
    return m_attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CDMABuffer::beginDataPtr(uint32_t flags) {
    if (!m_cpuFallback || m_attrs.planes != 1 || m_attrs.fds[0] < 0)
        return {nullptr, 0, 0};

    const size_t LEN = (size_t)m_attrs.strides[0] * m_attrs.size.y;

    if (!m_map) {
        m_mapSize = m_attrs.offsets[0] + LEN;
        void* map = mmap(nullptr, m_mapSize, PROT_READ, MAP_SHARED, m_attrs.fds[0], 0);
        if (map == MAP_FAILED) {
            Log::logger->log(Log::ERR, "CDMABuffer: CPU fallback mmap failed");
            m_mapSize = 0;
            return {nullptr, 0, 0};
        }
        m_map = map;
    }

    if (!m_staging) {
        m_staging = aligned_alloc(64, (LEN + 63) & ~(size_t)63);
        if (!m_staging) {
            Log::logger->log(Log::ERR, "CDMABuffer: CPU fallback staging alloc failed");
            return {nullptr, 0, 0};
        }
    }

    // the mapping is typically write-combined: bracket a streaming-load copy into cached
    // staging memory with the sync ioctls, and let GL read the fast staging copy instead.
    dma_buf_sync sync = {.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ};
    ioctl(m_attrs.fds[0], DMA_BUF_IOCTL_SYNC, &sync);

    wcReadCopy((uint8_t*)m_staging, (uint8_t*)m_map + m_attrs.offsets[0], LEN);

    sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
    ioctl(m_attrs.fds[0], DMA_BUF_IOCTL_SYNC, &sync);

    // returned length must be exactly stride * height: SSurfaceState::updateSynchronousTexture
    // derives the stride from it as len / bufferSize.y
    return {(uint8_t*)m_staging, m_attrs.format, LEN};
}

void CDMABuffer::endDataPtr() {
    // nothing to do: the dmabuf sync bracket is fully handled inside beginDataPtr,
    // the caller only ever sees the staging copy
}

bool CDMABuffer::good() {
    return m_success;
}

void CDMABuffer::closeFDs() {
    if (m_map) {
        munmap(m_map, m_mapSize);
        m_map     = nullptr;
        m_mapSize = 0;
    }
    if (m_staging) {
        free(m_staging);
        m_staging = nullptr;
    }
    for (int i = 0; i < m_attrs.planes; ++i) {
        if (m_attrs.fds[i] == -1)
            continue;
        close(m_attrs.fds[i]);
        m_attrs.fds[i] = -1;
    }
    m_attrs.planes = 0;
}

std::vector<CFileDescriptor> CDMABuffer::exportSyncFiles() {
    if (!good())
        return {};

#ifndef __linux__
    return {};
#else
    std::vector<CFileDescriptor> syncFds;
    syncFds.reserve(m_attrs.fds.size());

    for (const auto& fd : m_attrs.fds) {
        if (fd == -1)
            continue;

        // buffer readability checks are rather slow on some Intel laptops
        // see https://gitlab.freedesktop.org/drm/intel/-/issues/9415
        if (g_pHyprRenderer && !g_pHyprRenderer->isIntel()) {
            if (CFileDescriptor::isReadable(fd))
                continue;
        }

        CFileDescriptor fence = DRM::exportFence(fd);
        if (fence.isValid())
            syncFds.emplace_back(std::move(fence));
    }

    return syncFds;
#endif
}
