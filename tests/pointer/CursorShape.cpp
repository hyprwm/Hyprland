#include <gtest/gtest.h>

#include <pointer/cursor/CursorShape.hpp>
#include <pointer/cursor/CursorManager.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace Pointer::Cursor;
using namespace std::chrono_literals;

using CairoSurface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;

static CairoSurface makeSurface(int width = 2, int height = 2) {
    return CairoSurface{cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height), cairo_surface_destroy};
}

static SCursorImageData makeImage(const CairoSurface& surface, int delay, int hotspotX = 0, int hotspotY = 0) {
    return {
        .surface  = surface.get(),
        .size     = cairo_image_surface_get_width(surface.get()),
        .delay    = delay,
        .hotspotX = hotspotX,
        .hotspotY = hotspotY,
    };
}

TEST(CursorShape, EmptyShapeHasNoTimeline) {
    const CCursorShape shape{Hyprcursor::SCursorShapeData{}};

    EXPECT_FALSE(shape.valid());
    EXPECT_EQ(shape.frameAt(0ms), 0U);
    EXPECT_EQ(shape.frameAt(std::chrono::milliseconds::max()), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(0ms), std::nullopt);
    EXPECT_THROW(shape.frame(0), std::out_of_range);
}

TEST(CursorShape, SingleFrameIsStaticRegardlessOfDelay) {
    const auto surface = makeSurface();

    for (const int delay : {0, -1, std::numeric_limits<int>::min(), 75, std::numeric_limits<int>::max()}) {
        SCOPED_TRACE(delay);
        const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(surface, delay)}}};

        ASSERT_TRUE(shape.valid());
        EXPECT_EQ(shape.frame(0).delay, 0ms);
        EXPECT_EQ(shape.frameAt(0ms), 0U);
        EXPECT_EQ(shape.frameAt(std::chrono::milliseconds::max()), 0U);
        EXPECT_EQ(shape.timeUntilNextFrame(0ms), std::nullopt);
        EXPECT_EQ(shape.timeUntilNextFrame(std::chrono::milliseconds::max()), std::nullopt);
    }
}

TEST(CursorShape, UnequalFrameDelaysSelectExactBoundariesAndSkipCycles) {
    const auto         surface = makeSurface();
    const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(surface, 10), makeImage(surface, 20), makeImage(surface, 30)}}};
    ASSERT_TRUE(shape.valid());

    struct SSample {
        std::chrono::milliseconds elapsed;
        size_t                    frame = 0;
        std::chrono::milliseconds remaining;
    };
    const std::array samples = {
        SSample{0ms, 0, 10ms}, SSample{9ms, 0, 1ms},   SSample{10ms, 1, 20ms}, SSample{29ms, 1, 1ms},   SSample{30ms, 2, 30ms},
        SSample{59ms, 2, 1ms}, SSample{60ms, 0, 10ms}, SSample{70ms, 1, 20ms}, SSample{150ms, 2, 30ms}, SSample{60029ms, 1, 1ms},
    };
    for (const auto& sample : samples) {
        SCOPED_TRACE(sample.elapsed.count());
        EXPECT_EQ(shape.frameAt(sample.elapsed), sample.frame);
        EXPECT_EQ(shape.timeUntilNextFrame(sample.elapsed), sample.remaining);
    }
    EXPECT_EQ(shape.frame(0).delay, 10ms);
    EXPECT_EQ(shape.frame(1).delay, 20ms);
    EXPECT_EQ(shape.frame(2).delay, 30ms);
    EXPECT_THROW(shape.frame(3), std::out_of_range);
}

TEST(CursorShape, ShapesWithDifferentFrameCountsHaveIndependentTimelines) {
    const auto         surface = makeSurface();
    const CCursorShape first{Hyprcursor::SCursorShapeData{{makeImage(surface, 4), makeImage(surface, 6)}}};
    const CCursorShape second{Hyprcursor::SCursorShapeData{{makeImage(surface, 3), makeImage(surface, 5), makeImage(surface, 7), makeImage(surface, 9)}}};
    ASSERT_TRUE(first.valid());
    ASSERT_TRUE(second.valid());

    EXPECT_EQ(first.frameAt(15ms), 1U);
    EXPECT_EQ(first.timeUntilNextFrame(15ms), 5ms);
    EXPECT_EQ(second.frameAt(15ms), 3U);
    EXPECT_EQ(second.timeUntilNextFrame(15ms), 9ms);
    EXPECT_EQ(first.frameAt(24ms), 1U);
    EXPECT_EQ(first.timeUntilNextFrame(24ms), 6ms);
    EXPECT_EQ(second.frameAt(24ms), 0U);
    EXPECT_EQ(second.timeUntilNextFrame(24ms), 3ms);
}

