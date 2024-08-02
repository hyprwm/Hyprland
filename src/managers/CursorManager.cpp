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

    xcursor.loadTheme(getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "default", m_iSize * std::ceil(m_fCursorScale));

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
        if (!xcursor.defaultCursor) {
            g_pPointerManager->resetCursorImage();
            return;
        }
    }

    auto& icon = xcursor.defaultCursor;
    // try to get an icon we know if we have one
    for (auto const& c : xcursor.cursors) {
        if (c.first != name)
            continue;

        icon = c.second;
    }

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
        g_pCompositor->scheduleFrameForMonitor(m.get(), Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_SHAPE);
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
// _ -> -
// clang-format off
static std::array<const char*, 77> XCURSOR_STANDARD_NAMES = {
    "X_cursor",
    "arrow",
    "based_arrow_down",
    "based_arrow_up",
    "boat",
    "bogosity",
    "bottom_left_corner",
    "bottom_right_corner",
    "bottom_side",
    "bottom_tee",
    "box_spiral",
    "center_ptr",
    "circle",
    "clock",
    "coffee_mug",
    "cross",
    "cross_reverse",
    "crosshair",
    "diamond_cross",
    "dot",
    "dotbox",
    "double_arrow",
    "draft_large",
    "draft_small",
    "draped_box",
    "exchange",
    "fleur",
    "gobbler",
    "gumby",
    "hand1",
    "hand2",
    "heart",
    "icon",
    "iron_cross",
    "left_ptr",
    "left_side",
    "left_tee",
    "leftbutton",
    "ll_angle",
    "lr_angle",
    "man",
    "middlebutton",
    "mouse",
    "pencil",
    "pirate",
    "plus",
    "question_arrow",
    "right_ptr",
    "right_side",
    "right_tee",
    "rightbutton",
    "rtl_logo",
    "sailboat",
    "sb_down_arrow",
    "sb_h_double_arrow",
    "sb_left_arrow",
    "sb_right_arrow",
    "sb_up_arrow",
    "sb_v_double_arrow",
    "shuttle",
    "sizing",
    "spider",
    "spraycan",
    "star",
    "target",
    "tcross",
    "top_left_arrow",
    "top_left_corner",
    "top_right_corner",
    "top_side",
    "top_tee",
    "trek",
    "ul_angle",
    "umbrella",
    "ur_angle",
    "watch",
    "xterm",
};
// clang-format on

void CCursorManager::SXCursorManager::loadTheme(const std::string& name, int size) {
    if (lastLoadSize == size && themeName == name)
        return;

    lastLoadSize = size;
    themeLoaded  = false;
    themeName    = name.empty() ? "default" : name;

    std::vector<std::pair<std::string, SP<SXCursor>>> newCursors;

    // load the default xcursor shapes that exist in the theme
    for (size_t i = 0; i < XCURSOR_STANDARD_NAMES.size(); ++i) {
        auto shape  = XCURSOR_STANDARD_NAMES.at(i);
        auto xImage = XcursorShapeLoadImage(i << 1 /* wtf xcursor? */, themeName.c_str(), size);

        if (!xImage) {
            Debug::log(LOG, "XCursor failed to find a shape with name {}, trying size 24.", shape);
            xImage = XcursorShapeLoadImage(i << 1 /* wtf xcursor? */, themeName.c_str(), 24);

            if (!xImage) {
                Debug::log(LOG, "XCursor failed to find a shape with name {}, skipping", shape);
                continue;
            }
        }

        auto xcursor     = makeShared<SXCursor>();
        xcursor->size    = {(int)xImage->width, (int)xImage->height};
        xcursor->hotspot = {(int)xImage->xhot, (int)xImage->yhot};

        xcursor->pixels.resize(xImage->width * xImage->height);
        std::memcpy(xcursor->pixels.data(), xImage->pixels, xImage->width * xImage->height * sizeof(uint32_t));

        newCursors.emplace_back(std::string{shape}, xcursor);

        XcursorImageDestroy(xImage);
    }

    if (newCursors.empty()) {
        Debug::log(ERR, "XCursor failed finding any shapes in theme \"{}\".", themeName);
        if (!defaultCursor) {
            defaultCursor = createDefaultCursor();
        }
        return;
    }

    cursors.clear();
    cursors = std::move(newCursors);
    defaultCursor.reset();
    themeLoaded = true;

    for (auto const& shape : CURSOR_SHAPE_NAMES) {
        auto legacyName = getLegacyShapeName(shape);
        if (legacyName.empty())
            continue;

        auto it = std::find_if(cursors.begin(), cursors.end(), [&legacyName](auto const& c) { return c.first == legacyName; });

        if (it == cursors.end()) {
            Debug::log(LOG, "XCursor failed to find a legacy shape with name {}, skipping", legacyName);
            continue;
        }

        cursors.emplace_back(shape, it->second);
    }

    // set default cursor
    for (auto const& c : cursors) {
        if (c.first == "left_ptr" || c.first == "arrow") {
            defaultCursor = c.second;
            break;
        }
    }

    // just assign the first one then.
    if (!defaultCursor)
        defaultCursor = cursors.at(0).second;
}

