#include "Transformer.hpp"

#include <cmath>

using namespace Render;

CBox Render::pixelBoxForLogical(const CBox& box, double scale) {
    if (!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.w) || !std::isfinite(box.h) || !std::isfinite(scale) || box.w <= 0.0 || box.h <= 0.0 || scale <= 0.0)
        return {};

    const double x1 = std::floor(box.x * scale);
    const double y1 = std::floor(box.y * scale);
    const double x2 = std::ceil((box.x + box.w) * scale);
    const double y2 = std::ceil((box.y + box.h) * scale);
    return {x1, y1, x2 - x1, y2 - y1};
}

void IWindowTransformer::preWindowRender(CSurfacePassElement::SRenderData* pRenderData) {
    ;
}

int IWindowTransformer::priority() const {
    return 0;
}

bool IWindowTransformer::active() const {
    return true;
}

bool IWindowTransformer::blocksDirectScanout() const {
    return active();
}

bool IWindowTransformer::allocatesOutputBuffer() const {
    return true;
}

CBox IWindowTransformer::transformedExtents(const CBox& currentBox) const {
    return currentBox;
}

CBox IWindowTransformer::sourceBoxForOutput(const CBox&, const CBox& inputBox) const {
    return inputBox;
}

CBox IWindowTransformer::transformBoxForDamage(const CBox& currentBox) const {
    return transformedExtents(currentBox);
}

void IWindowTransformer::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) {
    ;
}
