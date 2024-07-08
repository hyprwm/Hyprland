#include "CursorManager.hpp"
#include "Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "PointerManager.hpp"
#include "../xwayland/XWayland.hpp"
#include <cstring>
#include "../helpers/CursorShapes.hpp"

extern "C" {
#include <X11/Xcursor/Xcursor.h>
}

static int cursorAnimTimer(void* data) {
    g_pCursorManager->tickAnimatedCursor();
    return 1;
}

static void hcLogger(enum eHyprcursorLogLevel level, char* message) {
    if (level == HC_LOG_TRACE)
        return;

    Debug::log(NONE, "[hc] {}", message);
}

CCursorManager::CCursorManager() {
    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(m_szTheme.empty() ? nullptr : m_szTheme.c_str(), hcLogger);

    if (!m_pHyprcursor->valid())
        Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to X.", m_szTheme);

    // find default size. First, HYPRCURSOR_SIZE, then XCURSOR_SIZE, then 24
    auto SIZE = getenv("HYPRCURSOR_SIZE");
    if (SIZE) {
        try {
            m_iSize = std::stoi(SIZE);
        } catch (...) { ; }
    }

    SIZE = getenv("XCURSOR_SIZE");
    if (SIZE && m_iSize == 0) {
        try {
            m_iSize = std::stoi(SIZE);
        } catch (...) { ; }
    }

    if (m_iSize == 0)
        m_iSize = 24;

    xcursor.loadTheme(getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "", m_iSize * std::ceil(m_fCursorScale));

    m_pAnimationTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, ::cursorAnimTimer, nullptr);

    updateTheme();

    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateTheme(); });
}

CCursorManager::~CCursorManager() {
    if (m_pAnimationTimer)
        wl_event_source_remove(m_pAnimationTimer);
}

void CCursorManager::dropBufferRef(CCursorManager::CCursorBuffer* ref) {
    std::erase_if(m_vCursorBuffers, [ref](const auto& buf) { return buf.get() == ref; });
}

CCursorManager::CCursorBuffer::CCursorBuffer(cairo_surface_t* surf, const Vector2D& size_, const Vector2D& hot_) : hotspot(hot_) {
    surface = surf;
    size    = size_;
    stride  = cairo_image_surface_get_stride(surf);
}

CCursorManager::CCursorBuffer::CCursorBuffer(uint8_t* pixelData_, const Vector2D& size_, const Vector2D& hot_) : hotspot(hot_) {
    pixelData = pixelData_;
    size      = size_;
    stride    = 4 * size_.x;
}

CCursorManager::CCursorBuffer::~CCursorBuffer() {
    ;
}

Aquamarine::eBufferCapability CCursorManager::CCursorBuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CCursorManager::CCursorBuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_SHM;
}

void CCursorManager::CCursorBuffer::update(const Hyprutils::Math::CRegion& damage) {
    ;
}

bool CCursorManager::CCursorBuffer::isSynchronous() {
    return true;
}

bool CCursorManager::CCursorBuffer::good() {
    return true;
}

Aquamarine::SSHMAttrs CCursorManager::CCursorBuffer::shm() {
    Aquamarine::SSHMAttrs attrs;
    attrs.success = true;
    attrs.format  = DRM_FORMAT_ARGB8888;
    attrs.size    = size;
    attrs.stride  = stride;
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CCursorManager::CCursorBuffer::beginDataPtr(uint32_t flags) {
    return {pixelData ? pixelData : cairo_image_surface_get_data(surface), DRM_FORMAT_ARGB8888, stride};
}

void CCursorManager::CCursorBuffer::endDataPtr() {
    ;
}

SP<Aquamarine::IBuffer> CCursorManager::getCursorBuffer() {
    return !m_vCursorBuffers.empty() ? m_vCursorBuffers.back() : nullptr;
}

void CCursorManager::setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot) {
    if (!surf || !surf->resource())
        g_pPointerManager->resetCursorImage();
    else
        g_pPointerManager->setCursorSurface(surf, hotspot);

    m_bOurBufferConnected = false;
}

