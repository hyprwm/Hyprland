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

SWindowTransformPlan CWindowTransformerList::plan(const CBox& currentBox, const CBox& outputBox) const {
    SWindowTransformPlan plan;
    plan.outputBox = outputBox;

    CBox fullBox = currentBox;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        auto& stage                 = plan.stages.emplace_back();
        stage.fullInputBox          = fullBox;
        stage.fullOutputBox         = transformer->transformedExtents(fullBox);
        stage.allocatesOutputBuffer = transformer->allocatesOutputBuffer();
        fullBox                     = stage.fullOutputBox;
    }

    CBox   requiredBox = outputBox.intersection(fullBox);
    size_t stageIndex  = plan.stages.size();
    for (auto transformer = m_transformers.rbegin(); transformer != m_transformers.rend(); ++transformer) {
        if (!(*transformer)->active())
            continue;

        auto& stage     = plan.stages[--stageIndex];
        stage.outputBox = requiredBox.intersection(stage.fullOutputBox);
        requiredBox     = (*transformer)->sourceBoxForOutput(stage.outputBox, stage.fullInputBox).intersection(stage.fullInputBox);
        stage.inputBox  = requiredBox;
    }

    plan.sourceBox = requiredBox;
    return plan;
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

SWindowTransformBuffer CWindowTransformerList::transform(const SWindowTransformBuffer& in, const SWindowTransformPlan& plan, const SWindowTransformContext& context) const {
    SWindowTransformBuffer last  = in;
    size_t                 stage = 0;
    for (auto const& transformer : m_transformers) {
        if (!transformer->active())
            continue;

        if (stage >= plan.stages.size())
            break;

        auto stageContext       = context;
        stageContext.currentBox = plan.stages[stage].fullInputBox;
        stageContext.inputBox   = plan.stages[stage].inputBox;
        stageContext.outputBox  = plan.stages[stage].outputBox;
        last                    = transformer->transform(last, stageContext);
        if (!last.success)
            break;

        ++stage;
    }

    return last;
}

void CWindowTransformerList::removeInactive() {
    std::erase_if(m_transformers, [](auto const& transformer) { return !transformer->active(); });
}

void CWindowTransformerList::sort() {
    std::ranges::stable_sort(m_transformers, [](auto const& lhs, auto const& rhs) { return lhs->priority() < rhs->priority(); });
}
