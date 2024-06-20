#include "CursorManager.hpp"
#include "Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "PointerManager.hpp"
#include "../xwayland/XWayland.hpp"

extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_xcursor_manager.h>
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

    m_pWLRXCursorMgr = wlr_xcursor_manager_create(getenv("XCURSOR_THEME"), m_iSize);
    wlr_xcursor_manager_load(m_pWLRXCursorMgr, 1.0);

    m_pAnimationTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, ::cursorAnimTimer, nullptr);

    updateTheme();

    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateTheme(); });
}

CCursorManager::~CCursorManager() {
    if (m_pWLRXCursorMgr)
        wlr_xcursor_manager_destroy(m_pWLRXCursorMgr);

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
    if (!m_pWLRXCursorMgr) {
        g_pPointerManager->resetCursorImage();
        return;
    }

    float scale = std::ceil(m_fCursorScale);
    wlr_xcursor_manager_load(m_pWLRXCursorMgr, scale);

    auto xcursor = wlr_xcursor_manager_get_xcursor(m_pWLRXCursorMgr, name.c_str(), scale);
    if (!xcursor) {
        Debug::log(ERR, "XCursor has no shape {}, retrying with left-ptr", name);
        xcursor = wlr_xcursor_manager_get_xcursor(m_pWLRXCursorMgr, "left-ptr", scale);
    }

    if (!xcursor || !xcursor->images[0]) {
        Debug::log(ERR, "XCursor is broken. F this garbage.");
        g_pPointerManager->resetCursorImage();
        return;
    }

    auto image = xcursor->images[0];

    m_vCursorBuffers.emplace_back(
        makeShared<CCursorBuffer>(image->buffer, Vector2D{(int)image->width, (int)image->height}, Vector2D{(double)image->hotspot_x, (double)image->hotspot_y}));

    g_pPointerManager->setCursorBuffer(getCursorBuffer(), Vector2D{(double)image->hotspot_x, (double)image->hotspot_y} / scale, scale);
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
    } else if (const auto XCURSOR = wlr_xcursor_manager_get_xcursor(m_pWLRXCursorMgr, "left_ptr", 1); XCURSOR) {
        g_pXWayland->setCursor(XCURSOR->images[0]->buffer, XCURSOR->images[0]->width * 4, {(int)XCURSOR->images[0]->width, (int)XCURSOR->images[0]->height},
                               {(double)XCURSOR->images[0]->hotspot_x, (double)XCURSOR->images[0]->hotspot_y});
    } else
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

    if (m_pWLRXCursorMgr)
        wlr_xcursor_manager_destroy(m_pWLRXCursorMgr);

    m_pWLRXCursorMgr = wlr_xcursor_manager_create(name.empty() ? "" : name.c_str(), size);
    bool xSuccess    = wlr_xcursor_manager_load(m_pWLRXCursorMgr, 1.0) == 1;

    // this basically checks if xcursor changed used theme to default but better
    bool                       diffTheme = false;
    wlr_xcursor_manager_theme* theme;
    wl_list_for_each(theme, &m_pWLRXCursorMgr->scaled_themes, link) {
        if (std::string{theme->theme->name} != name) {
            diffTheme = true;
            break;
        }
    }

    if (xSuccess && !diffTheme) {
        m_szTheme = name;
        m_iSize   = size;
        updateTheme();
        return true;
    }

    Debug::log(ERR, "X also failed loading theme \"{}\", falling back to previous theme.", name);

    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(m_szTheme.c_str(), hcLogger);

    wlr_xcursor_manager_destroy(m_pWLRXCursorMgr);
    m_pWLRXCursorMgr = wlr_xcursor_manager_create(m_szTheme.c_str(), m_iSize);
    wlr_xcursor_manager_load(m_pWLRXCursorMgr, 1.0);

    updateTheme();
    return false;
}
