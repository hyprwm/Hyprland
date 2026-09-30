#include "WorkBufferPool.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../render/Framebuffer.hpp"
#include <hyprgraphics/egl/Egl.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Monitor;
using namespace std::chrono_literals;

static constexpr size_t        MAX_WORK_BUFFERS = 8;
static constexpr auto          IDLE_TIMEOUT     = 5s;
static constexpr auto          POLL_INTERVAL    = 1s;

static std::optional<uint64_t> bufferBytes(const Vector2D& size, DRMFormat format, uint64_t limit) {
    // Validate before converting the floating-point dimensions to alloc()'s ints.
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0 || size.y <= 0 || size.x > std::numeric_limits<int>::max() || size.y > std::numeric_limits<int>::max() ||
        std::floor(size.x) != size.x || std::floor(size.y) != size.y)
        return std::nullopt;

    const auto FORMAT = Hyprgraphics::Egl::getPixelFormatFromDRM(format);
    if (!FORMAT || !FORMAT->bytesPerBlock || Hyprgraphics::Egl::pixelsPerBlock(FORMAT) != 1)
        return std::nullopt;

    const auto PIXELS = sc<uint64_t>(size.x) * sc<uint64_t>(size.y);
    if (PIXELS > limit / FORMAT->bytesPerBlock)
        return std::nullopt;

    return PIXELS * FORMAT->bytesPerBlock;
}

CWorkBufferPool::CWorkBufferPool(std::function<SP<Render::IFramebuffer>()> createBuffer, std::function<void()> prepareRelease, uint64_t byteLimit) :
    m_createBuffer(std::move(createBuffer)), m_prepareRelease(std::move(prepareRelease)), m_byteLimit(byteLimit) {
    ;
}

CWorkBufferPool::~CWorkBufferPool() {
    if (m_expiryTimer) {
        m_expiryTimer->cancel();
        if (g_pEventLoopManager)
            g_pEventLoopManager->removeTimer(m_expiryTimer);
    }

    if (!m_resources.empty())
        m_prepareRelease();
}

void CWorkBufferPool::armTimer() {
    if (!m_expiryTimer) {
        m_expiryTimer = makeShared<CEventLoopTimer>(POLL_INTERVAL, [this](SP<CEventLoopTimer>, void*) { expire(); }, nullptr);
        g_pEventLoopManager->addTimer(m_expiryTimer);
    } else if (!m_expiryTimer->armed())
        m_expiryTimer->updateTimeout(POLL_INTERVAL);
}

void CWorkBufferPool::expire() {
    const auto NOW      = Time::steadyNow();
    bool       prepared = false;
    std::erase_if(m_resources, [&](auto& resource) {
        if (resource.buffer.strongRef() > 1) {
            resource.idleSince.reset();
            return false;
        }

        // References can be held across timer ticks. Start the idle grace period
        // when we observe their release, rather than aging an in-use allocation.
        if (!resource.idleSince)
            resource.idleSince = NOW;
        if (NOW - *resource.idleSince < IDLE_TIMEOUT)
            return false;

        if (!prepared) {
            m_prepareRelease();
            prepared = true;
        }
        return true;
    });

    // No periodic wakeups once the pool is empty. acquire() rearms it.
    if (!m_resources.empty())
        armTimer();
}

SP<Render::IFramebuffer> CWorkBufferPool::acquire(const Vector2D& size, DRMFormat format, NColorManagement::PImageDescription imageDescription) {
    const auto BYTES = bufferBytes(size, format, m_byteLimit);
    if (!BYTES)
        return nullptr;

    auto exact = std::ranges::find_if(m_resources, [&](const auto& resource) {
        return resource.buffer.strongRef() == 1 && resource.buffer->isAllocated() && resource.buffer->m_size == size && resource.buffer->m_drmFormat == format;
    });
    if (exact != m_resources.end()) {
        exact->idleSince.reset();
        exact->buffer->setImageDescription(imageDescription);
        return exact->buffer;
    }

    uint64_t allocatedBytes = 0;
    uint64_t busyBytes      = 0;
    for (const auto& resource : m_resources) {
        allocatedBytes += resource.bytes;
        if (resource.buffer.strongRef() > 1)
            busyBytes += resource.bytes;
    }
    if (*BYTES > m_byteLimit - busyBytes)
        return nullptr;

    // Keep a bounded warm cache of recurring sizes. Resize an idle entry only
    // when retaining it alongside the new allocation would exceed either limit.
    SP<Render::IFramebuffer> buffer;
    if (m_resources.size() >= MAX_WORK_BUFFERS || *BYTES > m_byteLimit - allocatedBytes) {
        auto idle = std::ranges::find_if(m_resources, [](const auto& resource) { return resource.buffer.strongRef() == 1; });
        if (idle == m_resources.end())
            return nullptr;

        allocatedBytes -= idle->bytes;
        buffer = std::move(idle->buffer);
        m_resources.erase(idle);
    }

    for (auto resource = m_resources.begin(); resource != m_resources.end() && *BYTES > m_byteLimit - allocatedBytes;) {
        if (resource->buffer.strongRef() > 1) {
            ++resource;
            continue;
        }
        allocatedBytes -= resource->bytes;
        resource = m_resources.erase(resource);
    }

    if (!buffer)
        buffer = m_createBuffer();
    // A failed resize may have already destroyed the old allocation. Drop it
    // instead of retaining stale size/accounting or returning an invalid FB.
    if (!buffer || !buffer->alloc(sc<int>(size.x), sc<int>(size.y), format))
        return nullptr;

    buffer->setImageDescription(imageDescription);
    m_resources.push_back({
        .buffer = buffer,
        .bytes  = *BYTES,
    });
    armTimer();
    return buffer;
}

void CWorkBufferPool::setImageDescription(NColorManagement::PImageDescription imageDescription) {
    for (const auto& resource : m_resources)
        resource.buffer->setImageDescription(imageDescription);
}

void CWorkBufferPool::forEachUnused(const std::function<void(SP<Render::IFramebuffer>)>& callback) {
    for (const auto& resource : m_resources) {
        if (resource.buffer.strongRef() > 1)
            continue;
        callback(resource.buffer);
    }
}
