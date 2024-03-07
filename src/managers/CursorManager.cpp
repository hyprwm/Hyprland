#include "CursorManager.hpp"
#include "Compositor.hpp"

extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
}

CCursorManager::CCursorManager() {
    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(m_szTheme.empty() ? nullptr : m_szTheme.c_str());

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

    updateTheme();

    g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateTheme(); });
}

static void cursorBufferDestroy(struct wlr_buffer* wlr_buffer) {
    CCursorManager::CCursorBuffer::SCursorWlrBuffer* buffer = wl_container_of(wlr_buffer, buffer, base);
    buffer->dropped                                         = true;
}

static bool cursorBufferBeginDataPtr(struct wlr_buffer* wlr_buffer, uint32_t flags, void** data, uint32_t* format, size_t* stride) {
    CCursorManager::CCursorBuffer::SCursorWlrBuffer* buffer = wl_container_of(wlr_buffer, buffer, base);

    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
        return false;

    *data   = cairo_image_surface_get_data(buffer->surface);
    *stride = cairo_image_surface_get_stride(buffer->surface);
    *format = DRM_FORMAT_ARGB8888;
    return true;
}

static void cursorBufferEndDataPtr(struct wlr_buffer* wlr_buffer) {
    ;
}

//
static const wlr_buffer_impl bufferImpl = {
    .destroy               = cursorBufferDestroy,
    .begin_data_ptr_access = cursorBufferBeginDataPtr,
    .end_data_ptr_access   = cursorBufferEndDataPtr,
};

CCursorManager::CCursorBuffer::CCursorBuffer(cairo_surface_t* surf, const Vector2D& size_, const Vector2D& hot_) : size(size_), hotspot(hot_) {
    wlrBuffer.surface = surf;
    wlr_buffer_init(&wlrBuffer.base, &bufferImpl, size.x, size.y);
}

CCursorManager::CCursorBuffer::~CCursorBuffer() {
    if (g_pCursorManager->m_bOurBufferConnected)
        wlr_cursor_set_surface(g_pCompositor->m_sWLRCursor, nullptr, 0, 0);

    if (!wlrBuffer.dropped)
        wlr_buffer_drop(&wlrBuffer.base);
}

wlr_buffer* CCursorManager::getCursorBuffer() {
    return m_sCursorBuffer ? &m_sCursorBuffer->wlrBuffer.base : nullptr;
}

void CCursorManager::setCursorSurface(wlr_surface* surf, const Vector2D& hotspot) {
    wlr_cursor_set_surface(g_pCompositor->m_sWLRCursor, surf, hotspot.x, hotspot.y);

    m_bOurBufferConnected = false;
}

void CCursorManager::setCursorFromName(const std::string& name) {
    if (m_sCursorBuffer)
        m_sCursorBuffer.reset();

    const auto IMAGES = m_pHyprcursor->getShape(name.c_str(), m_sCurrentStyleInfo);

    m_sCursorBuffer = std::make_unique<CCursorBuffer>(IMAGES.images[0].surface, Vector2D{IMAGES.images[0].size, IMAGES.images[0].size},
                                                      Vector2D{IMAGES.images[0].hotspotX, IMAGES.images[0].hotspotY});

    if (g_pCompositor->m_sWLRCursor)
        wlr_cursor_set_buffer(g_pCompositor->m_sWLRCursor, getCursorBuffer(), IMAGES.images[0].hotspotX, IMAGES.images[0].hotspotY, m_fCursorScale);

    m_bOurBufferConnected = true;
}

SCursorImageData CCursorManager::dataFor(const std::string& name) {
    const auto IMAGES = m_pHyprcursor->getShape(name.c_str(), m_sCurrentStyleInfo);

    if (IMAGES.images.empty())
        return {};

    return IMAGES.images[0];
}

void CCursorManager::updateTheme() {
    float highestScale = 1.0;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->scale > highestScale)
            highestScale = m->scale;
    }

    if (highestScale * m_iSize == m_sCurrentStyleInfo.size)
        return;

    if (m_sCurrentStyleInfo.size)
        m_pHyprcursor->cursorSurfaceStyleDone(m_sCurrentStyleInfo);

    m_sCurrentStyleInfo.size = m_iSize * highestScale;
    m_fCursorScale           = highestScale;

    m_pHyprcursor->loadThemeStyle(m_sCurrentStyleInfo);

    setCursorFromName("left_ptr");

    for (auto& m : g_pCompositor->m_vMonitors) {
        m->forceFullFrames = 5;
        g_pCompositor->scheduleFrameForMonitor(m.get());
    }
}

void CCursorManager::changeTheme(const std::string& name, const int size) {
    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(name.empty() ? "" : name.c_str());
    m_szTheme     = name;
    m_iSize       = size;

    updateTheme();
}
