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
    auto [dataPtr, fmt, size] = buffer->buffer->beginDataPtr(0);
    if (dataPtr) {
        auto drmFmt = NFormatUtils::shmToDRM(fmt);
        auto stride = bufferSize.y ? size / bufferSize.y : 0;
        if (lastTexture && lastTexture->m_isSynchronous && lastTexture->m_vSize == bufferSize) {
            texture = lastTexture;
            texture->update(drmFmt, dataPtr, stride, accumulateBufferDamage());
        } else
            texture = makeShared<CTexture>(drmFmt, dataPtr, stride, bufferSize);
    }
    buffer->buffer->endDataPtr();
}

void SSurfaceState::reset() {
    damage.clear();
    bufferDamage.clear();
    transform = WL_OUTPUT_TRANSFORM_NORMAL;
    scale     = 1;
    offset    = {};
    size      = {};
}
