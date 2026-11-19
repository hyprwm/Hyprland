#include <gtest/gtest.h>

#include <pointer/PointerManager.hpp>

using Pointer::CPointerManager;

static const std::vector<Vector2D> I915_HINTS = {{64, 64}, {128, 128}, {256, 256}};

TEST(CursorBufferSize, PicksSmallestSupportedSizeThatFits) {
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {24, 24}), Vector2D(64, 64));
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {64, 64}), Vector2D(64, 64));
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {65, 30}), Vector2D(128, 128));
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {200, 200}), Vector2D(256, 256));
}

TEST(CursorBufferSize, IgnoresHintOrder) {
    const std::vector<Vector2D> hints = {{256, 256}, {64, 64}, {128, 128}};
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, hints, {48, 48}), Vector2D(64, 64));
}

TEST(CursorBufferSize, FallsBackToMaxSize) {
    // no hints exposed by the driver
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, {}, {24, 24}), Vector2D(256, 256));
    // no hint fits, but the max size does
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, {{64, 64}}, {100, 100}), Vector2D(256, 256));
    // hints larger than the max are never used
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({128, 128}, {{512, 512}}, {100, 100}), Vector2D(128, 128));
}

TEST(CursorBufferSize, NonSquareHints) {
    const std::vector<Vector2D> hints = {{64, 32}, {64, 64}, {256, 256}};
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, hints, {48, 24}), Vector2D(64, 32));
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, hints, {24, 48}), Vector2D(64, 64));
}

TEST(CursorBufferSize, UnlimitedPlaneUsesCursorSize) {
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({-1, -1}, I915_HINTS, {30, 40}), Vector2D(30, 40));
}

TEST(CursorBufferSize, RejectsUnusableInput) {
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({}, I915_HINTS, {24, 24}), std::nullopt);
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {0, 24}), std::nullopt);
    EXPECT_EQ(CPointerManager::hwCursorBufferSize({256, 256}, I915_HINTS, {257, 24}), std::nullopt);
}