std::string CCursorManager::SXCursorManager::getLegacyShapeName(std::string const& shape) {
    if (shape == "invalid")
        return std::string();
    else if (shape == "default")
        return "left_ptr";
    else if (shape == "context-menu")
        return "left_ptr";
    else if (shape == "help")
        return "left_ptr";
    else if (shape == "pointer")
        return "hand2";
    else if (shape == "progress")
        return "watch";
    else if (shape == "wait")
        return "watch";
    else if (shape == "cell")
        return "plus";
    else if (shape == "crosshair")
        return "cross";
    else if (shape == "text")
        return "xterm";
    else if (shape == "vertical-text")
        return "xterm";
    else if (shape == "alias")
        return "dnd-link";
    else if (shape == "copy")
        return "dnd-copy";
    else if (shape == "move")
        return "dnd-move";
    else if (shape == "no-drop")
        return "dnd-none";
    else if (shape == "not-allowed")
        return "crossed_circle";
    else if (shape == "grab")
        return "hand1";
    else if (shape == "grabbing")
        return "hand1";
    else if (shape == "e-resize")
        return "right_side";
    else if (shape == "n-resize")
        return "top_side";
    else if (shape == "ne-resize")
        return "top_right_corner";
    else if (shape == "nw-resize")
        return "top_left_corner";
    else if (shape == "s-resize")
        return "bottom_side";
    else if (shape == "se-resize")
        return "bottom_right_corner";
    else if (shape == "sw-resize")
        return "bottom_left_corner";
    else if (shape == "w-resize")
        return "left_side";
    else if (shape == "ew-resize")
        return "sb_h_double_arrow";
    else if (shape == "ns-resize")
        return "sb_v_double_arrow";
    else if (shape == "nesw-resize")
        return "fd_double_arrow";
    else if (shape == "nwse-resize")
        return "bd_double_arrow";
    else if (shape == "col-resize")
        return "sb_h_double_arrow";
    else if (shape == "row-resize")
        return "sb_v_double_arrow";
    else if (shape == "all-scroll")
        return "fleur";
    else if (shape == "zoom-in")
        return "left_ptr";
    else if (shape == "zoom-out")
        return "left_ptr";

    return std::string();
}

