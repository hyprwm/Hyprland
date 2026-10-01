#include <gtest/gtest.h>

#include <pointer/cursor/CursorShape.hpp>
#include <pointer/cursor/CursorManager.hpp>

#include <array>
#include <cstring>
#include <drm_fourcc.h>
#include <memory>
#include <stdexcept>

using namespace Pointer::Cursor;
using namespace std::chrono_literals;

using CairoSurface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;

static SCursorImageData makeImage(const CairoSurface& surface) {
    return {
        .surface  = surface.get(),
        .size     = cairo_image_surface_get_width(surface.get()),
        .delay    = 10,
        .hotspotX = 1,
        .hotspotY = 1,
    };
}

TEST(CursorShapeFormats, RGB24BecomesOpaqueOwnedARGB32) {
    // Native-endian RGB24 words with differing unused bytes and padded rows.
    std::array<uint32_t, 6> pixels = {
        0x00123456, 0x7FABCDEF, 0xDEADBEEF, 0xFF010203, 0x00000000, 0xDEADBEEF,
    };
    CairoSurface surface{
        cairo_image_surface_create_for_data(rc<unsigned char*>(pixels.data()), CAIRO_FORMAT_RGB24, 2, 2, 3 * sizeof(uint32_t)),
        cairo_surface_destroy,
    };
    ASSERT_EQ(cairo_surface_status(surface.get()), CAIRO_STATUS_SUCCESS);

    Hyprcursor::SCursorShapeData data{{makeImage(surface)}};
    const CCursorShape           shape{data};
    ASSERT_TRUE(shape.valid());
    ASSERT_TRUE(shape.frame(0).buffer);
    EXPECT_EQ(shape.frame(0).buffer->size, Vector2D(2, 2));
    EXPECT_EQ(shape.frame(0).hotspot, Vector2D(1, 1));

    const auto checkPixels = [&shape] {
        const auto& BUFFER                 = shape.frame(0).buffer;
        const auto [bytes, format, stride] = BUFFER->beginDataPtr(0);
        ASSERT_NE(bytes, nullptr);
        EXPECT_EQ(format, DRM_FORMAT_ARGB8888);
        ASSERT_EQ(stride, 2 * sizeof(uint32_t));
        const std::array<uint32_t, 4> EXPECTED = {
            0xFF123456,
            0xFFABCDEF,
            0xFF010203,
            0xFF000000,
        };
        for (size_t i = 0; i < EXPECTED.size(); ++i) {
            uint32_t pixel = 0;
            std::memcpy(&pixel, bytes + (i / 2) * stride + (i % 2) * sizeof(pixel), sizeof(pixel));
            EXPECT_EQ(pixel, EXPECTED[i]) << "pixel " << i;
        }
        BUFFER->endDataPtr();
    };
    checkPixels();

    cairo_surface_flush(surface.get());
    pixels.fill(0);
    cairo_surface_mark_dirty(surface.get());
    surface.reset();
    data.images.clear();
    checkPixels();
}

TEST(CursorShapeFormats, UnsupportedA8RejectsEntireShape) {
    const CairoSurface valid{cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2), cairo_surface_destroy};
    const CairoSurface unsupported{cairo_image_surface_create(CAIRO_FORMAT_A8, 2, 2), cairo_surface_destroy};
    ASSERT_EQ(cairo_surface_status(valid.get()), CAIRO_STATUS_SUCCESS);
    ASSERT_EQ(cairo_surface_status(unsupported.get()), CAIRO_STATUS_SUCCESS);

    const CCursorShape shape{Hyprcursor::SCursorShapeData{{makeImage(valid), makeImage(unsupported)}}};
    EXPECT_FALSE(shape.valid());
    EXPECT_EQ(shape.frameAt(10ms), 0U);
    EXPECT_EQ(shape.timeUntilNextFrame(0ms), std::nullopt);
    EXPECT_THROW(shape.frame(0), std::out_of_range);
}
