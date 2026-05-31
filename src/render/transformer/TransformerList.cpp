#include "TransformerList.hpp"

using namespace Render;

bool CWindowTransformerList::empty() const {
    return std::ranges::none_of(m_transformers, [](auto const& transformer) { return transformer->active(); });
}

bool CWindowTransformerList::blocksDirectScanout() const {
    return std::ranges::any_of(m_transformers, [](auto const& transformer) { return transformer->blocksDirectScanout(); });
}

CBox CWindowTransformerList::transformedExtents(const CBox& currentBox) const {
    CBox box = currentBox;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        box = transformer->transformedExtents(box);
    }

    return box;
}

CBox CWindowTransformerList::sourceBoxForRender(const CBox& currentBox, const CBox& monitorBox) const {
    CBox box = currentBox;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        box = transformer->sourceBoxForRender(box, monitorBox);
    }

    return box;
}

CBox CWindowTransformerList::transformBoxForDamage(const CBox& currentBox) const {
    CBox box = currentBox;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        box = transformer->transformBoxForDamage(box);
    }

    return box;
}

void CWindowTransformerList::preWindowRender(CSurfacePassElement::SRenderData* pRenderData) const {
    for (auto const& transformer : m_transformers) {
        if (transformer->active())
            transformer->preWindowRender(pRenderData);
    }
}

void CWindowTransformerList::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) const {
    for (auto const& transformer : m_transformers) {
        if (transformer->active())
            transformer->amendTransformedRenderData(currentBox, pMotionBlurData);
    }
}

SP<Render::IFramebuffer> CWindowTransformerList::transform(SP<Render::IFramebuffer> in, const SWindowTransformContext& context) const {
    SP<Render::IFramebuffer> last = in;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        last = transformer->transform(last, context);
    }

    return last;
}

void CWindowTransformerList::removeInactive() {
    std::erase_if(m_transformers, [](auto const& transformer) { return !transformer->active(); });
}

void CWindowTransformerList::sort() {
    std::ranges::stable_sort(m_transformers, [](auto const& lhs, auto const& rhs) { return lhs->priority() < rhs->priority(); });
}
