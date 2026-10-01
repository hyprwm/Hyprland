#include "CursorShape.hpp"
#include "CursorManager.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

using namespace Pointer::Cursor;

static bool validCursorImage(const SCursorImageData& image) {
    if (!image.surface || image.size <= 0 || cairo_surface_status(image.surface) != CAIRO_STATUS_SUCCESS || cairo_surface_get_type(image.surface) != CAIRO_SURFACE_TYPE_IMAGE)
        return false;

    const auto FORMAT = cairo_image_surface_get_format(image.surface);
    if (FORMAT != CAIRO_FORMAT_ARGB32 && FORMAT != CAIRO_FORMAT_RGB24)
        return false;

    const auto WIDTH  = cairo_image_surface_get_width(image.surface);
    const auto HEIGHT = cairo_image_surface_get_height(image.surface);
    const auto STRIDE = cairo_image_surface_get_stride(image.surface);
    if (WIDTH <= 0 || HEIGHT <= 0 || STRIDE <= 0)
        return false;

    if (sc<uint64_t>(WIDTH) * 4 > sc<uint64_t>(STRIDE) || sc<uint64_t>(STRIDE) * sc<uint64_t>(HEIGHT) > sc<uint64_t>(std::numeric_limits<ptrdiff_t>::max()))
        return false;

    cairo_surface_flush(image.surface);
    return cairo_surface_status(image.surface) == CAIRO_STATUS_SUCCESS && cairo_image_surface_get_data(image.surface);
}

static SP<CCursorBuffer> copyCursorImage(const SCursorImageData& image, const Vector2D& size, const Vector2D& hotspot) {
    if (cairo_image_surface_get_format(image.surface) == CAIRO_FORMAT_ARGB32)
        return makeShared<CCursorBuffer>(image.surface, size, hotspot);

    // RGB24's unused byte is not alpha. Convert it to opaque ARGB32 before copying.
    const std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface{
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cairo_image_surface_get_width(image.surface), cairo_image_surface_get_height(image.surface)),
        cairo_surface_destroy,
    };
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return nullptr;

    const std::unique_ptr<cairo_t, decltype(&cairo_destroy)> context{
        cairo_create(surface.get()),
        cairo_destroy,
    };
    cairo_set_operator(context.get(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context.get(), image.surface, 0, 0);
    cairo_paint(context.get());
    if (cairo_status(context.get()) != CAIRO_STATUS_SUCCESS)
        return nullptr;

    cairo_surface_flush(surface.get());
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS || !cairo_image_surface_get_data(surface.get()))
        return nullptr;

    return makeShared<CCursorBuffer>(surface.get(), size, hotspot);
}

CCursorShape::CCursorShape(const Hyprcursor::SCursorShapeData& data) {
    if (data.images.empty())
        return;

    // Commit only complete shapes: a bad frame must not leave a partial animation.
    std::vector<SFrame>   frames;
    std::vector<uint64_t> frameEnds;
    uint64_t              cycleDuration = 0;
    const bool            ANIMATED      = data.images.size() > 1;
    frames.reserve(data.images.size());
    frameEnds.reserve(data.images.size());

    for (const auto& image : data.images) {
        if (!validCursorImage(image))
            return;

        const auto DELAY = ANIMATED ? sc<uint64_t>(std::max(image.delay, 1)) : 0;
        if (DELAY > sc<uint64_t>(std::chrono::milliseconds::max().count()) - cycleDuration)
            return;

        const Vector2D SIZE{cairo_image_surface_get_width(image.surface), cairo_image_surface_get_height(image.surface)};
        const Vector2D HOTSPOT{image.hotspotX, image.hotspotY};
        auto           buffer = copyCursorImage(image, SIZE, HOTSPOT);
        if (!buffer)
            return;

        frames.push_back(SFrame{
            .buffer  = std::move(buffer),
            .hotspot = HOTSPOT,
            .delay   = std::chrono::milliseconds{sc<int64_t>(DELAY)},
        });
        cycleDuration += DELAY;
        frameEnds.push_back(cycleDuration);
    }

    m_frames        = std::move(frames);
    m_frameEnds     = std::move(frameEnds);
    m_cycleDuration = cycleDuration;
}

bool CCursorShape::valid() const {
    return !m_frames.empty();
}

const CCursorShape::SFrame& CCursorShape::frame(size_t index) const {
    return m_frames.at(index);
}

uint64_t CCursorShape::cyclePosition(std::chrono::milliseconds elapsed) const {
    if (elapsed.count() <= 0 || m_cycleDuration == 0)
        return 0;

    return sc<uint64_t>(elapsed.count()) % m_cycleDuration;
}

size_t CCursorShape::frameAt(std::chrono::milliseconds elapsed) const {
    if (m_frames.size() < 2)
        return 0;

    return std::upper_bound(m_frameEnds.begin(), m_frameEnds.end(), cyclePosition(elapsed)) - m_frameEnds.begin();
}

std::optional<std::chrono::milliseconds> CCursorShape::timeUntilNextFrame(std::chrono::milliseconds elapsed) const {
    if (m_frames.size() < 2)
        return std::nullopt;

    const auto POSITION = cyclePosition(elapsed);
    const auto END      = std::upper_bound(m_frameEnds.begin(), m_frameEnds.end(), POSITION);
    return std::chrono::milliseconds{sc<int64_t>(*END - POSITION)};
}
