#include "CursorManager.hpp"
#include "Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "PointerManager.hpp"
#include "../xwayland/XWayland.hpp"

static int cursorAnimTimer(SP<CEventLoopTimer> self, void* data) {
    const auto cursorMgr = reinterpret_cast<CCursorManager*>(data);
    cursorMgr->tickAnimatedCursor();
    return 1;
}

static void hcLogger(enum eHyprcursorLogLevel level, char* message) {
    if (level == HC_LOG_TRACE)
        return;

    Debug::log(NONE, "[hc] {}", message);
}

CCursorManager::CCursorManager() {
    m_pHyprcursor = std::make_unique<Hyprcursor::CHyprcursorManager>(m_szTheme.empty() ? nullptr : m_szTheme.c_str(), hcLogger);
    m_pXcursor    = std::make_unique<CXCursorManager>();

    if (m_pHyprcursor->valid()) {
        // find default size. First, HYPRCURSOR_SIZE then default to 24
        auto const* SIZE = getenv("HYPRCURSOR_SIZE");
        if (SIZE) {
            try {
                m_iSize = std::stoi(SIZE);
            } catch (...) { ; }
        }

        if (m_iSize <= 0) {
            Debug::log(WARN, "HYPRCURSOR_SIZE size not set, defaulting to size 24");
            m_iSize = 24;
        }
    } else {
        Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to Xcursor.", m_szTheme);

        auto const* SIZE = getenv("XCURSOR_SIZE");
        if (SIZE) {
            try {
                m_iSize = std::stoi(SIZE);
            } catch (...) { ; }
        }

        if (m_iSize <= 0) {
            Debug::log(WARN, "XCURSOR_SIZE size not set, defaulting to size 24");
            m_iSize = 24;
        }

        m_pXcursor->loadTheme(getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "default", m_iSize * std::ceil(m_fCursorScale));
    }

    m_pAnimationTimer = makeShared<CEventLoopTimer>(std::nullopt, cursorAnimTimer, this);
    g_pEventLoopManager->addTimer(m_pAnimationTimer);

    updateTheme();

    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateTheme(); });
}

CCursorManager::~CCursorManager() {
    if (m_pAnimationTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_pAnimationTimer);
        m_pAnimationTimer.reset();
    }
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

    auto  xcursor = m_pXcursor->getShape(name, m_iSize * scale);
    auto& icon    = xcursor->images.front();

    m_vCursorBuffers.emplace_back(makeShared<CCursorBuffer>((uint8_t*)icon.pixels.data(), icon.size, icon.hotspot));

    g_pPointerManager->setCursorBuffer(getCursorBuffer(), icon.hotspot / scale, scale);
    if (m_vCursorBuffers.size() > 1)
        dropBufferRef(m_vCursorBuffers.at(0).get());

    m_currentXcursor      = xcursor;
    m_bOurBufferConnected = true;

    if (m_currentXcursor->images.size() > 1) {
        // animated
        m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(m_currentXcursor->images[0].delay));
        m_iCurrentAnimationFrame = 0;
    } else {
        // disarm
        m_pAnimationTimer->updateTimeout(std::nullopt);
    }
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
        m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(m_sCurrentCursorShapeData.images[0].delay));
        m_iCurrentAnimationFrame = 0;
    } else {
        // disarm
        m_pAnimationTimer->updateTimeout(std::nullopt);
    }
}

void CCursorManager::tickAnimatedCursor() {
    if (!m_pHyprcursor->valid() && m_currentXcursor->images.size() > 1 && m_bOurBufferConnected) {
        m_iCurrentAnimationFrame++;

        if ((size_t)m_iCurrentAnimationFrame >= m_currentXcursor->images.size())
            m_iCurrentAnimationFrame = 0;

        float scale = std::ceil(m_fCursorScale);
        auto& icon  = m_currentXcursor->images.at(m_iCurrentAnimationFrame);
        m_vCursorBuffers.emplace_back(makeShared<CCursorBuffer>((uint8_t*)icon.pixels.data(), icon.size, icon.hotspot));

        g_pPointerManager->setCursorBuffer(getCursorBuffer(), icon.hotspot / scale, scale);

        if (m_vCursorBuffers.size() > 1)
            dropBufferRef(m_vCursorBuffers.at(0).get());

        m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(m_currentXcursor->images[m_iCurrentAnimationFrame].delay));

        return;
    }

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

    if (m_vCursorBuffers.size() > 1)
        dropBufferRef(m_vCursorBuffers.at(0).get());

    m_pAnimationTimer->updateTimeout(std::chrono::milliseconds(m_sCurrentCursorShapeData.images[m_iCurrentAnimationFrame].delay));
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
    } else {
        auto  xcursor = m_pXcursor->getShape("left_ptr", m_iSize * std::ceil(m_fCursorScale));
        auto& icon    = xcursor->images.front();

        g_pXWayland->setCursor((uint8_t*)icon.pixels.data(), icon.size.x * 4, icon.size, icon.hotspot);
    }
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

    Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to XCursor.", name);

    m_pXcursor->loadTheme(name, size);

    m_szTheme = name;
    m_iSize   = size;
    updateTheme();
    return true;
}