void CCursorManager::setXCursor(const std::string& name) {
    float scale = std::ceil(m_fCursorScale);

    if (!xcursor.themeLoaded) {
        Debug::log(ERR, "XCursor failed to find theme in setXCursor");
        g_pPointerManager->resetCursorImage();
        return;
    }

    auto& icon = xcursor.defaultCursor;
    // try to get an icon we know if we have one
    if (xcursor.cursors.contains(name))
        icon = xcursor.cursors.at(name);

    m_vCursorBuffers.emplace_back(makeShared<CCursorBuffer>((uint8_t*)icon->pixels.data(), icon->size, icon->hotspot));

    g_pPointerManager->setCursorBuffer(getCursorBuffer(), icon->hotspot / scale, scale);
    if (m_vCursorBuffers.size() > 1)
        dropBufferRef(m_vCursorBuffers.at(0).get());

    m_bOurBufferConnected = true;
}

void CCursorManager::setCursorFromName(const std::string& name) {

    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");

    if (!m_pHyprcursor->valid() || !*PUSEHYPRCURSOR) {
        setXCursor(name);
        return;
    }

    m_sCurrentCursorShapeData = m_pHyprcursor->getShape(name.c_str(), m_sCurrentStyleInfo);

    if (m_sCurrentCursorShapeData.images.size() < 1) {
        // try with '_' first (old hc, etc)
        std::string newName = name;
        std::replace(newName.begin(), newName.end(), '-', '_');

        m_sCurrentCursorShapeData = m_pHyprcursor->getShape(newName.c_str(), m_sCurrentStyleInfo);
    }

    if (m_sCurrentCursorShapeData.images.size() < 1) {
        // fallback to a default if available
        constexpr const std::array<const char*, 3> fallbackShapes = {"default", "left_ptr", "left-ptr"};

        for (auto& s : fallbackShapes) {
            m_sCurrentCursorShapeData = m_pHyprcursor->getShape(s, m_sCurrentStyleInfo);

            if (m_sCurrentCursorShapeData.images.size() > 0)
                break;
        }

        if (m_sCurrentCursorShapeData.images.size() < 1) {
            Debug::log(ERR, "BUG THIS: No fallback found for a cursor in setCursorFromName");
            setXCursor(name);
            return;
        }
    }

    m_vCursorBuffers.emplace_back(makeShared<CCursorBuffer>(m_sCurrentCursorShapeData.images[0].surface,
                                                            Vector2D{m_sCurrentCursorShapeData.images[0].size, m_sCurrentCursorShapeData.images[0].size},
                                                            Vector2D{m_sCurrentCursorShapeData.images[0].hotspotX, m_sCurrentCursorShapeData.images[0].hotspotY}));

    g_pPointerManager->setCursorBuffer(getCursorBuffer(), Vector2D{m_sCurrentCursorShapeData.images[0].hotspotX, m_sCurrentCursorShapeData.images[0].hotspotY} / m_fCursorScale,
                                       m_fCursorScale);
    if (m_vCursorBuffers.size() > 1)
        dropBufferRef(m_vCursorBuffers.at(0).get());

    m_bOurBufferConnected = true;

    if (m_sCurrentCursorShapeData.images.size() > 1) {
        // animated
        wl_event_source_timer_update(m_pAnimationTimer, m_sCurrentCursorShapeData.images[0].delay);
        m_iCurrentAnimationFrame = 0;
    } else {
        // disarm
        wl_event_source_timer_update(m_pAnimationTimer, 0);
    }
}

