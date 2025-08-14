#include "CursorManager.hpp"
#include "Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "PointerManager.hpp"
#include "../xwayland/XWayland.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../helpers/Monitor.hpp"

static int cursorAnimTimer(SP<CEventLoopTimer> self, void* data) {
    const auto cursorMgr = sc<CCursorManager*>(data);
    cursorMgr->tickAnimatedCursor();
    return 1;
}

static void hcLogger(enum eHyprcursorLogLevel level, char* message) {
    if (level == HC_LOG_TRACE)
        return;

    Debug::log(NONE, "[hc] {}", message);
}

CCursorBuffer::CCursorBuffer(cairo_surface_t* surf, const Vector2D& size_, const Vector2D& hot_) : m_hotspot(hot_), m_stride(cairo_image_surface_get_stride(surf)) {
    size = size_;

    m_data = std::vector<uint8_t>(cairo_image_surface_get_data(surf), cairo_image_surface_get_data(surf) + (cairo_image_surface_get_height(surf) * m_stride));
}

CCursorBuffer::CCursorBuffer(const uint8_t* pixelData, const Vector2D& size_, const Vector2D& hot_) : m_hotspot(hot_), m_stride(4 * size_.x) {
    size = size_;

    m_data = std::vector<uint8_t>(pixelData, pixelData + (sc<int>(size_.y) * m_stride));
}

Aquamarine::eBufferCapability CCursorBuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CCursorBuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_SHM;
}

void CCursorBuffer::update(const Hyprutils::Math::CRegion& damage) {
    ;
}

bool CCursorBuffer::isSynchronous() {
    return true;
}

bool CCursorBuffer::good() {
    return true;
}

Aquamarine::SSHMAttrs CCursorBuffer::shm() {
    Aquamarine::SSHMAttrs attrs;
    attrs.success = true;
    attrs.format  = DRM_FORMAT_ARGB8888;
    attrs.size    = size;
    attrs.stride  = m_stride;
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CCursorBuffer::beginDataPtr(uint32_t flags) {
    return {m_data.data(), DRM_FORMAT_ARGB8888, m_stride};
}

void CCursorBuffer::endDataPtr() {
    ;
}

CCursorManager::CCursorManager() {
    m_hyprcursor               = makeUnique<Hyprcursor::CHyprcursorManager>(m_theme.empty() ? nullptr : m_theme.c_str(), hcLogger);
    m_xcursor                  = makeUnique<CXCursorManager>();
    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");

    if (m_hyprcursor->valid() && *PUSEHYPRCURSOR) {
        // find default size. First, HYPRCURSOR_SIZE then default to 24
        auto const* SIZE = getenv("HYPRCURSOR_SIZE");
        if (SIZE) {
            try {
                m_size = std::stoi(SIZE);
            } catch (...) { ; }
        }

        if (m_size <= 0) {
            Debug::log(WARN, "HYPRCURSOR_SIZE size not set, defaulting to size 24");
            m_size = 24;
        }
    } else {
        Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to Xcursor.", m_theme);

        auto const* SIZE = getenv("XCURSOR_SIZE");
        if (SIZE) {
            try {
                m_size = std::stoi(SIZE);
            } catch (...) { ; }
        }

        if (m_size <= 0) {
            Debug::log(WARN, "XCURSOR_SIZE size not set, defaulting to size 24");
            m_size = 24;
        }
    }

    // since we fallback to xcursor always load it on startup. otherwise we end up with a empty theme if hyprcursor is enabled in the config
    // and then later is disabled.
    m_xcursor->loadTheme(getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "default", m_size, m_cursorScale);

    m_animationTimer = makeShared<CEventLoopTimer>(std::nullopt, cursorAnimTimer, this);
    g_pEventLoopManager->addTimer(m_animationTimer);

    updateTheme();

    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateTheme(); });
}

CCursorManager::~CCursorManager() {
    if (m_animationTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_animationTimer);
        m_animationTimer.reset();
    }

    if (m_hyprcursor->valid() && m_currentStyleInfo.size > 0)
        m_hyprcursor->cursorSurfaceStyleDone(m_currentStyleInfo);
}

SP<Aquamarine::IBuffer> CCursorManager::getCursorBuffer() {
    return !m_cursorBuffers.empty() ? m_cursorBuffers.back() : nullptr;
}

