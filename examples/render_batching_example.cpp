// Example demonstrating render batching usage in Hyprland

#include "../src/render/OpenGL.hpp"
#include "../src/render/BatchManager.hpp"
#include "../src/helpers/Color.hpp"

// Example 1: Basic batching
void exampleBasicBatching(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Begin batching mode
    batchManager->beginBatch();
    
    // Add multiple rectangles with same shader state
    // These will be batched together
    CHyprColor red(1.0f, 0.0f, 0.0f, 1.0f);
    batchManager->addRect(CBox(0, 0, 100, 100), red, 0, 2.0f);
    batchManager->addRect(CBox(100, 0, 100, 100), red, 0, 2.0f);
    batchManager->addRect(CBox(200, 0, 100, 100), red, 0, 2.0f);
    
    // Different rounding - will create a new batch
    CHyprColor blue(0.0f, 0.0f, 1.0f, 1.0f);
    batchManager->addRect(CBox(0, 100, 100, 100), blue, 10, 2.0f);
    batchManager->addRect(CBox(100, 100, 100, 100), blue, 10, 2.0f);
    
    // End batching - all operations are flushed
    batchManager->endBatch();
}

// Example 2: Mixed operation types
void exampleMixedOperations(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    batchManager->beginBatch();
    
    // Add different types of operations
    CHyprColor color(0.5f, 0.5f, 0.5f, 1.0f);
    CBox box(50, 50, 200, 200);
    
    // Shadow
    batchManager->addShadow(box, 10, 2.0f, 20, color);
    
    // Border
    batchManager->addBorder(box, color, 10, 2.0f, 2);
    
    // Rectangle
    batchManager->addRect(box, color, 10, 2.0f);
    
    // Texture (would need real texture ID in practice)
    batchManager->addTexture(123, box, 0.8f, 10, 2.0f);
    
    batchManager->endBatch();
}

// Example 3: Performance monitoring
void exampleWithMetrics(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Reset metrics before starting
    batchManager->resetMetrics();
    
    batchManager->beginBatch();
    
    // Add many operations
    CHyprColor color(1.0f, 0.5f, 0.0f, 1.0f);
    for (int i = 0; i < 100; i++) {
        CBox box(i * 10, 0, 10, 10);
        batchManager->addRect(box, color, 0, 2.0f);
    }
    
    batchManager->endBatch();
    
    // Get performance metrics
    auto metrics = batchManager->getMetrics();
    Debug::log(LOG, "Render batching metrics:");
    Debug::log(LOG, "  Draw calls: {}", metrics.drawCalls);
    Debug::log(LOG, "  State changes: {}", metrics.stateChanges);
    Debug::log(LOG, "  Texture binds: {}", metrics.textureBinds);
}

// Example 4: Auto-flush mode
void exampleAutoFlush(CHyprOpenGLImpl* opengl) {
    auto* batchManager = opengl->getBatchManager();
    
    // Enable auto-flush for large batches
    batchManager->setAutoFlush(true);
    
    batchManager->beginBatch();
    
    // Add many operations - will auto-flush when batch size limit is reached
    CHyprColor green(0.0f, 1.0f, 0.0f, 1.0f);
    for (int i = 0; i < 2000; i++) {
        CBox box(0, 0, 10, 10);
        batchManager->addRect(box, green, 0, 2.0f);
    }
    
    batchManager->endBatch();
}