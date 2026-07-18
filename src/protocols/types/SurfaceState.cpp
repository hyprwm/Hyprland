#include "SurfaceState.hpp"
#include "helpers/Format.hpp"
#include "protocols/types/Buffer.hpp"
#include "render/Renderer.hpp"
#include "render/Texture.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

static bool isInInt32Range(const double value) {
    return std::isfinite(value) && value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max();
}

Vector2D SSurfaceState::sourceSize() {
    if UNLIKELY (!texture)
        return {};

    if UNLIKELY (viewport.hasSource)
        return viewport.source.size();

    Vector2D trc = transform % 2 == 1 ? Vector2D{bufferSize.y, bufferSize.x} : bufferSize;
    return trc / scale;
}

CRegion SSurfaceState::accumulateBufferDamage() {
    if (damage.empty())
        return bufferDamage;

    CRegion surfaceDamage = damage;
    if (viewport.hasDestination) {
        Vector2D scale = sourceSize() / viewport.destination;
        surfaceDamage.scale(scale);
    }

    if (viewport.hasSource)
        surfaceDamage.translate(viewport.source.pos());

    Vector2D trc = transform % 2 == 1 ? Vector2D{bufferSize.y, bufferSize.x} : bufferSize;

    bufferDamage = surfaceDamage.scale(scale).transform(Math::wlTransformToHyprutils(Math::invertTransform(transform)), trc.x, trc.y).add(bufferDamage);
    damage.clear();
    return bufferDamage;
}

CRegion SSurfaceState::effectiveInputRegion() const {
    if (!isInInt32Range(size.x) || !isInInt32Range(size.y) || size.x <= 0 || size.y <= 0)
        return {};

    const auto width  = sc<int32_t>(size.x);
    const auto height = sc<int32_t>(size.y);

    if (inputIsInfinite)
        return CRegion{0, 0, width, height};

    return input.copy().intersect(0, 0, width, height);
}

bool SSurfaceState::inputContainsPoint(const Vector2D& point, const Vector2D& offset) const {
    if (!isInInt32Range(point.x) || !isInInt32Range(point.y) || !isInInt32Range(offset.x) || !isInInt32Range(offset.y) || !isInInt32Range(size.x) || !isInInt32Range(size.y) ||
        size.x <= 0 || size.y <= 0)
        return false;

    // Pixman truncates region translations and query coordinates separately.
    const auto localX = sc<int64_t>(sc<int32_t>(point.x)) - sc<int32_t>(offset.x);
    const auto localY = sc<int64_t>(sc<int32_t>(point.y)) - sc<int32_t>(offset.y);
    const auto width  = sc<int32_t>(size.x);
    const auto height = sc<int32_t>(size.y);

    if (localX < 0 || localY < 0 || localX >= width || localY >= height)
        return false;

    if (!inputIsInfinite && !input.containsPoint({sc<int32_t>(localX), sc<int32_t>(localY)}))
        return false;

    int64_t x1 = 0, y1 = 0, x2 = width, y2 = height;
    if (!inputIsInfinite) {
        const auto extents = pixman_region32_extents(input.pixman());
        x1                 = std::max<int64_t>(0, extents->x1);
        y1                 = std::max<int64_t>(0, extents->y1);
        x2                 = std::min<int64_t>(width, extents->x2);
        y2                 = std::min<int64_t>(height, extents->y2);
    }

    const auto offsetX = sc<int32_t>(offset.x);
    const auto offsetY = sc<int32_t>(offset.y);
    const auto intMin  = std::numeric_limits<int32_t>::min();
    const auto intMax  = std::numeric_limits<int32_t>::max();

    return offsetX + x1 >= intMin && offsetY + y1 >= intMin && offsetX + x2 <= intMax && offsetY + y2 <= intMax;
}

void SSurfaceState::updateSynchronousTexture(SP<Render::ITexture> lastTexture) {
    auto [dataPtr, fmt, size] = buffer->beginDataPtr(0);
    if (dataPtr) {
        auto drmFmt = NFormatUtils::shmToDRM(fmt);
        auto stride = bufferSize.y ? size / bufferSize.y : 0;
        if (lastTexture && lastTexture->m_isSynchronous && lastTexture->m_size == bufferSize) {
            texture = lastTexture;
            texture->update(drmFmt, dataPtr, stride, accumulateBufferDamage());
        } else
            texture = g_pHyprRenderer->createTexture(drmFmt, dataPtr, stride, bufferSize);
    }
    buffer->endDataPtr();
}

void SSurfaceState::reset() {
    updated.all = false;

    // After commit, there is no pending buffer until the next attach.
    buffer = {};

    // applies only to the buffer that is attached to the surface
    acquire = {};

    // wl_surface.commit assigns pending ... and clears pending damage.
    damage.clear();
    bufferDamage.clear();

    callbacks.clear();
    presentationFeedbacks.clear();
    lockMask = LOCK_REASON_NONE;

    barrierSet    = false;
    surfaceLocked = false;
    fifoScheduled = false;

    pendingTimeout.reset();
    commitTimingTarget.reset();
    timer.reset(); // CEventLoopManager::nudgeTimers should handle it eventually
}

void SSurfaceState::updateFrom(SSurfaceState& ref) {
    updated = ref.updated;

    if (ref.updated.bits.buffer) {
        buffer     = ref.buffer;
        texture    = ref.texture;
        size       = ref.size;
        bufferSize = ref.bufferSize;
    }

    if (ref.updated.bits.damage) {
        damage       = ref.damage;
        bufferDamage = ref.bufferDamage;
    } else {
        // damage is always relative to the current commit
        damage.clear();
        bufferDamage.clear();
    }

    if (ref.updated.bits.input) {
        input           = ref.input;
        inputIsInfinite = ref.inputIsInfinite;
    }

    if (ref.updated.bits.opaque)
        opaque = ref.opaque;

    if (ref.updated.bits.offset)
        offset = ref.offset;

    if (ref.updated.bits.scale)
        scale = ref.scale;

    if (ref.updated.bits.transform)
        transform = ref.transform;

    if (ref.updated.bits.viewport)
        viewport = ref.viewport;

    if (ref.updated.bits.acquire)
        acquire = ref.acquire;

    if (ref.updated.bits.acked)
        ackedSize = ref.ackedSize;

    if (ref.updated.bits.frame) {
        callbacks.insert(callbacks.end(), std::make_move_iterator(ref.callbacks.begin()), std::make_move_iterator(ref.callbacks.end()));
        ref.callbacks.clear();
    }

    if (!ref.presentationFeedbacks.empty()) {
        presentationFeedbacks.insert(presentationFeedbacks.end(), std::make_move_iterator(ref.presentationFeedbacks.begin()),
                                     std::make_move_iterator(ref.presentationFeedbacks.end()));
        ref.presentationFeedbacks.clear();
    }

    if (ref.barrierSet)
        barrierSet = ref.barrierSet;
}
