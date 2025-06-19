#include "BatchedShadowPassElement.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"
#include "../OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"

void CBatchedShadowPassElement::addShadow(CHyprDropShadowDecoration* deco, float alpha) {
    m_shadows.push_back({deco, alpha});
}

void CBatchedShadowPassElement::draw(const CRegion& damage) {
    if (m_shadows.empty() || !g_pHyprOpenGL)
        return;

    // Check if shadows are enabled
    static auto PSHADOWS = CConfigValue<Hyprlang::INT>("decoration:shadow:enabled");
    if (*PSHADOWS != 1)
        return;

    // Get common shadow parameters
    static auto PSHADOWSHARP = CConfigValue<Hyprlang::INT>("decoration:shadow:sharp");
    static auto PSHADOWSIZE  = CConfigValue<Hyprlang::INT>("decoration:shadow:range");
    const bool  SHARP        = *PSHADOWSHARP;

    // Start batching
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    batchManager->beginBatch();

    // Process all shadows
    for (const auto& shadow : m_shadows) {
        if (!shadow.deco)
            continue;

        // Call the decoration's render method which will use batching
        shadow.deco->renderBatched(shadow.alpha);
    }

    // End batching - this will render all shadows efficiently
    batchManager->endBatch();
}