void CCursorManager::setCursorSurface(SP<CWLSurface> surf, const Vector2D& hotspot) {
    if (!surf || !surf->resource())
        g_pPointerManager->resetCursorImage();
    else
        g_pPointerManager->setCursorSurface(surf, hotspot);

    m_ourBufferConnected = false;
}

void CCursorManager::setCursorBuffer(SP<CCursorBuffer> buf, const Vector2D& hotspot, const float& scale) {
    m_cursorBuffers.emplace_back(buf);
    g_pPointerManager->setCursorBuffer(getCursorBuffer(), hotspot, scale);
    if (m_cursorBuffers.size() > 1)
        std::erase_if(m_cursorBuffers, [this](const auto& buf) { return buf.get() == m_cursorBuffers.front().get(); });

    m_ourBufferConnected = true;
}

void CCursorManager::setAnimationTimer(const int& frame, const int& delay) {
    if (delay > 0) {
        // arm
        m_animationTimer->updateTimeout(std::chrono::milliseconds(delay));
    } else {
        // disarm
        m_animationTimer->updateTimeout(std::nullopt);
    }

    m_currentAnimationFrame = frame;
}

void CCursorManager::setCursorFromName(const std::string& name) {

    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");

    auto        setXCursor = [this](auto const& name) {
        float scale = std::ceil(m_cursorScale);

        auto  xcursor = m_xcursor->getShape(name, m_size, m_cursorScale);
        auto& icon    = xcursor->images.front();
        auto  buf     = makeShared<CCursorBuffer>(rc<uint8_t*>(icon.pixels.data()), icon.size, icon.hotspot);
        setCursorBuffer(buf, icon.hotspot / scale, scale);

        m_currentXcursor = xcursor;

        int delay = 0;
        int frame = 0;
        if (m_currentXcursor->images.size() > 1)
            delay = m_currentXcursor->images[frame].delay;

        setAnimationTimer(frame, delay);
    };

    auto setHyprCursor = [this](auto const& name) {
        m_currentCursorShapeData = m_hyprcursor->getShape(name.c_str(), m_currentStyleInfo);

        if (m_currentCursorShapeData.images.empty()) {
            // try with '_' first (old hc, etc)
            std::string newName = name;
            std::ranges::replace(newName, '-', '_');

            m_currentCursorShapeData = m_hyprcursor->getShape(newName.c_str(), m_currentStyleInfo);
        }

        if (m_currentCursorShapeData.images.empty()) {
            // fallback to a default if available
            constexpr const std::array<const char*, 3> fallbackShapes = {"default", "left_ptr", "left-ptr"};

            for (auto const& s : fallbackShapes) {
                m_currentCursorShapeData = m_hyprcursor->getShape(s, m_currentStyleInfo);

                if (!m_currentCursorShapeData.images.empty())
                    break;
            }

            if (m_currentCursorShapeData.images.empty()) {
                Debug::log(ERR, "BUG THIS: No fallback found for a cursor in setCursorFromName");
                return false;
            }
        }

        auto buf = makeShared<CCursorBuffer>(m_currentCursorShapeData.images[0].surface, Vector2D{m_currentCursorShapeData.images[0].size, m_currentCursorShapeData.images[0].size},
                                             Vector2D{m_currentCursorShapeData.images[0].hotspotX, m_currentCursorShapeData.images[0].hotspotY});
        auto hotspot = Vector2D{m_currentCursorShapeData.images[0].hotspotX, m_currentCursorShapeData.images[0].hotspotY} / m_cursorScale;
        setCursorBuffer(buf, hotspot, m_cursorScale);

        int delay = 0;
        int frame = 0;
        if (m_currentCursorShapeData.images.size() > 1)
            delay = m_currentCursorShapeData.images[frame].delay;

        setAnimationTimer(frame, delay);
        return true;
    };

    if (!m_hyprcursor->valid() || !*PUSEHYPRCURSOR || !setHyprCursor(name))
        setXCursor(name);
}