void CCursorManager::tickAnimatedCursor() {
    if (m_sCurrentCursorShapeData.images.size() < 2 || !m_bOurBufferConnected)
        return;

    m_iCurrentAnimationFrame++;
    if ((size_t)m_iCurrentAnimationFrame >= m_sCurrentCursorShapeData.images.size())
        m_iCurrentAnimationFrame = 0;

    m_vCursorBuffers.emplace_back(makeShared<CCursorBuffer>(
        m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].surface,
        Vector2D{m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].size, m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].size},
        Vector2D{m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].hotspotX, m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].hotspotY}));

    g_pPointerManager->setCursorBuffer(
        getCursorBuffer(),
        Vector2D{m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].hotspotX, m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].hotspotY} / m_fCursorScale,
        m_fCursorScale);

    wl_event_source_timer_update(m_pAnimationTimer, m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].delay);
}

SCursorImageData CCursorManager::dataFor(const std::string& name) {

    if (!m_pHyprcursor->valid())
        return {};

    const auto IMAGES = m_pHyprcursor->getShape(name.c_str(), m_sCurrentStyleInfo);

    if (IMAGES.images.empty())
        return {};

    return IMAGES.images[0];
}

void CCursorManager::setXWaylandCursor() {
    const auto CURSOR = dataFor("left_ptr");
    if (CURSOR.surface) {
        g_pXWayland->setCursor(cairo_image_surface_get_data(CURSOR.surface), cairo_image_surface_get_stride(CURSOR.surface), {CURSOR.size, CURSOR.size},
                               {CURSOR.hotspotX, CURSOR.hotspotY});
    } else if (xcursor.themeLoaded)
        g_pXWayland->setCursor((uint8_t*)xcursor.defaultCursor->pixels.data(), xcursor.defaultCursor->size.x * 4, xcursor.defaultCursor->size, xcursor.defaultCursor->hotspot);
    else
        Debug::log(ERR, "CursorManager: no valid cursor for xwayland");
}

void CCursorManager::updateTheme() {
    float highestScale = 1.0;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->scale > highestScale)
            highestScale = m->scale;
    }

    if (m_sCurrentStyleInfo.size && m_pHyprcursor->valid())
        m_pHyprcursor->cursorSurfaceStyleDone(m_sCurrentStyleInfo);

    m_sCurrentStyleInfo.size = std::round(m_iSize * highestScale);
    m_fCursorScale           = highestScale;

    if (m_pHyprcursor->valid())
        m_pHyprcursor->loadThemeStyle(m_sCurrentStyleInfo);

    setCursorFromName("left_ptr");

    for (auto& m : g_pCompositor->m_vMonitors) {
        m->forceFullFrames = 5;
        g_pCompositor->scheduleFrameForMonitor(m.get());
    }
}

bool CCursorManager::changeTheme(const std::string& name, const int size) {
    auto options                 = Hyprcursor::SManagerOptions();
    options.logFn                = hcLogger;
    options.allowDefaultFallback = false;

    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(name.empty() ? "" : name.c_str(), options);
    if (m_pHyprcursor->valid()) {
        m_szTheme = name;
        m_iSize   = size;
        updateTheme();
        return true;
    }

    Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to X.", name);

    xcursor.loadTheme(name, size);

    m_szTheme = name;
    m_iSize   = size;
    updateTheme();
    return true;
}