SP<CCursorManager::SXCursor> CCursorManager::SXCursorManager::createDefaultCursor() {
    int                   cursorWidth    = 32;
    int                   cursorHeight   = 32;
    int                   cursorHotspotX = 3;
    int                   cursorHotspotY = 2;

    static const uint32_t pixels[] = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1b001816, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x8e008173, 0x5f00564d, 0x16001412, 0x09000807, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2b002624, 0x05000404, 0x00000000, 0x35002f2b, 0xd400bead,
        0xc300b09e, 0x90008275, 0x44003e37, 0x04000403, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x67005a56,
        0x6f00615c, 0x00000000, 0x00000000, 0x8b007c72, 0xf200d7c6, 0xfa00e0cc, 0xe800d0bd, 0xa0009181, 0x44003e37, 0x1a001815, 0x06000505, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x8d007976, 0xd600b8b3, 0x2500201f, 0x00000000, 0x17001413, 0xbd00a79c, 0xf600dacb, 0xff00e3d1, 0xfc00e1ce, 0xe800d0bc, 0xbf00ac9b,
        0x95008778, 0x51004a41, 0x0f000e0c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x92007b7b, 0xf500d0cf, 0x9e008685, 0x00000000, 0x00000000, 0x23001f1d, 0x64005853,
        0x9b008980, 0xd900bfb3, 0xfb00dfce, 0xff00e4d0, 0xfb00e1cd, 0xec00d5c0, 0xa7009788, 0x47004139, 0x1e001b18, 0x05000504, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xa200878a, 0xff00d6d9, 0xd600b4b5,
        0x0e000c0c, 0x00000000, 0x00000000, 0x02000202, 0x0c000b0a, 0x30002a28, 0x8e007d75, 0xd600bdb0, 0xef00d4c4, 0xfb00e0ce, 0xff00e4d0, 0xe600cfbb, 0xb800a695, 0x5f00564d,
        0x06000505, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x02000202, 0xc600a3aa, 0xff00d3da, 0xea00c3c8, 0x08000707, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000101, 0x2a002523, 0x61005550, 0x9500837b,
        0xd800bfb1, 0xfd00e1cf, 0xff00e5d0, 0xf500dcc7, 0x7c007065, 0x2a002622, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000505, 0xd600aeb9, 0xff00d0dc, 0xcc00a7af, 0x04000303, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x01000101, 0x01000101, 0x2c002724, 0xa1008e85, 0xe600ccbd, 0xf800ddcb, 0xef00d6c3, 0xc300af9f, 0x2c002824, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x09000708, 0xd800adbc, 0xff00cdde, 0xb90095a0, 0x02000202, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x10000e0d, 0x4b00423e, 0xa4009088, 0xfd00dfd0, 0xff00e3d1,
        0xae009c8f, 0x42003b36, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x14001012, 0xf400c0d6,
        0xff00cadf, 0xb2008e9c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x02000202, 0x1200100f, 0xa2008e86, 0xec00cfc3, 0xfc00ded0, 0xc300ada0, 0x15001311, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x2e002429, 0xfd00c4e0, 0xff00c7e2, 0x8f00707e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1e001a19, 0x75006662, 0xfb00dbd1, 0xf700d9cc, 0x9600847c, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e002f37, 0xfc00c1e1, 0xff00c5e3, 0x60004b55, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x15001212, 0xa6008f8b, 0xff00ddd5,
        0xf800d8ce, 0x36002f2d, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x51003d49, 0xfe00bfe5, 0xfe00c1e4, 0x4c003a44,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x01000101, 0x1d001918, 0xf400d1cd, 0xfe00dad5, 0xb3009a96, 0x03000303, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x66004b5d, 0xff00bee7, 0xfd00bee5, 0x4500343f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000202, 0x82006e6e, 0xff00d8d7, 0xd800b9b6, 0x33002c2b, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x70005267, 0xff00bbe9, 0xfa00b8e3, 0x29001e25, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3f003536, 0xea00c3c7, 0xf800d1d3,
        0x4a003e3f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5f004458, 0xff00b8eb, 0xf400b1e0, 0x29001e26, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x1b001617, 0xe100bac0, 0xff00d4da, 0x82006c6f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5a004054, 0xfe00b4eb,
        0xfb00b3e8, 0x3b002a36, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0a000809, 0xc900a4ad, 0xff00d1dc, 0x88007075, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x44002f3f, 0xf300aae3, 0xfc00b1ea, 0x48003343, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x05000404, 0xdf00b3c2, 0xff00cedd, 0x8f00747c, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x25001a23, 0xf200a6e4, 0xff00b1ef, 0x84005c7c, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x09000708,
        0xe400b5c8, 0xff00cbdf, 0x78006068, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x12000c11, 0xc00082b6, 0xff00aef1, 0xaa0075a0,
        0x11000c10, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x1e00171b, 0xf700c1db, 0xff00c8e1, 0x4b003b42, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x05000305, 0x8d005f86, 0xfc00aaef, 0xed00a0e1, 0x26001a24, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6f005463, 0xff00c4e3, 0xf500beda, 0x0c00090b, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e00293b, 0xed009de2, 0xff00aaf3, 0x8b005d84, 0x04000304, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x07000506, 0xeb00b1d4, 0xff00c1e6, 0xba008ea7,
        0x02000202, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xb20075ab, 0xff00a7f4, 0xf300a1e9, 0x35002333,
        0x06000406, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x8900647d, 0xff00bde8, 0xfb00bce2, 0x26001d22, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x2b001c29, 0xf1009ce8, 0xfe00a5f4, 0xc60082be, 0x3b002738, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x01000101, 0x54003c4d, 0xfd00b8e8, 0xff00baea, 0x96006f89, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x78004d74, 0xe20091da, 0xff00a5f6, 0xb60077af, 0x42002b3f, 0x0c00080c, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x9400688a, 0xff00b5ed, 0xff00b6ec, 0xcc0093bc, 0x02000102, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000304, 0x8100527d, 0xeb0096e4, 0xf900a0f1, 0xdc008dd4,
        0x9b006595, 0x32002130, 0x0a00070a, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2100161f, 0x5300394e, 0xe2009cd4, 0xff00b1ef, 0xff00b2ee, 0xc8008cba, 0x17001015,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x19001018, 0x5e003b5b, 0xcb0081c5, 0xff00a2f8, 0xfc00a1f4, 0xde0090d6, 0xa9006da3, 0x8300567e, 0x6f00496a, 0x76004e71, 0xbb007db2, 0xeb009edf, 0xfb00a9ee, 0xff00adf1,
        0xfd00adee, 0x9e006d95, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000304, 0x34002133, 0xa60069a1, 0xd70089d1, 0xf2009bea, 0xff00a3f7, 0xfb00a1f2, 0xfb00a2f2, 0xff00a6f5,
        0xff00a8f4, 0xfd00a7f2, 0xed009ee2, 0xcf008bc5, 0x14000e13, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000b11, 0x35002134, 0x5b003959,
        0x8b005887, 0xb30072ae, 0xc90080c3, 0xd10086ca, 0xa6006ba0, 0x65004261, 0x3c002839, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x01000101, 0x06000406, 0x0d00080d, 0x0f000a0f, 0x0a00060a, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000};

    auto hackcursor     = makeShared<SXCursor>();
    hackcursor->size    = {cursorWidth, cursorHeight};
    hackcursor->hotspot = {cursorHotspotX, cursorHotspotY};
    hackcursor->pixels.resize(cursorWidth * cursorHeight);
    std::memcpy(hackcursor->pixels.data(), pixels, cursorWidth * cursorHeight * sizeof(uint32_t));

    return hackcursor;
}
