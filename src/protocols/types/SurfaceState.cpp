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
        if (lastTexture && lastTexture->m_isSynchronous && lastTexture->m_size == bufferSize) {
            texture = lastTexture;
            texture->update(drmFmt, dataPtr, stride, accumulateBufferDamage());
        } else
            texture = makeShared<CTexture>(drmFmt, dataPtr, stride, bufferSize);
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
}

void SSurfaceState::updateFrom(SSurfaceState& ref) {
    updated = ref.updated;

    if (ref.updated.bits.buffer) {
        if (!ref.buffer.m_buffer)
            texture.reset(); // null buffer reset texture.

        buffer     = ref.buffer;
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

    if (ref.updated.bits.input)
        input = ref.input;

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
}