// Taken from https://gitlab.freedesktop.org/xorg/lib/libxcursor/-/blob/master/src/library.c
// however modified to fit wayland cursor shape names better.
// _ -> -
// clang-format off
static std::array<const char*, 77> XCURSOR_STANDARD_NAMES = {
    "X_cursor",
    "default", // arrow
    "ns-resize", // based-arrow-down
    "ns-resize", // based-arrow-up
    "boat",
    "bogosity",
    "sw-resize", // bottom-left-corner
    "se-resize", // bottom-right-corner
    "s-resize", // bottom-side
    "bottom-tee",
    "box-spiral",
    "center-ptr",
    "circle",
    "clock",
    "coffee-mug",
    "cross",
    "cross-reverse",
    "crosshair",
    "diamond-cross",
    "dot",
    "dotbox",
    "double-arrow",
    "draft-large",
    "draft-small",
    "draped-box",
    "exchange",
    "move", // fleur
    "gobbler",
    "gumby",
    "pointer", // hand1
    "grabbing", // hand2
    "heart",
    "icon",
    "iron-cross",
    "default", // left-ptr
    "w-resize", // left-side
    "left-tee",
    "leftbutton",
    "ll-angle",
    "lr-angle",
    "man",
    "middlebutton",
    "mouse",
    "pencil",
    "pirate",
    "plus",
    "help", // question-arrow
    "right-ptr",
    "e-resize", // right-side
    "right-tee",
    "rightbutton",
    "rtl-logo",
    "sailboat",
    "ns-resize", // sb-down-arrow
    "ew-resize", // sb-h-double-arrow
    "ew-resize", // sb-left-arrow
    "ew-resize", // sb-right-arrow
    "n-resize", // sb-up-arrow
    "s-resize", // sb-v-double-arrow
    "shuttle",
    "sizing",
    "spider",
    "spraycan",
    "star",
    "target",
    "cell", // tcross
    "nw-resize", // top-left-arrow
    "nw-resize", // top-left-corner
    "ne-resize", // top-right-corner
    "n-resize", // top-side
    "top-tee",
    "trek",
    "ul-angle",
    "umbrella",
    "ur-angle",
    "wait", // watch
    "text", // xterm
};
// clang-format on

void CCursorManager::SXCursorManager::loadTheme(const std::string& name, int size) {
    if (lastLoadSize == size && themeName == name)
        return;

    lastLoadSize = size;
    themeLoaded  = false;
    themeName    = name.empty() ? "default" : name;

    auto img = XcursorShapeLoadImage(2, themeName.c_str(), size);

    if (!img) {
        Debug::log(ERR, "XCursor failed finding theme \"{}\". Trying size 24.", themeName);
        size = 24;
        img  = XcursorShapeLoadImage(2, themeName.c_str(), size);
        if (!img) {
            Debug::log(ERR, "XCursor failed finding theme \"{}\".", themeName);
            return;
        }
    }

    defaultCursor          = makeShared<SXCursor>();
    defaultCursor->size    = {(int)img->width, (int)img->height};
    defaultCursor->hotspot = {(int)img->xhot, (int)img->yhot};

    defaultCursor->pixels.resize(img->width * img->height);
    std::memcpy(defaultCursor->pixels.data(), img->pixels, img->width * img->height * sizeof(uint32_t));

    themeLoaded = true;

    // gather as many shapes as we can find.
    cursors.clear();

    for (auto& shape : CURSOR_SHAPE_NAMES) {
        int id = -1;
        for (size_t i = 0; i < XCURSOR_STANDARD_NAMES.size(); ++i) {
            if (XCURSOR_STANDARD_NAMES.at(i) == std::string{shape}) {
                id = i;
                break;
            }
        }

        if (id < 0) {
            Debug::log(LOG, "XCursor has no shape {}, skipping", shape);
            continue;
        }

        auto xImage = XcursorShapeLoadImage(id << 1 /* wtf xcursor? */, themeName.c_str(), size);

        if (!xImage) {
            Debug::log(LOG, "XCursor failed to find a shape with name {}, skipping", shape);
            continue;
        }

        auto xcursor     = makeShared<SXCursor>();
        xcursor->size    = {(int)xImage->width, (int)xImage->height};
        xcursor->hotspot = {(int)xImage->xhot, (int)xImage->yhot};

        xcursor->pixels.resize(xImage->width * xImage->height);
        std::memcpy(xcursor->pixels.data(), xImage->pixels, xImage->width * xImage->height * sizeof(uint32_t));

        cursors.emplace(std::string{shape}, xcursor);
    }
}
