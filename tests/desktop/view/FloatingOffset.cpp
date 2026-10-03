#include <desktop/view/window/FloatingOffset.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;
using Hyprutils::Math::Vector2D;

TEST(FloatingOffset, DefaultsToNoWindowDisplacement) {
    const CFloatingOffset offset;
    EXPECT_EQ(offset.value(), Vector2D{});
    EXPECT_EQ(offset.source(), eFloatingOffsetSource::WINDOW);
}

TEST(FloatingOffset, LastWriterReplacesValueAndSource) {
    CFloatingOffset offset;
    offset.set({7, 11});
    EXPECT_EQ(offset.value(), Vector2D(7, 11));
    EXPECT_EQ(offset.source(), eFloatingOffsetSource::WINDOW);

    offset.set({23, 37}, eFloatingOffsetSource::WORKSPACE);
    EXPECT_EQ(offset.value(), Vector2D(23, 37));
    EXPECT_EQ(offset.source(), eFloatingOffsetSource::WORKSPACE);

    offset.set({3, 5});
    EXPECT_EQ(offset.value(), Vector2D(3, 5));
    EXPECT_EQ(offset.source(), eFloatingOffsetSource::WINDOW);
}

TEST(FloatingOffset, ClearDropsWorkspaceProvenance) {
    CFloatingOffset offset;
    offset.set({23, 37}, eFloatingOffsetSource::WORKSPACE);
    offset.clear();
    EXPECT_EQ(offset.value(), Vector2D{});
    EXPECT_EQ(offset.source(), eFloatingOffsetSource::WINDOW);
}