void CCursorManager::tickAnimatedCursor() {
    if (!m_ourBufferConnected)
        return;

    if (!m_hyprcursor->valid() && m_currentXcursor->images.size() > 1) {
        m_currentAnimationFrame++;

        if (sc<size_t>(m_currentAnimationFrame) >= m_currentXcursor->images.size())
            m_currentAnimationFrame = 0;

        float scale = std::ceil(m_cursorScale);
        auto& icon  = m_currentXcursor->images.at(m_currentAnimationFrame);
        auto  buf   = makeShared<CCursorBuffer>(rc<uint8_t*>(icon.pixels.data()), icon.size, icon.hotspot);
        setCursorBuffer(buf, icon.hotspot / scale, scale);
        setAnimationTimer(m_currentAnimationFrame, m_currentXcursor->images[m_currentAnimationFrame].delay);
    } else if (m_currentCursorShapeData.images.size() > 1) {
        m_currentAnimationFrame++;

        if (sc<size_t>(m_currentAnimationFrame) >= m_currentCursorShapeData.images.size())
            m_currentAnimationFrame = 0;

        auto hotspot =
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotX, m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotY} / m_cursorScale;
        auto buf = makeShared<CCursorBuffer>(
            m_currentCursorShapeData.images[m_currentAnimationFrame].surface,
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].size, m_currentCursorShapeData.images[m_currentAnimationFrame].size},
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotX, m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotY});
        setCursorBuffer(buf, hotspot, m_cursorScale);
        setAnimationTimer(m_currentAnimationFrame, m_currentCursorShapeData.images[m_currentAnimationFrame].delay);
    }
}

SCursorImageData CCursorManager::dataFor(const std::string& name) {

    if (!m_hyprcursor->valid())
        return {};

    const auto IMAGES = m_hyprcursor->getShape(name.c_str(), m_currentStyleInfo);

    if (IMAGES.images.empty())
        return {};

    return IMAGES.images[0];
}

void CCursorManager::setXWaylandCursor() {
    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");
    const auto  CURSOR         = dataFor("left_ptr");
    if (CURSOR.surface && *PUSEHYPRCURSOR)
        g_pXWayland->setCursor(cairo_image_surface_get_data(CURSOR.surface), cairo_image_surface_get_stride(CURSOR.surface), {CURSOR.size, CURSOR.size},
                               {CURSOR.hotspotX, CURSOR.hotspotY});
    else {
        auto  xcursor = m_xcursor->getShape("left_ptr", m_size, 1);
        auto& icon    = xcursor->images.front();

        g_pXWayland->setCursor(rc<uint8_t*>(icon.pixels.data()), icon.size.x * 4, icon.size, icon.hotspot);
    }
}

void CCursorManager::updateTheme() {
    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");
    float       highestScale   = 1.0;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->m_scale > highestScale)
            highestScale = m->m_scale;
    }

    m_cursorScale            = highestScale;
    m_currentCursorShapeData = {};

    if (*PUSEHYPRCURSOR) {
        if (m_currentStyleInfo.size > 0 && m_hyprcursor->valid())
            m_hyprcursor->cursorSurfaceStyleDone(m_currentStyleInfo);

        m_currentStyleInfo.size = std::round(m_size * highestScale);

        if (m_hyprcursor->valid())
            m_hyprcursor->loadThemeStyle(m_currentStyleInfo);
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        m->m_forceFullFrames = 5;
        g_pCompositor->scheduleFrameForMonitor(m, Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_SHAPE);
    }
}

bool CCursorManager::changeTheme(const std::string& name, const int size) {
    static auto PUSEHYPRCURSOR = CConfigValue<Hyprlang::INT>("cursor:enable_hyprcursor");
    m_theme                    = name.empty() ? "" : name;
    m_size                     = size <= 0 ? 24 : size;
    auto xcursor_theme         = getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "default";

    if (*PUSEHYPRCURSOR) {
        auto options                 = Hyprcursor::SManagerOptions();
        options.logFn                = hcLogger;
        options.allowDefaultFallback = false;
        m_theme                      = name.empty() ? "" : name;
        m_size                       = size;

        m_hyprcursor = makeUnique<Hyprcursor::CHyprcursorManager>(m_theme.empty() ? nullptr : m_theme.c_str(), options);
        if (!m_hyprcursor->valid()) {
            Debug::log(ERR, "Hyprcursor failed loading theme \"{}\", falling back to XCursor.", m_theme);
            m_xcursor->loadTheme(m_theme.empty() ? xcursor_theme : m_theme, m_size, m_cursorScale);
        }
    } else
        m_xcursor->loadTheme(m_theme.empty() ? xcursor_theme : m_theme, m_size, m_cursorScale);

    updateTheme();

    return true;
}

void CCursorManager::syncGsettings() {
    m_xcursor->syncGsettings();
}

float CCursorManager::getScaledSize() const {
    return m_size * m_cursorScale;
}