TEST(CursorShape, NonpositiveAnimatedDelaysBecomeOneMillisecond) {
    const auto         surface = makeSurface();
    const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(surface, 0), makeImage(surface, std::numeric_limits<int>::min()), makeImage(surface, 3)}}};
    ASSERT_TRUE(shape.valid());

    EXPECT_EQ(shape.frame(0).delay, 1ms);
    EXPECT_EQ(shape.frame(1).delay, 1ms);
    EXPECT_EQ(shape.frame(2).delay, 3ms);
    EXPECT_EQ(shape.frameAt(0ms), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(0ms), 1ms);
    EXPECT_EQ(shape.frameAt(1ms), 1U);
    EXPECT_EQ(shape.timeUntilNextFrame(1ms), 1ms);
    EXPECT_EQ(shape.frameAt(2ms), 2U);
    EXPECT_EQ(shape.timeUntilNextFrame(2ms), 3ms);
    EXPECT_EQ(shape.frameAt(5ms), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(5ms), 1ms);
    EXPECT_EQ(shape.frameAt(std::chrono::milliseconds::min()), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(std::chrono::milliseconds::min()), 1ms);
}

TEST(CursorShape, LargeDelaysAndElapsedTimesDoNotOverflow) {
    const auto         surface = makeSurface();
    const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(surface, std::numeric_limits<int>::max()), makeImage(surface, 1)}}};
    ASSERT_TRUE(shape.valid());

    EXPECT_EQ(shape.frameAt(2147483646ms), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(2147483646ms), 1ms);
    EXPECT_EQ(shape.frameAt(2147483647ms), 1U);
    EXPECT_EQ(shape.timeUntilNextFrame(2147483647ms), 1ms);
    EXPECT_EQ(shape.frameAt(2147483648ms), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(2147483648ms), 2147483647ms);
    EXPECT_EQ(shape.frameAt(9223372036854775807ms), 1U);
    EXPECT_EQ(shape.timeUntilNextFrame(9223372036854775807ms), 1ms);
}

TEST(CursorShape, InvalidFrameRejectsTheWholeShape) {
    const auto surface = makeSurface();
    const auto empty   = makeSurface(0, 0);
    const auto error   = makeSurface(-1, -1);

    for (const auto& invalid : {
             SCursorImageData{.surface = nullptr, .size = 2, .delay = 10},
             SCursorImageData{.surface = surface.get(), .size = 0, .delay = 10},
             SCursorImageData{.surface = surface.get(), .size = -1, .delay = 10},
             SCursorImageData{.surface = empty.get(), .size = 2, .delay = 10},
             SCursorImageData{.surface = error.get(), .size = 2, .delay = 10},
         }) {
        const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(surface, 10), invalid}}};

        EXPECT_FALSE(shape.valid());
        EXPECT_EQ(shape.frameAt(10ms), 0U);
        EXPECT_EQ(shape.timeUntilNextFrame(0ms), std::nullopt);
        EXPECT_THROW(shape.frame(0), std::out_of_range);
    }
}

TEST(CursorShape, BuffersKeepActualDimensionsPixelHotspotsAndCopiedPixelsAfterSurfaceDestruction) {
    auto firstSurface  = makeSurface(3, 3);
    auto secondSurface = makeSurface(5, 5);
    ASSERT_EQ(cairo_surface_status(firstSurface.get()), CAIRO_STATUS_SUCCESS);
    ASSERT_EQ(cairo_surface_status(secondSurface.get()), CAIRO_STATUS_SUCCESS);

    const auto fill = [](const CairoSurface& surface, uint8_t value) {
        cairo_surface_flush(surface.get());
        const auto BYTES = sc<size_t>(cairo_image_surface_get_stride(surface.get())) * cairo_image_surface_get_height(surface.get());
        std::fill_n(cairo_image_surface_get_data(surface.get()), BYTES, value);
        cairo_surface_mark_dirty(surface.get());
    };
    fill(firstSurface, 0x35);
    fill(secondSurface, 0xA7);

    Hyprcursor::SCursorShapeData data{{makeImage(firstSurface, 10, 2, 1), makeImage(secondSurface, 20, 4, 3)}};
    const CCursorShape           shape{data};
    ASSERT_TRUE(shape.valid());
    ASSERT_TRUE(shape.frame(0).buffer);
    ASSERT_TRUE(shape.frame(1).buffer);
    EXPECT_EQ(shape.frame(0).buffer->size, Vector2D(3, 3));
    EXPECT_EQ(shape.frame(1).buffer->size, Vector2D(5, 5));
    EXPECT_EQ(shape.frame(0).hotspot, Vector2D(2, 1));
    EXPECT_EQ(shape.frame(1).hotspot, Vector2D(4, 3));

    // Mutating the borrowed pixels before destroying them also detects retained surface references.
    fill(firstSurface, 0);
    fill(secondSurface, 0);
    firstSurface.reset();
    secondSurface.reset();
    data.images.clear();

    const std::array<uint8_t, 2> expected = {0x35, 0xA7};
    for (size_t i = 0; i < expected.size(); ++i) {
        const auto& BUFFER                  = shape.frame(i).buffer;
        const auto [pixels, format, stride] = BUFFER->beginDataPtr(0);
        const auto BYTES                    = stride * sc<size_t>(BUFFER->size.y);
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(stride, sc<size_t>(BUFFER->size.x) * 4);
        EXPECT_TRUE(std::all_of(pixels, pixels + BYTES, [value = expected[i]](uint8_t byte) { return byte == value; }));
        BUFFER->endDataPtr();
    }

    EXPECT_EQ(shape.frame(shape.frameAt(30ms)).buffer.get(), shape.frame(0).buffer.get());
    EXPECT_EQ(shape.frame(shape.frameAt(40ms)).buffer.get(), shape.frame(1).buffer.get());
}
