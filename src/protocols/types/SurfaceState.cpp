#include "SurfaceState.hpp"
#include "helpers/Format.hpp"
#include "protocols/types/Buffer.hpp"
#include "render/Texture.hpp"

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

    bufferDamage = surfaceDamage.scale(scale).transform(wlTransformToHyprutils(invertTransform(transform)), trc.x, trc.y).add(bufferDamage);
    damage.clear();
    return bufferDamage;
}

void SSurfaceState::updateSynchronousTexture(SP<CTexture> lastTexture) {
    auto [dataPtr, fmt, size] = buffer->beginDataPtr(0);
    if (dataPtr) {
        auto drmFmt = NFormatUtils::shmToDRM(fmt);
        auto stride = bufferSize.y ? size / bufferSize.y : 0;
        if (lastTexture && lastTexture->m_isSynchronous && lastTexture->m_vSize == bufferSize) {
            texture = lastTexture;
            texture->update(drmFmt, dataPtr, stride, accumulateBufferDamage());
        } else
            texture = makeShared<CTexture>(drmFmt, dataPtr, stride, bufferSize);
    }
    buffer->endDataPtr();
}

void SSurfaceState::reset() {
    updated.all = false;
    ready       = false;

    // After commit, there is no pending buffer until the next attach.
    buffer = {};

    // it applies only to the buffer that is attached to the surface
    acquire = {};

    // wl_surface.commit assings pending ... and clears pending damage.
    damage.clear();
    bufferDamage.clear();
}

SSurfaceState::SSurfaceState(SSurfaceState&& other) noexcept : acquire(std::move(other.acquire)) {
    ;
}

void SSurfaceState::updateFrom(SSurfaceState& ref) {
    updated = ref.updated;

    if (ref.updated.buffer) {
        buffer     = ref.buffer;
        texture    = ref.texture;
        size       = ref.size;
        bufferSize = ref.bufferSize;
    }

    if (ref.updated.damage) {
        damage       = ref.damage;
        bufferDamage = ref.bufferDamage;
    }

    if (ref.updated.input)
        input = ref.input;

    if (ref.updated.opaque)
        opaque = ref.opaque;

    if (ref.updated.offset)
        offset = ref.offset;

    if (ref.updated.scale)
        scale = ref.scale;

    if (ref.updated.transform)
        transform = ref.transform;

    if (ref.updated.viewport)
        viewport = ref.viewport;
}
