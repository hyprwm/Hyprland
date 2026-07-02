#include "Transformer.hpp"

using namespace Render;

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

CBox IWindowTransformer::transformedExtents(const CBox& currentBox) const {
    return currentBox;
}

CBox IWindowTransformer::sourceBoxForRender(const CBox& currentBox, const CBox&) const {
    return currentBox;
}

CBox IWindowTransformer::transformBoxForDamage(const CBox& currentBox) const {
    return transformedExtents(currentBox);
}

void IWindowTransformer::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) {
    ;
}
