#include "../BatchManager.hpp"
#include "../OpenGL.hpp"
#include "../../Compositor.hpp"
#include <chrono>
#include <vector>
#include <random>

// Test function to demonstrate instanced rendering performance
void testInstancedRenderingPerformance(CHyprOpenGLImpl* gl) {
    if (!gl)
        return;

    CRenderBatchManager batchManager;
    batchManager.setOpenGLContext(gl);

    // Test parameters
    const int                             NUM_RECTS  = 1000;
    const int                             NUM_FRAMES = 100;

    std::random_device                    rd;
    std::mt19937                          gen(rd());
    std::uniform_real_distribution<float> posDist(0.0f, 1920.0f);
    std::uniform_real_distribution<float> sizeDist(10.0f, 100.0f);
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

    // Generate random rectangles
    struct TestRect {
        CBox       box;
        CHyprColor color;
    };

    std::vector<TestRect> testRects;
    for (int i = 0; i < NUM_RECTS; i++) {
        TestRect rect;
        rect.box   = CBox(posDist(gen), posDist(gen), sizeDist(gen), sizeDist(gen));
        rect.color = CHyprColor(colorDist(gen), colorDist(gen), colorDist(gen), 1.0f);
        testRects.push_back(rect);
    }

    Debug::log(LOG, "Starting instanced rendering performance test with {} rectangles", NUM_RECTS);

    // Test 1: Without instancing
    batchManager.setUseInstancing(false);
    batchManager.resetMetrics();

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        batchManager.beginBatch();
        for (const auto& rect : testRects) {
            batchManager.addRect(rect.box, rect.color, 5, 2.0f);
        }
        batchManager.endBatch();
    }

    auto endTime  = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    Debug::log(LOG, "=== Batched Rendering (No Instancing) ===");
    Debug::log(LOG, "Total time: {} ms", duration / 1000.0f);
    Debug::log(LOG, "Average frame time: {} ms", duration / 1000.0f / NUM_FRAMES);
    batchManager.printPerformanceReport();

    // Test 2: With instancing
    batchManager.setUseInstancing(true);
    batchManager.resetMetrics();

    startTime = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        batchManager.beginBatch();
        for (const auto& rect : testRects) {
            batchManager.addRect(rect.box, rect.color, 5, 2.0f);
        }
        batchManager.endBatch();
    }

    endTime  = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    Debug::log(LOG, "\n=== Instanced Rendering ===");
    Debug::log(LOG, "Total time: {} ms", duration / 1000.0f);
    Debug::log(LOG, "Average frame time: {} ms", duration / 1000.0f / NUM_FRAMES);
    batchManager.printPerformanceReport();

    // Test different batch sizes
    Debug::log(LOG, "\n=== Batch Size Comparison ===");
    const int batchSizes[] = {10, 50, 100, 500, 1000};

    for (int batchSize : batchSizes) {
        batchManager.resetMetrics();
        batchManager.beginBatch();

        for (int i = 0; i < batchSize; i++) {
            batchManager.addRect(testRects[i % NUM_RECTS].box, testRects[i % NUM_RECTS].color, 5, 2.0f);
        }

        batchManager.endBatch();

        auto metrics = batchManager.getMetrics();
        Debug::log(LOG, "Batch size {}: {} draw calls, {} instances, efficiency: {:.1f}%", batchSize, metrics.drawCalls, metrics.instancesRendered,
                   batchManager.getInstancedRenderingEfficiency() * 100.0f);
    }
}