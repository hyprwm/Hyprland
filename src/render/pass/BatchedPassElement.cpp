#include "BatchedPassElement.hpp"
#include "RectPassElement.hpp"
#include "TexPassElement.hpp"
#include "ShadowPassElement.hpp"
#include "../OpenGL.hpp"
#include "../../Compositor.hpp"

extern UP<CHyprOpenGLImpl> g_pHyprOpenGL;

CBatchedPassElement::CBatchedPassElement() = default;

void CBatchedPassElement::draw(const CRegion& damage) {
    if (!g_pHyprOpenGL)
        return;

    auto* batchManager = g_pHyprOpenGL->getBatchManager();

    // Start batching
    batchManager->beginBatch();

    // Add all batchable elements
    for (const auto& elem : m_batchableElements) {
        switch (elem.type) {
            case SBatchableElement::RECT: batchManager->addRect(elem.box, elem.color, elem.round, elem.roundingPower); break;

            case SBatchableElement::TEXTURE:
                if (elem.texture && elem.texture->m_texID) {
                    batchManager->addTexture(elem.texture->m_texID, elem.box, elem.alpha, elem.round, elem.roundingPower);
                }
                break;

            case SBatchableElement::SHADOW: batchManager->addShadow(elem.box, elem.round, elem.roundingPower, elem.shadowRange, elem.color); break;
        }
    }

    // End batching - this flushes all batched operations
    batchManager->endBatch();

    // Draw unbatchable elements normally
    for (auto& elem : m_unbatchableElements) {
        elem->draw(damage);
    }
}

bool CBatchedPassElement::needsPrecomputeBlur() {
    // Check if any element needs blur
    for (auto& elem : m_unbatchableElements) {
        if (elem->needsPrecomputeBlur())
            return true;
    }
    return false;
}

void CBatchedPassElement::addElement(UP<IPassElement> element) {
    if (!element)
        return;

    if (canBatch(element.get())) {
        extractBatchableData(element.get());
    } else {
        m_unbatchableElements.push_back(std::move(element));
    }
}

void CBatchedPassElement::addBatchableElement(const SBatchableElement& element) {
    m_batchableElements.push_back(element);
}

void CBatchedPassElement::clear() {
    m_unbatchableElements.clear();
    m_batchableElements.clear();
}

bool CBatchedPassElement::canBatch(IPassElement* element) {
    // TODO: Implement pass element batching in future iteration
    // This requires refactoring pass element classes to expose batch-compatible data
    // For now, return false to disable batching
    return false;
}

void CBatchedPassElement::extractBatchableData(IPassElement* element) {
    // TODO: Implement data extraction when pass element batching is enabled
    // This is a placeholder for future implementation
}