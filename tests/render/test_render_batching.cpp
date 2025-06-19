#include <gtest/gtest.h>
#include "../../src/render/BatchManager.hpp"
#include "../../src/render/OpenGL.hpp"
#include "../../src/helpers/Color.hpp"
#include "../../src/helpers/math/Math.hpp"

class BatchManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        batchManager = std::make_unique<CRenderBatchManager>();
    }

    void TearDown() override {
        batchManager.reset();
    }

    std::unique_ptr<CRenderBatchManager> batchManager;
};

TEST_F(BatchManagerTest, BeginEndBatch) {
    // Test basic begin/end functionality
    batchManager->beginBatch();
    EXPECT_TRUE(batchManager->isBatching());
    
    batchManager->endBatch();
    EXPECT_FALSE(batchManager->isBatching());
}

TEST_F(BatchManagerTest, AddRectanglesToBatch) {
    batchManager->beginBatch();
    
    // Add rectangles with same state (should be batched together)
    CBox box1(0, 0, 100, 100);
    CBox box2(100, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    batchManager->addRect(box1, color, 0, 2.0f);
    batchManager->addRect(box2, color, 0, 2.0f);
    
    auto metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 2);
    
    batchManager->endBatch();
}

TEST_F(BatchManagerTest, BatchesByShaderState) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    // Different rounding values should create different batches
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addRect(box, color, 10, 2.0f); // Different rounding
    batchManager->addRect(box, color, 0, 3.0f);  // Different power
    
    auto metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 3);
    
    // When we flush, we should have at least 3 draw calls
    batchManager->endBatch();
    metrics = batchManager->getMetrics();
    EXPECT_GE(metrics.drawCalls, 3);
}

TEST_F(BatchManagerTest, TextureBatching) {
    batchManager->beginBatch();
    
    CBox box1(0, 0, 100, 100);
    CBox box2(100, 0, 100, 100);
    uint32_t textureId = 123;
    
    // Same texture ID should batch together
    batchManager->addTexture(textureId, box1, 1.0f, 0, 2.0f);
    batchManager->addTexture(textureId, box2, 1.0f, 0, 2.0f);
    
    auto metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 2);
    
    batchManager->endBatch();
}

TEST_F(BatchManagerTest, MixedOperationTypes) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    uint32_t textureId = 123;
    
    // Mix different operation types
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addTexture(textureId, box, 1.0f, 0, 2.0f);
    batchManager->addBorder(box, color, 0, 2.0f, 2);
    batchManager->addShadow(box, 10, 2.0f, 20, color);
    
    auto metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 4);
    
    batchManager->endBatch();
}

TEST_F(BatchManagerTest, FlushOnStateChange) {
    batchManager->beginBatch();
    batchManager->setAutoFlush(true);
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    // Add operations that would require state changes
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addRect(box, color, 10, 2.0f); // Different state
    
    // Auto-flush should have occurred
    auto metrics = batchManager->getMetrics();
    EXPECT_GE(metrics.drawCalls, 1);
    
    batchManager->endBatch();
}

TEST_F(BatchManagerTest, MetricsTracking) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    uint32_t tex1 = 123;
    uint32_t tex2 = 456;
    
    // Operations that will create state changes
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addTexture(tex1, box, 1.0f, 0, 2.0f);
    batchManager->addTexture(tex2, box, 1.0f, 0, 2.0f); // Different texture
    
    batchManager->endBatch();
    
    auto metrics = batchManager->getMetrics();
    EXPECT_GT(metrics.drawCalls, 0);
    EXPECT_GT(metrics.stateChanges, 0);
    EXPECT_GT(metrics.textureBinds, 0);
}

TEST_F(BatchManagerTest, ClearBatch) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addRect(box, color, 0, 2.0f);
    
    auto metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 2);
    
    batchManager->clearBatch();
    metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.pendingOperations, 0);
    
    batchManager->endBatch();
}

TEST_F(BatchManagerTest, ResetMetrics) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->endBatch();
    
    auto metrics = batchManager->getMetrics();
    EXPECT_GT(metrics.drawCalls, 0);
    
    batchManager->resetMetrics();
    metrics = batchManager->getMetrics();
    EXPECT_EQ(metrics.drawCalls, 0);
    EXPECT_EQ(metrics.stateChanges, 0);
    EXPECT_EQ(metrics.textureBinds, 0);
}

TEST_F(BatchManagerTest, OptimizedPathActivation) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    // Add exactly 4 rectangles (should trigger optimized path)
    for (int i = 0; i < 4; i++) {
        batchManager->addRect(CBox(i * 100, 0, 100, 100), color, 0, 2.0f);
    }
    
    batchManager->endBatch();
    
    auto metrics = batchManager->getMetrics();
    // With optimized path, should be 1 draw call instead of 4
    EXPECT_EQ(metrics.drawCalls, 1);
    EXPECT_EQ(metrics.stateChanges, 1);
}

TEST_F(BatchManagerTest, SmallBatchFallback) {
    batchManager->beginBatch();
    
    CBox box(0, 0, 100, 100);
    CHyprColor color(1.0f, 0.0f, 0.0f, 1.0f);
    
    // Add only 2 rectangles (should use fallback path)
    batchManager->addRect(box, color, 0, 2.0f);
    batchManager->addRect(box, color, 0, 2.0f);
    
    batchManager->endBatch();
    
    auto metrics = batchManager->getMetrics();
    // With fallback path, should be 2 draw calls
    EXPECT_EQ(metrics.drawCalls, 2);
    EXPECT_EQ(metrics.stateChanges, 1);
}

TEST_F(BatchManagerTest, LargeBatchPerformance) {
    batchManager->beginBatch();
    
    CHyprColor color(1.0f, 0.5f, 0.0f, 1.0f);
    const int RECT_COUNT = 1000;
    
    // Add many rectangles
    for (int i = 0; i < RECT_COUNT; i++) {
        CBox box(i % 100 * 10, i / 100 * 10, 10, 10);
        batchManager->addRect(box, color, 0, 2.0f);
    }
    
    batchManager->endBatch();
    
    auto metrics = batchManager->getMetrics();
    // Should be significantly fewer draw calls than rectangles
    EXPECT_LT(metrics.drawCalls, RECT_COUNT / 10);
    EXPECT_EQ(metrics.stateChanges, 1);
}