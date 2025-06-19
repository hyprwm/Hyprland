/**
 * Example: How to use render batching in Hyprland
 * 
 * This example demonstrates how to integrate the render batching system
 * to reduce draw calls and improve rendering performance.
 */

#include "../src/render/OpenGL.hpp"
#include "../src/render/BatchManager.hpp"
#include "../src/render/pass/Pass.hpp"
#include "../src/render/pass/BatchedPassElement.hpp"
#include "../src/render/pass/RectPassElement.hpp"
#include "../src/render/pass/TexPassElement.hpp"

// Example 1: Direct batching API usage
void exampleDirectBatching() {
    // Get the batch manager from OpenGL
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    // Begin batching operations
    batchManager->beginBatch();
    
    // Add multiple rectangles - these will be grouped by state
    CHyprColor red(1.0f, 0.0f, 0.0f, 1.0f);
    CHyprColor blue(0.0f, 0.0f, 1.0f, 1.0f);
    
    // These rectangles have the same rounding, so they'll be batched together
    for (int i = 0; i < 10; i++) {
        CBox box(i * 50, 0, 45, 45);
        batchManager->addRect(box, red, 5, 2.0f);
    }
    
    // These have different rounding, so they'll be a separate batch
    for (int i = 0; i < 10; i++) {
        CBox box(i * 50, 50, 45, 45);
        batchManager->addRect(box, blue, 10, 2.0f);
    }
    
    // End batching - this flushes all pending operations
    batchManager->endBatch();
    
    // Check performance metrics
    const auto& stats = batchManager->getStats();
    Debug::log(LOG, "Batching stats: {} total draw calls, {} batched calls, {} state changes",
               stats.totalDrawCalls, stats.batchedDrawCalls, stats.stateChanges);
}

// Example 2: Using batching with the Pass system
void examplePassSystemBatching(CRenderPass& renderPass) {
    // Create a batched pass element
    auto batchedPass = makeShared<CBatchedPassElement>();
    
    // Add multiple rect elements to be batched
    CHyprColor green(0.0f, 1.0f, 0.0f, 0.8f);
    
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            CBox box(x * 100 + 10, y * 100 + 10, 80, 80);
            
            auto rectData = CRectPassElement::SRectData{
                .box = box,
                .color = green,
                .round = 10,
                .roundingPower = 2.0f,
                .blur = false
            };
            
            batchedPass->addElement(makeShared<CRectPassElement>(rectData));
        }
    }
    
    // Add the batched pass to the render pass
    renderPass.add(batchedPass);
}

// Example 3: Batching textures
void exampleTextureBatching(CRenderPass& renderPass, SP<CTexture> iconTexture) {
    auto batchedPass = makeShared<CBatchedPassElement>();
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    batchManager->beginBatch();
    
    // Draw multiple instances of the same texture
    // These will be batched together since they use the same texture
    for (int i = 0; i < 20; i++) {
        CBox box(i * 32, 100, 32, 32);
        batchManager->addTexture(iconTexture, box, 1.0f, 0, 2.0f);
    }
    
    batchManager->endBatch();
}

// Example 4: Mixed batching with manual flush control
void exampleMixedBatching() {
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    batchManager->beginBatch();
    
    // Add some rectangles
    CHyprColor color(0.5f, 0.5f, 0.5f, 1.0f);
    for (int i = 0; i < 5; i++) {
        CBox box(i * 20, 0, 15, 15);
        batchManager->addRect(box, color, 0, 2.0f);
    }
    
    // Manually flush rectangles before adding textures
    batchManager->flush(CRenderBatchManager::BATCH_RECT);
    
    // Now add textures - they'll be in a separate batch
    // ... texture operations ...
    
    // End batching
    batchManager->endBatch();
}

// Example 5: Conditional batching based on performance needs
void exampleConditionalBatching(bool highPerformanceMode) {
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    if (highPerformanceMode) {
        // Use batching for better performance
        batchManager->beginBatch();
        
        // ... render operations ...
        
        batchManager->endBatch();
    } else {
        // Render immediately without batching
        // The batch manager will automatically render immediately
        // when not in batch mode
        
        CHyprColor color(1.0f, 1.0f, 1.0f, 1.0f);
        CBox box(0, 0, 100, 100);
        batchManager->addRect(box, color, 0, 2.0f);
    }
}

// Example 6: Performance monitoring
void examplePerformanceMonitoring() {
    auto* batchManager = g_pHyprOpenGL->getBatchManager();
    
    // Reset stats before measuring
    batchManager->resetStats();
    
    // Perform rendering operations
    batchManager->beginBatch();
    
    // ... lots of render operations ...
    
    batchManager->endBatch();
    
    // Get performance metrics
    const auto& stats = batchManager->getStats();
    
    // Calculate improvement
    float drawCallReduction = 0.0f;
    if (stats.totalDrawCalls > 0) {
        drawCallReduction = (1.0f - (float)stats.batchedDrawCalls / stats.totalDrawCalls) * 100.0f;
    }
    
    Debug::log(LOG, "Batching reduced draw calls by {:.1f}% ({} -> {} calls)",
               drawCallReduction, stats.totalDrawCalls, stats.batchedDrawCalls);
    Debug::log(LOG, "State changes: {}, Texture binds: {}",
               stats.stateChanges, stats.textureBinds);
}