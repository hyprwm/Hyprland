#include <GLES3/gl32.h>
#include <cstdint>
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>
#include <numbers>
#include <random>
#include <pango/pangocairo.h>
#include "OpenGL.hpp"
#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/CursorShapes.hpp"
#include "../helpers/TransferFunction.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/legacy/ConfigManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../helpers/cm/ColorManagement.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/CursorManager.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "../helpers/env/Env.hpp"
#include "../helpers/MainLoopExecutor.hpp"
#include "../i18n/Engine.hpp"
#include "../event/EventBus.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "hyprerror/HyprError.hpp"
#include "macros.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/PreBlurElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "GLRenderer.hpp"
#include "Shader.hpp"
#include "AsyncResourceGatherer.hpp"
#include <ranges>
#include <algorithm>
#include <fstream>
#include <string>
#include <xf86drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <filesystem>
#include <cstring>
#include "./shaders/Shaders.hpp"
#include "ShaderLoader.hpp"
#include "Texture.hpp"
#include "gl/GLFramebuffer.hpp"
#include "gl/GLTexture.hpp"

using namespace Hyprutils::OS;
using namespace NColorManagement;
using namespace Render;
using namespace Render::GL;

static inline void loadGLProc(void* pProc, const char* name) {
    void* proc = rc<void*>(eglGetProcAddress(name));
    if (proc == nullptr) {
        Log::logger->log(Log::CRIT, "[Tracy GPU Profiling] eglGetProcAddress({}) failed", name);
        abort();
    }
    *sc<void**>(pProc) = proc;
}

static enum Hyprutils::CLI::eLogLevel eglLogToLevel(EGLint type) {
    switch (type) {
        case EGL_DEBUG_MSG_CRITICAL_KHR: return Log::CRIT;
        case EGL_DEBUG_MSG_ERROR_KHR: return Log::ERR;
        case EGL_DEBUG_MSG_WARN_KHR: return Log::WARN;
        case EGL_DEBUG_MSG_INFO_KHR: return Log::DEBUG;
        default: return Log::DEBUG;
    }
}

static const char* eglErrorToString(EGLint error) {
    switch (error) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_DEVICE_EXT: return "EGL_BAD_DEVICE_EXT";
        case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
    }
    return "Unknown";
}

static void eglLog(EGLenum error, const char* command, EGLint type, EGLLabelKHR thread, EGLLabelKHR obj, const char* msg) {
    Log::logger->log(eglLogToLevel(type), "[EGL] Command {} errored out with {} (0x{}): {}", command, eglErrorToString(error), error, msg);
}

static int openRenderNode(int drmFd) {
    auto renderName = drmGetRenderDeviceNameFromFd(drmFd);
    if (!renderName) {
        // This can happen on split render/display platforms, fallback to
        // primary node
        renderName = drmGetPrimaryDeviceNameFromFd(drmFd);
        if (!renderName) {
            Log::logger->log(Log::ERR, "drmGetPrimaryDeviceNameFromFd failed");
            return -1;
        }
        Log::logger->log(Log::DEBUG, "DRM dev {} has no render node, falling back to primary", renderName);

        drmVersion* render_version = drmGetVersion(drmFd);
        if (render_version && render_version->name) {
            Log::logger->log(Log::DEBUG, "DRM dev versionName {}", render_version->name);
            if (strcmp(render_version->name, "evdi") == 0) {
                free(renderName); // NOLINT(cppcoreguidelines-no-malloc)
                renderName = strdup("/dev/dri/card0");
            }
            drmFreeVersion(render_version);
        }
    }

    Log::logger->log(Log::DEBUG, "openRenderNode got drm device {}", renderName);

    int renderFD = open(renderName, O_RDWR | O_CLOEXEC);
    if (renderFD < 0)
        Log::logger->log(Log::ERR, "openRenderNode failed to open drm device {}", renderName);

    free(renderName); // NOLINT(cppcoreguidelines-no-malloc)
    return renderFD;
}

static ShaderFeatureFlags globalFeatures() {
    return g_pHyprRenderer->m_renderData.pMonitor && g_pHyprRenderer->m_renderData.pMonitor->needsUnmodifiedCopy() && g_pHyprRenderer->m_renderData.currentFB->getMirrorTexture() ?
        SH_FEAT_MIRROR :
        0;
}

void CHyprOpenGLImpl::initEGL(bool gbm) {
    std::vector<EGLint> attrs;
    if (m_exts.KHR_display_reference) {
        attrs.push_back(EGL_TRACK_REFERENCES_KHR);
        attrs.push_back(EGL_TRUE);
    }

    attrs.push_back(EGL_NONE);

    m_eglDisplay = m_proc.eglGetPlatformDisplayEXT(gbm ? EGL_PLATFORM_GBM_KHR : EGL_PLATFORM_DEVICE_EXT, gbm ? m_gbmDevice : m_eglDevice, attrs.data());
    if (m_eglDisplay == EGL_NO_DISPLAY)
        RASSERT(false, "EGL: failed to create a platform display");

    attrs.clear();

    EGLint major, minor;
    if (eglInitialize(m_eglDisplay, &major, &minor) == EGL_FALSE)
        RASSERT(false, "EGL: failed to initialize a platform display");

    const std::string EGLEXTENSIONS = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);

    m_exts.IMG_context_priority               = EGLEXTENSIONS.contains("IMG_context_priority");
    m_exts.EXT_create_context_robustness      = EGLEXTENSIONS.contains("EXT_create_context_robustness");
    m_exts.EXT_image_dma_buf_import           = EGLEXTENSIONS.contains("EXT_image_dma_buf_import");
    m_exts.EXT_image_dma_buf_import_modifiers = EGLEXTENSIONS.contains("EXT_image_dma_buf_import_modifiers");
    m_exts.KHR_context_flush_control          = EGLEXTENSIONS.contains("EGL_KHR_context_flush_control");

    if (m_exts.IMG_context_priority) {
        Log::logger->log(Log::DEBUG, "EGL: IMG_context_priority supported, requesting high");
        attrs.push_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
        attrs.push_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }

    if (m_exts.EXT_create_context_robustness) {
        Log::logger->log(Log::DEBUG, "EGL: EXT_create_context_robustness supported, requesting lose on reset");
        attrs.push_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
        attrs.push_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);
    }

    if (m_exts.KHR_context_flush_control) {
        Log::logger->log(Log::DEBUG, "EGL: Using KHR_context_flush_control");
        attrs.push_back(EGL_CONTEXT_RELEASE_BEHAVIOR_KHR);
        attrs.push_back(EGL_CONTEXT_RELEASE_BEHAVIOR_NONE_KHR); // or _FLUSH_KHR
    }

    auto attrsNoVer = attrs;

    attrs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    attrs.push_back(3);
    attrs.push_back(EGL_CONTEXT_MINOR_VERSION);
    attrs.push_back(2);
    attrs.push_back(EGL_NONE);

    m_eglContext = eglCreateContext(m_eglDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrs.data());
    if (m_eglContext == EGL_NO_CONTEXT) {
        Log::logger->log(Log::WARN, "EGL: Failed to create a context with GLES3.2, retrying 3.0");

        attrs = attrsNoVer;
        attrs.push_back(EGL_CONTEXT_MAJOR_VERSION);
        attrs.push_back(3);
        attrs.push_back(EGL_CONTEXT_MINOR_VERSION);
        attrs.push_back(0);
        attrs.push_back(EGL_NONE);

        m_eglContext        = eglCreateContext(m_eglDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrs.data());
        m_eglContextVersion = EGL_CONTEXT_GLES_3_0;

        if (m_eglContext == EGL_NO_CONTEXT)
            RASSERT(false, "EGL: failed to create a context with either GLES3.2 or 3.0");
    }

    if (m_exts.IMG_context_priority) {
        EGLint priority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
        eglQueryContext(m_eglDisplay, m_eglContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &priority);
        if (priority != EGL_CONTEXT_PRIORITY_HIGH_IMG)
            Log::logger->log(Log::ERR, "EGL: Failed to obtain a high priority context");
        else
            Log::logger->log(Log::DEBUG, "EGL: Got a high priority context");
    }

    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext);
}

static bool drmDeviceHasName(const drmDevice* device, const std::string& name) {
    for (size_t i = 0; i < DRM_NODE_MAX; i++) {
        if (!(device->available_nodes & (1 << i)))
            continue;

        if (device->nodes[i] == name)
            return true;
    }
    return false;
}

EGLDeviceEXT CHyprOpenGLImpl::eglDeviceFromDRMFD(int drmFD) {
    EGLint nDevices = 0;
    if (!m_proc.eglQueryDevicesEXT(0, nullptr, &nDevices)) {
        Log::logger->log(Log::ERR, "eglDeviceFromDRMFD: eglQueryDevicesEXT failed");
        return EGL_NO_DEVICE_EXT;
    }

    if (nDevices <= 0) {
        Log::logger->log(Log::ERR, "eglDeviceFromDRMFD: no devices");
        return EGL_NO_DEVICE_EXT;
    }

    std::vector<EGLDeviceEXT> devices;
    devices.resize(nDevices);

    if (!m_proc.eglQueryDevicesEXT(nDevices, devices.data(), &nDevices)) {
        Log::logger->log(Log::ERR, "eglDeviceFromDRMFD: eglQueryDevicesEXT failed (2)");
        return EGL_NO_DEVICE_EXT;
    }

    drmDevice* drmDev = nullptr;
    if (int ret = drmGetDevice(drmFD, &drmDev); ret < 0) {
        Log::logger->log(Log::ERR, "eglDeviceFromDRMFD: drmGetDevice failed");
        return EGL_NO_DEVICE_EXT;
    }

    for (auto const& d : devices) {
        auto devName = m_proc.eglQueryDeviceStringEXT(d, EGL_DRM_DEVICE_FILE_EXT);
        if (!devName)
            continue;

        if (drmDeviceHasName(drmDev, devName)) {
            Log::logger->log(Log::DEBUG, "eglDeviceFromDRMFD: Using device {}", devName);
            drmFreeDevice(&drmDev);
            return d;
        }
    }

    drmFreeDevice(&drmDev);
    Log::logger->log(Log::DEBUG, "eglDeviceFromDRMFD: No drm devices found");
    return EGL_NO_DEVICE_EXT;
}

CHyprOpenGLImpl::CHyprOpenGLImpl() : m_drmFD(g_pCompositor->m_drmRenderNode.fd >= 0 ? g_pCompositor->m_drmRenderNode.fd : g_pCompositor->m_drm.fd) {
    const std::string EGLEXTENSIONS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    Log::logger->log(Log::DEBUG, "Supported EGL global extensions: ({}) {}", std::ranges::count(EGLEXTENSIONS, ' '), EGLEXTENSIONS);

    m_exts.KHR_display_reference = EGLEXTENSIONS.contains("KHR_display_reference");

    loadGLProc(&m_proc.glEGLImageTargetRenderbufferStorageOES, "glEGLImageTargetRenderbufferStorageOES");
    loadGLProc(&m_proc.eglCreateImageKHR, "eglCreateImageKHR");
    loadGLProc(&m_proc.eglDestroyImageKHR, "eglDestroyImageKHR");
    loadGLProc(&m_proc.eglQueryDmaBufFormatsEXT, "eglQueryDmaBufFormatsEXT");
    loadGLProc(&m_proc.eglQueryDmaBufModifiersEXT, "eglQueryDmaBufModifiersEXT");
    loadGLProc(&m_proc.glEGLImageTargetTexture2DOES, "glEGLImageTargetTexture2DOES");
    loadGLProc(&m_proc.eglDebugMessageControlKHR, "eglDebugMessageControlKHR");
    loadGLProc(&m_proc.eglGetPlatformDisplayEXT, "eglGetPlatformDisplayEXT");
    loadGLProc(&m_proc.eglCreateSyncKHR, "eglCreateSyncKHR");
    loadGLProc(&m_proc.eglDestroySyncKHR, "eglDestroySyncKHR");
    loadGLProc(&m_proc.eglDupNativeFenceFDANDROID, "eglDupNativeFenceFDANDROID");
    loadGLProc(&m_proc.eglWaitSyncKHR, "eglWaitSyncKHR");

    RASSERT(m_proc.eglCreateSyncKHR, "Display driver doesn't support eglCreateSyncKHR");
    RASSERT(m_proc.eglDupNativeFenceFDANDROID, "Display driver doesn't support eglDupNativeFenceFDANDROID");
    RASSERT(m_proc.eglWaitSyncKHR, "Display driver doesn't support eglWaitSyncKHR");

    if (EGLEXTENSIONS.contains("EGL_EXT_device_base") || EGLEXTENSIONS.contains("EGL_EXT_device_enumeration"))
        loadGLProc(&m_proc.eglQueryDevicesEXT, "eglQueryDevicesEXT");

    if (EGLEXTENSIONS.contains("EGL_EXT_device_base") || EGLEXTENSIONS.contains("EGL_EXT_device_query")) {
        loadGLProc(&m_proc.eglQueryDeviceStringEXT, "eglQueryDeviceStringEXT");
        loadGLProc(&m_proc.eglQueryDisplayAttribEXT, "eglQueryDisplayAttribEXT");
    }

    static const auto GLDEBUG = CConfigValue<Config::INTEGER>("debug:gl_debugging");
    if (EGLEXTENSIONS.contains("EGL_KHR_debug") && *GLDEBUG) {
        loadGLProc(&m_proc.eglDebugMessageControlKHR, "eglDebugMessageControlKHR");
        static const EGLAttrib debugAttrs[] = {
            EGL_DEBUG_MSG_CRITICAL_KHR, EGL_TRUE, EGL_DEBUG_MSG_ERROR_KHR, EGL_TRUE, EGL_DEBUG_MSG_WARN_KHR, EGL_TRUE, EGL_DEBUG_MSG_INFO_KHR, EGL_TRUE, EGL_NONE,
        };
        m_proc.eglDebugMessageControlKHR(::eglLog, debugAttrs);
    }

    RASSERT(eglBindAPI(EGL_OPENGL_ES_API) != EGL_FALSE, "Couldn't bind to EGL's opengl ES API. This means your gpu driver f'd up. This is not a hyprland issue.");

    bool success = false;
    if (EGLEXTENSIONS.contains("EXT_platform_device") || !m_proc.eglQueryDevicesEXT || !m_proc.eglQueryDeviceStringEXT) {
        m_eglDevice = eglDeviceFromDRMFD(m_drmFD);

        if (m_eglDevice != EGL_NO_DEVICE_EXT) {
            success = true;
            initEGL(false);
        }
    }

    if (!success) {
        Log::logger->log(Log::WARN, "EGL: EXT_platform_device or EGL_EXT_device_query not supported, using gbm");
        if (EGLEXTENSIONS.contains("KHR_platform_gbm")) {
            success = true;
            m_gbmFD = CFileDescriptor{openRenderNode(m_drmFD)};
            if (!m_gbmFD.isValid())
                RASSERT(false, "Couldn't open a gbm fd");

            m_gbmDevice = gbm_create_device(m_gbmFD.get());
            if (!m_gbmDevice)
                RASSERT(false, "Couldn't open a gbm device");

            initEGL(true);
        }
    }

    RASSERT(success, "EGL does not support KHR_platform_gbm or EXT_platform_device, this is an issue with your gpu driver.");

    auto* const EXTENSIONS = rc<const char*>(glGetString(GL_EXTENSIONS));
    RASSERT(EXTENSIONS, "Couldn't retrieve openGL extensions!");

    m_extensions = EXTENSIONS;

    Log::logger->log(Log::DEBUG, "Creating the Hypr OpenGL Renderer!");
    Log::logger->log(Log::DEBUG, "Using: {}", rc<const char*>(glGetString(GL_VERSION)));
    Log::logger->log(Log::DEBUG, "Vendor: {}", rc<const char*>(glGetString(GL_VENDOR)));
    Log::logger->log(Log::DEBUG, "Renderer: {}", rc<const char*>(glGetString(GL_RENDERER)));
    Log::logger->log(Log::DEBUG, "Supported extensions: ({}) {}", std::ranges::count(m_extensions, ' '), m_extensions);

    m_exts.EXT_read_format_bgra = m_extensions.contains("GL_EXT_read_format_bgra");

    RASSERT(m_extensions.contains("GL_EXT_texture_format_BGRA8888"), "GL_EXT_texture_format_BGRA8888 support by the GPU driver is required");

    if (!m_exts.EXT_read_format_bgra)
        Log::logger->log(Log::WARN, "Your GPU does not support GL_EXT_read_format_bgra, this may cause issues with texture importing");
    if (!m_exts.EXT_image_dma_buf_import || !m_exts.EXT_image_dma_buf_import_modifiers)
        Log::logger->log(Log::WARN, "Your GPU does not support DMABUFs, this will possibly cause issues and will take a hit on the performance.");

    const std::string EGLEXTENSIONS_DISPLAY = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);

    Log::logger->log(Log::DEBUG, "Supported EGL display extensions: ({}) {}", std::ranges::count(EGLEXTENSIONS_DISPLAY, ' '), EGLEXTENSIONS_DISPLAY);

#if defined(__linux__)
    m_exts.EGL_ANDROID_native_fence_sync_ext = EGLEXTENSIONS_DISPLAY.contains("EGL_ANDROID_native_fence_sync");

    if (!m_exts.EGL_ANDROID_native_fence_sync_ext)
        Log::logger->log(Log::WARN, "Your GPU does not support explicit sync via the EGL_ANDROID_native_fence_sync extension.");
#else
    m_exts.EGL_ANDROID_native_fence_sync_ext = false;
    Log::logger->log(Log::WARN, "Forcefully disabling explicit sync: BSD is missing support for proper timeline export");
#endif

#ifdef USE_TRACY_GPU

    loadGLProc(&glQueryCounter, "glQueryCounterEXT");
    loadGLProc(&glGetQueryObjectiv, "glGetQueryObjectivEXT");
    loadGLProc(&glGetQueryObjectui64v, "glGetQueryObjectui64vEXT");

#endif

    TRACY_GPU_CONTEXT;

    initDRMFormats();

    static auto P = Event::bus()->m_events.render.pre.listen([&](PHLMONITOR mon) { preRender(mon); });

    RASSERT(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Couldn't unset current EGL!");

    static auto addLastPressToHistory = [this](const Vector2D& pos, bool killing, bool touch) {
        // shift the new pos and time in
        std::ranges::rotate(m_pressedHistoryPositions, m_pressedHistoryPositions.end() - 1);
        m_pressedHistoryPositions[0] = pos;

        std::ranges::rotate(m_pressedHistoryTimers, m_pressedHistoryTimers.end() - 1);
        m_pressedHistoryTimers[0].reset();

        // shift killed flag in
        m_pressedHistoryKilled <<= 1;
        m_pressedHistoryKilled |= killing ? 1 : 0;
#if POINTER_PRESSED_HISTORY_LENGTH < 32
        m_pressedHistoryKilled &= (1U << POINTER_PRESSED_HISTORY_LENGTH) - 1;
#endif

        // shift touch flag in
        m_pressedHistoryTouched <<= 1;
        m_pressedHistoryTouched |= touch ? 1 : 0;
#if POINTER_PRESSED_HISTORY_LENGTH < 32
        m_pressedHistoryTouched &= (1U << POINTER_PRESSED_HISTORY_LENGTH) - 1;
#endif
    };

    static auto P2 = Event::bus()->m_events.input.mouse.button.listen([](IPointer::SButtonEvent e, Event::SCallbackInfo&) {
        if (e.state != WL_POINTER_BUTTON_STATE_PRESSED)
            return;

        addLastPressToHistory(g_pInputManager->getMouseCoordsInternal(), g_pInputManager->getClickMode() == CLICKMODE_KILL, false);
    });

    static auto P3 = Event::bus()->m_events.input.touch.down.listen([](ITouch::SDownEvent e, Event::SCallbackInfo&) {
        auto PMONITOR = g_pCompositor->getMonitorFromName(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "");

        PMONITOR = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();

        const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);

        addLastPressToHistory(TOUCH_COORDS, g_pInputManager->getClickMode() == CLICKMODE_KILL, true);
    });

    m_finalScreenShader = makeShared<CShader>();
}

CHyprOpenGLImpl::~CHyprOpenGLImpl() {
    if (m_eglDisplay && m_eglContext != EGL_NO_CONTEXT)
        eglDestroyContext(m_eglDisplay, m_eglContext);

    if (m_eglDisplay)
        eglTerminate(m_eglDisplay);

    eglReleaseThread();

    if (m_gbmDevice)
        gbm_device_destroy(m_gbmDevice);
}

std::optional<std::vector<uint64_t>> CHyprOpenGLImpl::getModsForFormat(EGLint format) {
    // TODO: return std::expected when clang supports it

    if (!m_exts.EXT_image_dma_buf_import_modifiers)
        return std::nullopt;

    EGLint len = 0;
    if (!m_proc.eglQueryDmaBufModifiersEXT(m_eglDisplay, format, 0, nullptr, nullptr, &len)) {
        Log::logger->log(Log::ERR, "EGL: Failed to query mods");
        return std::nullopt;
    }

    if (len <= 0)
        return std::vector<uint64_t>{};

    std::vector<uint64_t>   mods;
    std::vector<EGLBoolean> external;

    mods.resize(len);
    external.resize(len);

    if (!m_proc.eglQueryDmaBufModifiersEXT(m_eglDisplay, format, len, mods.data(), external.data(), &len)) {
        const auto err = eglGetError();
        Log::logger->log(Log::ERR, "EGL: Failed to query mods (2) for format 0x{:x}, eglGetError: 0x{:x}", format, err);
        return std::nullopt;
    }

    std::vector<uint64_t> result;
    // reserve number of elements to avoid reallocations
    result.reserve(mods.size());

    bool linearIsExternal = false;
    for (size_t i = 0; i < std::min(mods.size(), external.size()); ++i) {
        if (external[i]) {
            if (mods[i] == DRM_FORMAT_MOD_LINEAR)
                linearIsExternal = true;
            continue;
        }

        result.push_back(mods[i]);
    }

    // if the driver doesn't mark linear as external, add it. It's allowed unless the driver says otherwise. (e.g. nvidia)
    if (!linearIsExternal && std::ranges::find(mods, DRM_FORMAT_MOD_LINEAR) == mods.end())
        result.push_back(DRM_FORMAT_MOD_LINEAR);

    return result;
}

void CHyprOpenGLImpl::initDRMFormats() {
    const auto DISABLE_MODS = Env::envEnabled("HYPRLAND_EGL_NO_MODIFIERS");
    if (DISABLE_MODS)
        Log::logger->log(Log::WARN, "HYPRLAND_EGL_NO_MODIFIERS set, disabling modifiers");

    if (!m_exts.EXT_image_dma_buf_import) {
        Log::logger->log(Log::ERR, "EGL: No dmabuf import, DMABufs will not work.");
        return;
    }

    std::vector<EGLint> formats;

    if (!m_exts.EXT_image_dma_buf_import_modifiers || !m_proc.eglQueryDmaBufFormatsEXT) {
        formats.push_back(DRM_FORMAT_ARGB8888);
        formats.push_back(DRM_FORMAT_XRGB8888);
        Log::logger->log(Log::WARN, "EGL: No mod support");
    } else {
        EGLint len = 0;
        if (!m_proc.eglQueryDmaBufFormatsEXT(m_eglDisplay, 0, nullptr, &len)) {
            const auto err = eglGetError();
            Log::logger->log(Log::ERR, "EGL: Failed to query formats, eglGetError: 0x{:x}", err);
            return;
        }
        formats.resize(len);
        if (!m_proc.eglQueryDmaBufFormatsEXT(m_eglDisplay, len, formats.data(), &len)) {
            const auto err = eglGetError();
            Log::logger->log(Log::ERR, "EGL: Failed to query formats (2), eglGetError: 0x{:x}", err);
            return;
        }
    }

    if (formats.empty()) {
        Log::logger->log(Log::ERR, "EGL: Failed to get formats, DMABufs will not work.");
        return;
    }

    Log::logger->log(Log::DEBUG, "Supported DMA-BUF formats:");

    std::vector<SDRMFormat> dmaFormats;
    // reserve number of elements to avoid reallocations
    dmaFormats.reserve(formats.size());

    for (auto const& fmt : formats) {
        std::vector<uint64_t> mods;
        if (!DISABLE_MODS) {
            auto ret = getModsForFormat(fmt);
            if (!ret.has_value())
                continue;

            mods = *ret;
        } else
            mods = {DRM_FORMAT_MOD_LINEAR};

        m_hasModifiers = m_hasModifiers || !mods.empty();

        // EGL can always do implicit modifiers.
        mods.push_back(DRM_FORMAT_MOD_INVALID);

        dmaFormats.push_back(SDRMFormat{
            .drmFormat = fmt,
            .modifiers = mods,
        });

        std::vector<std::pair<uint64_t, std::string>> modifierData;
        // reserve number of elements to avoid reallocations
        modifierData.reserve(mods.size());

        auto fmtName = drmGetFormatName(fmt);
        Log::logger->log(Log::DEBUG, "EGL: GPU Supports Format {} (0x{:x})", fmtName ? fmtName : "?unknown?", fmt);
        for (auto const& mod : mods) {
            auto modName = drmGetFormatModifierName(mod);
            modifierData.emplace_back(std::make_pair<>(mod, modName ? modName : "?unknown?"));
            free(modName); // NOLINT(cppcoreguidelines-no-malloc)
        }
        free(fmtName); // NOLINT(cppcoreguidelines-no-malloc)

        mods.clear();
        std::ranges::sort(modifierData, [](const auto& a, const auto& b) {
            if (a.first == 0)
                return false;
            if (a.second.contains("DCC"))
                return false;
            return true;
        });

        for (auto const& [m, name] : modifierData) {
            Log::logger->log(Log::DEBUG, "EGL: | with modifier {} (0x{:x})", name, m);
            mods.emplace_back(m);
        }
    }

    Log::logger->log(Log::DEBUG, "EGL: {} formats found in total. Some modifiers may be omitted as they are external-only.", dmaFormats.size());

    if (dmaFormats.empty())
        Log::logger->log(
            Log::WARN, "EGL: WARNING: No dmabuf formats were found, dmabuf will be disabled. This will degrade performance, but is most likely a driver issue or a very old GPU.");

    m_drmFormats = dmaFormats;
}

EGLImageKHR CHyprOpenGLImpl::createEGLImage(const Aquamarine::SDMABUFAttrs& attrs) {
    std::array<EGLint, 50> attribs;
    size_t                 idx = 0;

    attribs[idx++] = EGL_WIDTH;
    attribs[idx++] = attrs.size.x;
    attribs[idx++] = EGL_HEIGHT;
    attribs[idx++] = attrs.size.y;
    attribs[idx++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[idx++] = attrs.format;

    struct {
        EGLint fd;
        EGLint offset;
        EGLint pitch;
        EGLint modlo;
        EGLint modhi;
    } attrNames[4] = {
        {EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT}};

    for (int i = 0; i < attrs.planes; ++i) {
        attribs[idx++] = attrNames[i].fd;
        attribs[idx++] = attrs.fds[i];
        attribs[idx++] = attrNames[i].offset;
        attribs[idx++] = attrs.offsets[i];
        attribs[idx++] = attrNames[i].pitch;
        attribs[idx++] = attrs.strides[i];

        if (m_hasModifiers && attrs.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs[idx++] = attrNames[i].modlo;
            attribs[idx++] = sc<uint32_t>(attrs.modifier & 0xFFFFFFFF);
            attribs[idx++] = attrNames[i].modhi;
            attribs[idx++] = sc<uint32_t>(attrs.modifier >> 32);
        }
    }

    attribs[idx++] = EGL_IMAGE_PRESERVED_KHR;
    attribs[idx++] = EGL_TRUE;
    attribs[idx++] = EGL_NONE;

    RASSERT(idx <= attribs.size(), "createEglImage: attribs array out of bounds.");

    EGLImageKHR image = m_proc.eglCreateImageKHR(m_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
    if (image == EGL_NO_IMAGE_KHR) {
        Log::logger->log(Log::ERR, "EGL: EGLCreateImageKHR failed: {}", eglGetError());
        return EGL_NO_IMAGE_KHR;
    }

    return image;
}

void CHyprOpenGLImpl::beginSimple(PHLMONITOR pMonitor, const CRegion& damage, SP<IRenderbuffer> rb, SP<IFramebuffer> fb) {
    g_pHyprRenderer->m_renderData.pMonitor = pMonitor;

    const GLenum RESETSTATUS = glGetGraphicsResetStatus();
    if (RESETSTATUS != GL_NO_ERROR) {
        std::string errStr = "";
        switch (RESETSTATUS) {
            case GL_GUILTY_CONTEXT_RESET: errStr = "GL_GUILTY_CONTEXT_RESET"; break;
            case GL_INNOCENT_CONTEXT_RESET: errStr = "GL_INNOCENT_CONTEXT_RESET"; break;
            case GL_UNKNOWN_CONTEXT_RESET: errStr = "GL_UNKNOWN_CONTEXT_RESET"; break;
            default: errStr = "UNKNOWN??"; break;
        }
        RASSERT(false, "Aborting, glGetGraphicsResetStatus returned {}. Cannot continue until proper GPU reset handling is implemented.", errStr);
        return;
    }

    TRACY_GPU_ZONE("RenderBeginSimple");

    const auto FBO = rb ? rb->getFB() : fb;

    setViewport(0, 0, pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y);

    if (!m_shadersInitialized)
        initShaders();

    g_pHyprRenderer->m_renderData.transformDamage = true;
    g_pHyprRenderer->m_renderData.damage.set(damage);
    g_pHyprRenderer->m_renderData.finalDamage.set(damage);

    m_fakeFrame = true;

    g_pHyprRenderer->bindFB(FBO);
    m_offloadedFramebuffer = false;

    g_pHyprRenderer->m_renderData.mainFB = g_pHyprRenderer->m_renderData.currentFB;
    g_pHyprRenderer->m_renderData.outFB  = FBO;

    g_pHyprRenderer->pushMonitorTransformEnabled(false);
}

void CHyprOpenGLImpl::makeEGLCurrent() {
    if (!g_pCompositor || !g_pHyprOpenGL)
        return;

    if (eglGetCurrentContext() != g_pHyprOpenGL->m_eglContext)
        eglMakeCurrent(g_pHyprOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, g_pHyprOpenGL->m_eglContext);
}

void CHyprOpenGLImpl::begin(PHLMONITOR pMonitor, const CRegion& damage_, SP<IFramebuffer> fb, std::optional<CRegion> finalDamage) {
    g_pHyprRenderer->m_renderData.pMonitor = pMonitor;

    const GLenum RESETSTATUS = glGetGraphicsResetStatus();
    if (RESETSTATUS != GL_NO_ERROR) {
        std::string errStr = "";
        switch (RESETSTATUS) {
            case GL_GUILTY_CONTEXT_RESET: errStr = "GL_GUILTY_CONTEXT_RESET"; break;
            case GL_INNOCENT_CONTEXT_RESET: errStr = "GL_INNOCENT_CONTEXT_RESET"; break;
            case GL_UNKNOWN_CONTEXT_RESET: errStr = "GL_UNKNOWN_CONTEXT_RESET"; break;
            default: errStr = "UNKNOWN??"; break;
        }
        RASSERT(false, "Aborting, glGetGraphicsResetStatus returned {}. Cannot continue until proper GPU reset handling is implemented.", errStr);
        return;
    }

    TRACY_GPU_ZONE("RenderBegin");

    setViewport(0, 0, pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y);

    if (!m_shadersInitialized)
        initShaders();

    const bool HAS_MIRROR_FB = g_pHyprRenderer->m_renderData.pMonitor->resources()->hasMirrorFB();
    const bool NEEDS_COPY_FB = g_pHyprRenderer->m_renderData.pMonitor->needsACopyFB();

    g_pHyprRenderer->m_renderData.transformDamage = true;
    if (HAS_MIRROR_FB != NEEDS_COPY_FB) {
        // force full damage because the mirror fb will be empty
        g_pHyprRenderer->m_renderData.damage.set({0, 0, INT32_MAX, INT32_MAX});
        g_pHyprRenderer->m_renderData.finalDamage.set(g_pHyprRenderer->m_renderData.damage);
    } else {
        g_pHyprRenderer->m_renderData.damage.set(damage_);
        g_pHyprRenderer->m_renderData.finalDamage.set(finalDamage.value_or(damage_));
    }

    m_fakeFrame = fb;

    if (g_pHyprRenderer->m_reloadScreenShader) {
        g_pHyprRenderer->m_reloadScreenShader = false;
        static auto PSHADER                   = CConfigValue<std::string>("decoration:screen_shader");
        applyScreenShader(*PSHADER);
    }

    g_pHyprRenderer->bindFB(g_pHyprRenderer->m_renderData.pMonitor->resources()->getUnusedWorkBuffer());
    m_offloadedFramebuffer = true;

    g_pHyprRenderer->m_renderData.mainFB = g_pHyprRenderer->m_renderData.currentFB;
    g_pHyprRenderer->m_renderData.outFB  = fb ? fb : dc<CHyprGLRenderer*>(g_pHyprRenderer.get())->m_currentRenderbuffer->getFB();

    if UNLIKELY (g_pHyprRenderer->m_renderData.pMonitor->needsUnmodifiedCopy() && !m_fakeFrame) {
        if (!g_pHyprRenderer->m_renderData.pMonitor->resources()->m_mirrorTex) {
            GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
            glDrawBuffers(2, buffers);
            g_pHyprRenderer->m_renderData.pMonitor->resources()->enableMirror();
        }
        g_pHyprRenderer->m_renderData.mainFB->enableMirror(g_pHyprRenderer->m_renderData.pMonitor->resources()->m_mirrorTex);
    } else {
        if (g_pHyprRenderer->m_renderData.pMonitor->resources()->m_mirrorTex) {
            GLenum buffers[] = {GL_COLOR_ATTACHMENT0};
            glDrawBuffers(1, buffers);
            g_pHyprRenderer->m_renderData.pMonitor->resources()->disableMirror();
        }
        g_pHyprRenderer->m_renderData.mainFB->disableMirror();
    }

    g_pHyprRenderer->pushMonitorTransformEnabled(false);
}

void CHyprOpenGLImpl::end() {
    static auto PZOOMDISABLEAA = CConfigValue<Config::INTEGER>("cursor:zoom_disable_aa");
    auto&       m_renderData   = g_pHyprRenderer->m_renderData;
    const auto  PMONITOR       = m_renderData.pMonitor;
    TRACY_GPU_ZONE("RenderEnd");

    g_pHyprRenderer->m_renderData.currentWindow.reset();
    g_pHyprRenderer->m_renderData.surface.reset();
    g_pHyprRenderer->m_renderData.clipBox = {};

    // end the render, copy the data to the main framebuffer
    if LIKELY (m_offloadedFramebuffer) {
        g_pHyprRenderer->m_renderData.damage = g_pHyprRenderer->m_renderData.finalDamage;
        g_pHyprRenderer->pushMonitorTransformEnabled(true);

        CBox monbox = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

        if LIKELY (g_pHyprRenderer->m_renderMode == RENDER_MODE_NORMAL && g_pHyprRenderer->m_renderData.mouseZoomFactor == 1.0f)
            m_renderData.pMonitor->m_zoomController.m_resetCameraState = true;
        m_renderData.pMonitor->m_zoomController.applyZoomTransform(monbox, m_renderData);

        m_applyFinalShader = !g_pHyprRenderer->m_renderData.blockScreenShader;
        if UNLIKELY (g_pHyprRenderer->m_renderData.mouseZoomFactor != 1.F && g_pHyprRenderer->m_renderData.mouseZoomUseMouse && *PZOOMDISABLEAA)
            g_pHyprRenderer->m_renderData.useNearestNeighbor = true;

        // copy the damaged areas into the mirror buffer
        // we can't use the offloadFB for mirroring / ss, as it contains artifacts from blurring
        if UNLIKELY (g_pHyprRenderer->m_renderData.pMonitor->needsACopyFB() && !m_fakeFrame)
            saveBufferForMirror(monbox);

        const auto TEX = g_pHyprRenderer->m_renderData.currentFB->getTexture();
        g_pHyprRenderer->bindFB(g_pHyprRenderer->m_renderData.outFB);
        blend(false);

        const auto PRIMITIVE_BLOCKED = m_finalScreenShader->program() >= 1 || g_pHyprRenderer->m_crashingInProgress ||
            g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription->value() != SImageDescription{};

        if LIKELY (!PRIMITIVE_BLOCKED || g_pHyprRenderer->m_renderMode != RENDER_MODE_NORMAL)
            renderTexturePrimitive(TEX, monbox);
        else // we need to use renderTexture if we do any CM whatsoever.
            renderTexture(TEX, monbox, {.finalMonitorCM = true});

        blend(true);

        g_pHyprRenderer->m_renderData.useNearestNeighbor = false;
        m_applyFinalShader                               = false;
        g_pHyprRenderer->popMonitorTransformEnabled();
    }

    // reset our data
    g_pHyprRenderer->m_renderData.mouseZoomFactor   = 1.f;
    g_pHyprRenderer->m_renderData.mouseZoomUseMouse = true;
    g_pHyprRenderer->m_renderData.blockScreenShader = false;
    g_pHyprRenderer->m_renderData.currentFB.reset();
    g_pHyprRenderer->m_renderData.mainFB.reset();
    g_pHyprRenderer->m_renderData.outFB.reset();
    g_pHyprRenderer->popMonitorTransformEnabled();

    // invalidate our render FBs to signal to the driver we don't need them anymore
    if (!g_pHyprRenderer->m_renderData.pMonitor->useFP16()) { // FIXME wtf?
        g_pHyprRenderer->m_renderData.pMonitor->resources()->forEachUnusedFB(
            [](const auto& fb) {
                fb->bind();
                GLFB(fb)->invalidate({GL_STENCIL_ATTACHMENT, GL_COLOR_ATTACHMENT0});
            },
            false);
    }

    m_renderData.pMonitor.reset();

    static const auto GLDEBUG = CConfigValue<Config::INTEGER>("debug:gl_debugging");

    if (*GLDEBUG) {
        // check for gl errors
        const GLenum ERR = glGetError();

        if UNLIKELY (ERR == GL_CONTEXT_LOST) /* We don't have infra to recover from this */
            RASSERT(false, "glGetError at Opengl::end() returned GL_CONTEXT_LOST. Cannot continue until proper GPU reset handling is implemented.");
    }
}

static const std::vector<std::string> SHADER_INCLUDES = {
    "defines.h",   "constants.h", "cm_helpers.glsl", "rounding.glsl",    "CM.glsl",    "tonemap.glsl", "gain.glsl",
    "border.glsl", "shadow.glsl", "inner_glow.glsl", "blurprepare.glsl", "blur1.glsl", "blur2.glsl",   "blurFinish.glsl",
};

// order matters, see ePreparedFragmentShader
const std::array<std::string, SH_FRAG_LAST> FRAG_SHADERS = {
    "quad.frag",       "passthru.frag", "rgbamatte.frag",  "ext.frag",     "blur1.frag",  "blur2.frag",  "blurprepare.frag",
    "blurfinish.frag", "shadow.frag",   "inner_glow.frag", "surface.frag", "border.frag", "glitch.frag",
};

bool CHyprOpenGLImpl::initShaders(const std::string& path) {
    auto              shaders = makeShared<SPreparedShaders>();
    static const auto PCM     = CConfigValue<Config::INTEGER>("render:cm_enabled");

    try {
        auto shaderLoader = makeUnique<CShaderLoader>(SHADER_INCLUDES, FRAG_SHADERS, path);

        shaders->TEXVERTSRC    = shaderLoader->process("tex300.vert");
        shaders->TEXVERTSRC320 = shaderLoader->process("tex320.vert");

        m_cmSupported = *PCM;

        g_pShaderLoader = std::move(shaderLoader);

    } catch (const std::exception& e) {
        if (!m_shadersInitialized)
            throw e;

        Log::logger->log(Log::ERR, "Shaders update failed: {}", e.what());
        return false;
    }

    m_shaders            = shaders;
    m_shadersInitialized = true;

    Log::logger->log(Log::DEBUG, "Shaders initialized successfully.");
    return true;
}

void CHyprOpenGLImpl::applyScreenShader(const std::string& path) {

    static auto PDT = CConfigValue<Config::INTEGER>("debug:damage_tracking");

    m_finalScreenShader->destroy();

    if (path.empty() || path == STRVAL_EMPTY)
        return;

    std::string     absPath = absolutePath(path, Config::mgr()->getMainConfigPath());

    std::error_code ec;
    if (!std::filesystem::is_regular_file(absPath, ec)) {
        if (ec)
            g_pHyprError->queueError("Screen shader parser: Failed to check screen shader path: " + ec.message());
        else
            g_pHyprError->queueError("Screen shader parser: Screen shader path is not a regular file");
        return;
    }

    std::ifstream infile(absPath);

    if (!infile.good()) {
        g_pHyprError->queueError("Screen shader parser: Failed to open screen shader");
        return;
    }

    std::string fragmentShader((std::istreambuf_iterator<char>(infile)), (std::istreambuf_iterator<char>()));

    if (!m_finalScreenShader->createProgram(              //
            fragmentShader.starts_with("#version 320 es") // do not break existing custom shaders
                ?
                m_shaders->TEXVERTSRC320 :
                m_shaders->TEXVERTSRC,
            fragmentShader, true)) {
        // Error will have been sent by now by the underlying cause
        return;
    }

    if (m_finalScreenShader->getUniformLocation(SHADER_TIME) != -1)
        m_finalScreenShader->setInitialTime(g_pHyprRenderer->m_globalTimer.getSeconds());

    static auto uniformRequireNoDamage = [this](eShaderUniform uniform, const std::string& name) {
        if (*PDT == 0)
            return;
        if (m_finalScreenShader->getUniformLocation(uniform) == -1)
            return;

        // The screen shader uses the uniform
        // Since the screen shader could change every frame, damage tracking *needs* to be disabled
        g_pHyprError->queueError(std::format("Screen shader: Screen shader uses uniform '{}', which requires debug:damage_tracking to be switched off.\n"
                                             "WARNING:(Disabling damage tracking will *massively* increase GPU utilization!",
                                             name));
    };

    // Allow glitch shader to use time uniform whighout damage tracking
    if (!g_pHyprRenderer->m_crashingInProgress)
        uniformRequireNoDamage(SHADER_TIME, "time");

    uniformRequireNoDamage(SHADER_POINTER, "pointer_position");
    uniformRequireNoDamage(SHADER_POINTER_PRESSED_POSITIONS, "pointer_pressed_positions");
    uniformRequireNoDamage(SHADER_POINTER_PRESSED_TIMES, "pointer_pressed_times");
    uniformRequireNoDamage(SHADER_POINTER_PRESSED_KILLED, "pointer_pressed_killed");
    uniformRequireNoDamage(SHADER_POINTER_PRESSED_TOUCHED, "pointer_pressed_touched");
    uniformRequireNoDamage(SHADER_POINTER_LAST_ACTIVE, "pointer_last_active");
    uniformRequireNoDamage(SHADER_POINTER_HIDDEN, "pointer_hidden");
    uniformRequireNoDamage(SHADER_POINTER_KILLING, "pointer_killing");
    uniformRequireNoDamage(SHADER_POINTER_SHAPE, "pointer_shape");
    uniformRequireNoDamage(SHADER_POINTER_SHAPE_PREVIOUS, "pointer_shape_previous");
}

void CHyprOpenGLImpl::blend(bool enabled) {
    if (enabled) {
        setCapStatus(GL_BLEND, true);
        GLCALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)); // everything is premultiplied
    } else
        setCapStatus(GL_BLEND, false);

    m_blend = enabled;
}

void CHyprOpenGLImpl::scissor(const CBox& originalBox, bool transform) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT(m_renderData.pMonitor, "Tried to scissor without begin()!");

    // only call glScissor if the box has changed
    static CBox m_lastScissorBox = {};

    if (transform) {
        CBox       box = originalBox;
        const auto TR  = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));
        box.transform(TR, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y);

        if (box != m_lastScissorBox) {
            GLCALL(glScissor(box.x, box.y, box.width, box.height));
            m_lastScissorBox = box;
        }

        setCapStatus(GL_SCISSOR_TEST, true);
        return;
    }

    if (originalBox != m_lastScissorBox) {
        GLCALL(glScissor(originalBox.x, originalBox.y, originalBox.width, originalBox.height));
        m_lastScissorBox = originalBox;
    }

    setCapStatus(GL_SCISSOR_TEST, true);
}

void CHyprOpenGLImpl::scissor(const pixman_box32* pBox, bool transform) {
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        setCapStatus(GL_SCISSOR_TEST, false);
        return;
    }

    CBox newBox = {pBox->x1, pBox->y1, pBox->x2 - pBox->x1, pBox->y2 - pBox->y1};

    scissor(newBox, transform);
}

void CHyprOpenGLImpl::scissor(const int x, const int y, const int w, const int h, bool transform) {
    CBox box = {x, y, w, h};
    scissor(box, transform);
}

void CHyprOpenGLImpl::renderRect(const CBox& box, const CHyprColor& col, SRectRenderData data) {
    if (!data.damage)
        data.damage = &g_pHyprRenderer->m_renderData.damage;

    if (data.blur)
        renderRectWithBlurInternal(box, col, data);
    else
        renderRectWithDamageInternal(box, col, data);
}

void CHyprOpenGLImpl::renderRectWithBlurInternal(const CBox& box, const CHyprColor& col, const SRectRenderData& data) {
    if (data.damage->empty())
        return;

    CRegion damage{g_pHyprRenderer->m_renderData.damage};
    damage.intersect(box);

    auto blurredBG = data.xray ? g_pHyprRenderer->m_renderData.pMonitor->resources()->m_blurFB->getTexture() : g_pHyprRenderer->blurMainFramebuffer(data.blurA, &damage);

    CBox MONITORBOX = {0, 0, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.y};
    g_pHyprRenderer->pushMonitorTransformEnabled(true);
    const auto SAVEDRENDERMODIF               = g_pHyprRenderer->m_renderData.renderModif;
    g_pHyprRenderer->m_renderData.renderModif = {}; // fix shit
    renderTexture(blurredBG, MONITORBOX,
                  STextureRenderData{.damage = &damage, .a = data.blurA, .round = data.round, .roundingPower = 2.F, .allowCustomUV = false, .allowDim = false, .noAA = false});
    g_pHyprRenderer->popMonitorTransformEnabled();
    g_pHyprRenderer->m_renderData.renderModif = SAVEDRENDERMODIF;

    renderRectWithDamageInternal(box, col, data);
}

void CHyprOpenGLImpl::renderRectWithDamageInternal(const CBox& box, const CHyprColor& col, const SRectRenderData& data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderRectWithDamage");

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    auto        shader = useShader(getShaderVariant(SH_FRAG_QUAD, (data.round > 0 ? SH_FEAT_ROUNDING : 0) | globalFeatures()));
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());

    // premultiply the color as well as we don't work with straight alpha
    shader->setUniformFloat4(SHADER_COLOR, col.r * col.a, col.g * col.a, col.b * col.a, col.a);

    CBox transformedBox = box;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    // Rounded corners
    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat(SHADER_RADIUS, data.round);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0) {
        CRegion damageClip{g_pHyprRenderer->m_renderData.clipBox.x, g_pHyprRenderer->m_renderData.clipBox.y, g_pHyprRenderer->m_renderData.clipBox.width,
                           g_pHyprRenderer->m_renderData.clipBox.height};
        damageClip.intersect(*data.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        data.damage->forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    scissor(nullptr);
}

void CHyprOpenGLImpl::renderTexture(SP<ITexture> tex, const CBox& box, STextureRenderData data) {
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to render texture without begin()!");

    if (!data.damage) {
        if (g_pHyprRenderer->m_renderData.damage.empty())
            return;

        data.damage = &g_pHyprRenderer->m_renderData.damage;
    }

    if (data.blur)
        renderTextureWithBlurInternal(tex, box, data);
    else
        renderTextureInternal(tex, box, data);

    scissor(nullptr);
}

static std::map<std::pair<uint32_t, uint32_t>, std::array<GLfloat, 9>> primariesConversionCache;

void CHyprOpenGLImpl::passCMUniforms(WP<CShader> shader, const NColorManagement::PImageDescription imageDescription,
                                     const NColorManagement::PImageDescription targetImageDescription, bool modifySDR, float sdrMinLuminance, int sdrMaxLuminance,
                                     const SCMSettings& settings) {
    shader->setUniformInt(SHADER_SOURCE_TF, settings.sourceTF);
    shader->setUniformInt(SHADER_TARGET_TF, settings.targetTF);
    shader->setUniformFloat2(SHADER_SRC_TF_RANGE, settings.srcTFRange.min, settings.srcTFRange.max);
    shader->setUniformFloat2(SHADER_DST_TF_RANGE, settings.dstTFRange.min, settings.dstTFRange.max);
    shader->setUniformFloat(SHADER_SRC_REF_LUMINANCE, settings.srcRefLuminance);
    shader->setUniformFloat(SHADER_DST_REF_LUMINANCE, settings.dstRefLuminance);
    shader->setUniformFloat(SHADER_MAX_LUMINANCE, settings.maxLuminance);
    shader->setUniformFloat(SHADER_DST_MAX_LUMINANCE, settings.dstMaxLuminance);
    shader->setUniformFloat(SHADER_SDR_SATURATION, settings.sdrSaturation);
    shader->setUniformFloat(SHADER_SDR_BRIGHTNESS, settings.sdrBrightnessMultiplier);

    if (!targetImageDescription->value().icc.present) {
        const auto cacheKey = std::make_pair(imageDescription->id(), targetImageDescription->id());
        if (!primariesConversionCache.contains(cacheKey)) {
            const auto&                  mat             = settings.convertMatrix;
            const std::array<GLfloat, 9> glConvertMatrix = {
                mat[0][0], mat[1][0], mat[2][0], //
                mat[0][1], mat[1][1], mat[2][1], //
                mat[0][2], mat[1][2], mat[2][2], //
            };
            primariesConversionCache.insert(std::make_pair(cacheKey, glConvertMatrix));
        }
        shader->setUniformMatrix3fv(SHADER_CONVERT_MATRIX, 1, false, primariesConversionCache[cacheKey]);

        const auto                   mat                  = settings.dstPrimaries2XYZ;
        const std::array<GLfloat, 9> glTargetPrimariesXYZ = {
            mat[0][0], mat[1][0], mat[2][0], //
            mat[0][1], mat[1][1], mat[2][1], //
            mat[0][2], mat[1][2], mat[2][2], //
        };
        shader->setUniformMatrix3fv(SHADER_TARGET_PRIMARIES_XYZ, 1, false, glTargetPrimariesXYZ);
    } else {
        // TODO: this sucks
        GLCALL(glActiveTexture(GL_TEXTURE8));
        targetImageDescription->value().icc.lutTexture->bind();

        shader->setUniformInt(SHADER_LUT_3D, 8);
        shader->setUniformFloat(SHADER_LUT_SIZE, targetImageDescription->value().icc.lutSize);

        GLCALL(glActiveTexture(GL_TEXTURE0));
    }
}

void CHyprOpenGLImpl::passCMUniforms(WP<CShader> shader, const NColorManagement::PImageDescription imageDescription,
                                     const NColorManagement::PImageDescription targetImageDescription, bool modifySDR, float sdrMinLuminance, int sdrMaxLuminance) {
    const auto settings = g_pHyprRenderer->getCMSettings(imageDescription, targetImageDescription,
                                                         g_pHyprRenderer->m_renderData.surface.valid() ? g_pHyprRenderer->m_renderData.surface.lock() : nullptr, modifySDR,
                                                         sdrMinLuminance, sdrMaxLuminance);
    passCMUniforms(shader, imageDescription, targetImageDescription, modifySDR, sdrMinLuminance, sdrMaxLuminance, settings);
}

void CHyprOpenGLImpl::passCMUniforms(WP<CShader> shader, const PImageDescription imageDescription) {
    passCMUniforms(shader, imageDescription, g_pHyprRenderer->workBufferImageDescription(), true, g_pHyprRenderer->m_renderData.pMonitor->m_sdrMinLuminance,
                   g_pHyprRenderer->m_renderData.pMonitor->m_sdrMaxLuminance);
}

WP<CShader> CHyprOpenGLImpl::renderToOutputInternal() {
    static const auto PDT            = CConfigValue<Config::INTEGER>("debug:damage_tracking");
    static const auto PCURSORTIMEOUT = CConfigValue<Config::FLOAT>("cursor:inactive_timeout");

    auto&             m_renderData = g_pHyprRenderer->m_renderData;

    WP<CShader>       shader =
        g_pHyprRenderer->m_crashingInProgress ? getShaderVariant(SH_FRAG_GLITCH) : (m_finalScreenShader->program() ? m_finalScreenShader : getShaderVariant(SH_FRAG_PASSTHRURGBA));

    shader = useShader(shader);

    if (*PDT == 0 || g_pHyprRenderer->m_crashingInProgress)
        shader->setUniformFloat(SHADER_TIME, g_pHyprRenderer->m_globalTimer.getSeconds() - shader->getInitialTime());
    else
        shader->setUniformFloat(SHADER_TIME, 0.f);

    shader->setUniformInt(SHADER_WL_OUTPUT, m_renderData.pMonitor->m_id);
    shader->setUniformFloat2(SHADER_FULL_SIZE, m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y);
    shader->setUniformFloat(SHADER_POINTER_INACTIVE_TIMEOUT, *PCURSORTIMEOUT);
    shader->setUniformInt(SHADER_POINTER_HIDDEN, g_pHyprRenderer->m_cursorHiddenByCondition);
    shader->setUniformInt(SHADER_POINTER_KILLING, g_pInputManager->getClickMode() == CLICKMODE_KILL);
    shader->setUniformInt(SHADER_POINTER_SHAPE, g_pHyprRenderer->m_lastCursorData.shape);
    shader->setUniformInt(SHADER_POINTER_SHAPE_PREVIOUS, g_pHyprRenderer->m_lastCursorData.shapePrevious);
    shader->setUniformFloat(SHADER_POINTER_SIZE, g_pCursorManager->getScaledSize());

    if (*PDT == 0) {
        PHLMONITORREF pMonitor = m_renderData.pMonitor;
        Vector2D      p        = ((g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position) * pMonitor->m_scale);
        p                      = p.transform(Math::wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_pixelSize);
        shader->setUniformFloat2(SHADER_POINTER, p.x / pMonitor->m_pixelSize.x, p.y / pMonitor->m_pixelSize.y);

        std::vector<float> pressedPos = m_pressedHistoryPositions | std::views::transform([&](const Vector2D& vec) {
                                            Vector2D pPressed = ((vec - pMonitor->m_position) * pMonitor->m_scale);
                                            pPressed          = pPressed.transform(Math::wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_pixelSize);
                                            return std::array<float, 2>{pPressed.x / pMonitor->m_pixelSize.x, pPressed.y / pMonitor->m_pixelSize.y};
                                        }) |
            std::views::join | std::ranges::to<std::vector<float>>();

        shader->setUniform2fv(SHADER_POINTER_PRESSED_POSITIONS, pressedPos.size(), pressedPos);

        std::vector<float> pressedTime =
            m_pressedHistoryTimers | std::views::transform([](const CTimer& timer) { return timer.getSeconds(); }) | std::ranges::to<std::vector<float>>();

        shader->setUniform1fv(SHADER_POINTER_PRESSED_TIMES, pressedTime.size(), pressedTime);

        shader->setUniformInt(SHADER_POINTER_PRESSED_KILLED, m_pressedHistoryKilled);
        shader->setUniformInt(SHADER_POINTER_PRESSED_TOUCHED, m_pressedHistoryTouched);

        shader->setUniformFloat(SHADER_POINTER_LAST_ACTIVE, g_pInputManager->m_lastCursorMovement.getSeconds());
        shader->setUniformFloat(SHADER_POINTER_SWITCH_TIME, g_pHyprRenderer->m_lastCursorData.switchedTimer.getSeconds());

    } else {
        shader->setUniformFloat2(SHADER_POINTER, 0.f, 0.f);

        static const std::vector<float> pressedPosDefault(POINTER_PRESSED_HISTORY_LENGTH * 2uz, 0.f);
        static const std::vector<float> pressedTimeDefault(POINTER_PRESSED_HISTORY_LENGTH, 0.f);

        shader->setUniform2fv(SHADER_POINTER_PRESSED_POSITIONS, pressedPosDefault.size(), pressedPosDefault);
        shader->setUniform1fv(SHADER_POINTER_PRESSED_TIMES, pressedTimeDefault.size(), pressedTimeDefault);
        shader->setUniformInt(SHADER_POINTER_PRESSED_KILLED, 0);

        shader->setUniformFloat(SHADER_POINTER_LAST_ACTIVE, 0.f);
        shader->setUniformFloat(SHADER_POINTER_SWITCH_TIME, 0.f);
    }

    if (g_pHyprRenderer->m_crashingInProgress) {
        shader->setUniformFloat(SHADER_DISTORT, g_pHyprRenderer->m_crashingDistort);
        shader->setUniformFloat2(SHADER_FULL_SIZE, m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y);
    }

    return shader;
}

WP<CShader> CHyprOpenGLImpl::renderToFBInternal(SP<ITexture> tex, const STextureRenderData& data, eTextureType texType, const CBox& newBox) {
    static const auto  PPASS     = CConfigValue<Config::INTEGER>("render:cm_fs_passthrough");
    static const auto  PENABLECM = CConfigValue<Config::INTEGER>("render:cm_enabled");
    static auto        PBLEND    = CConfigValue<Config::INTEGER>("render:use_shader_blur_blend");

    auto&              m_renderData = g_pHyprRenderer->m_renderData;

    float              alpha = std::clamp(data.a, 0.f, 1.f);

    WP<CShader>        shader;
    ShaderFeatureFlags shaderFeatures = 0;

    switch (texType) {
        case TEXTURE_RGBA: shaderFeatures |= SH_FEAT_RGBA; break;
        case TEXTURE_RGBX: shaderFeatures &= ~SH_FEAT_RGBA; break;

        // TODO set correct features
        case TEXTURE_EXTERNAL: shader = getShaderVariant(SH_FRAG_EXT, SH_FEAT_ROUNDING | SH_FEAT_DISCARD | SH_FEAT_TINT | globalFeatures()); break; // might be unused
        default: RASSERT(false, "tex->m_iTarget unsupported!");
    }

    if (data.finalMonitorCM || (g_pHyprRenderer->m_renderData.currentWindow && g_pHyprRenderer->m_renderData.currentWindow->m_ruleApplicator->RGBX().valueOrDefault()))
        shaderFeatures &= ~SH_FEAT_RGBA;

    const auto surface           = g_pHyprRenderer->m_renderData.surface;
    const bool isHDRSurface      = surface.valid() && surface->m_colorManagement.valid() ? surface->m_colorManagement->isHDR() : false;
    const bool canPassHDRSurface = isHDRSurface && !surface->m_colorManagement->isWindowsScRGB(); // windows scRGB requires CM shader

    const auto WORK_BUFFER_IMAGE_DESCRIPTION = g_pHyprRenderer->workBufferImageDescription();

    // chosenSdrEotf contains the valid eotf for this display

    const auto SOURCE_IMAGE_DESCRIPTION = [&] {
        if (tex->m_imageDescription)
            return tex->m_imageDescription;

        // if valid CM surface, use that as a source
        if (g_pHyprRenderer->m_renderData.surface.valid() && g_pHyprRenderer->m_renderData.surface->m_colorManagement.valid())
            return CImageDescription::from(g_pHyprRenderer->m_renderData.surface->m_colorManagement->imageDescription());

        if (data.cmBackToSRGB)
            return g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription;

        // otherwise, if we are CM'ing back into source, use chosen, because that's what our work buffer is in
        // the same applies to the final monitor CM
        if (data.finalMonitorCM) // NOLINTNEXTLINE
            return WORK_BUFFER_IMAGE_DESCRIPTION;

        // otherwise, default
        return DEFAULT_IMAGE_DESCRIPTION;
    }();

    const auto TARGET_IMAGE_DESCRIPTION = [&] {
        if (g_pHyprRenderer->m_renderData.currentFB->imageDescription())
            return g_pHyprRenderer->m_renderData.currentFB->imageDescription();

        // if we are CM'ing back, use default sRGB
        if (data.cmBackToSRGB)
            return DEFAULT_IMAGE_DESCRIPTION;

        // for final CM, use the target description
        if (data.finalMonitorCM)
            return g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription;
        // otherwise, use chosen, we're drawing into the work buffer
        // NOLINTNEXTLINE
        return WORK_BUFFER_IMAGE_DESCRIPTION;
    }();

    if (data.blur && *PBLEND && data.blurredBG)
        shaderFeatures |= SH_FEAT_BLUR;

    if (data.discardActive)
        shaderFeatures |= SH_FEAT_DISCARD;

    const bool CANT_CHECK_CM_EQUALITY =
        data.cmBackToSRGB || data.finalMonitorCM || (!g_pHyprRenderer->m_renderData.surface || !g_pHyprRenderer->m_renderData.surface->m_colorManagement);

    const bool skipCM = !*PENABLECM || !m_cmSupported                                                    /* CM unsupported or disabled */
        || g_pHyprRenderer->m_renderData.pMonitor->doesNoShaderCM()                                      /* no shader needed */
        || (SOURCE_IMAGE_DESCRIPTION->id() == TARGET_IMAGE_DESCRIPTION->id() && !CANT_CHECK_CM_EQUALITY) /* Source and target have the same image description */
        || (((*PPASS && canPassHDRSurface) ||
             (*PPASS == 1 && !isHDRSurface && m_renderData.pMonitor->m_cmType != NCMType::CM_HDR && m_renderData.pMonitor->m_cmType != NCMType::CM_HDR_EDID)) &&
            m_renderData.pMonitor->inFullscreenMode()) /* Fullscreen window with pass cm enabled */;

    if (data.allowDim && g_pHyprRenderer->m_renderData.currentWindow &&
        (g_pHyprRenderer->m_renderData.currentWindow->m_notRespondingTint->value() > 0 || g_pHyprRenderer->m_renderData.currentWindow->m_dimPercent->value() > 0))
        shaderFeatures |= SH_FEAT_TINT;

    if (data.round > 0)
        shaderFeatures |= SH_FEAT_ROUNDING;

    if (!skipCM) {
        const auto settings = g_pHyprRenderer->getCMSettings(SOURCE_IMAGE_DESCRIPTION, TARGET_IMAGE_DESCRIPTION,
                                                             g_pHyprRenderer->m_renderData.surface.valid() ? g_pHyprRenderer->m_renderData.surface.lock() : nullptr, true,
                                                             g_pHyprRenderer->m_renderData.pMonitor->m_sdrMinLuminance, g_pHyprRenderer->m_renderData.pMonitor->m_sdrMaxLuminance);

        shaderFeatures |= SH_FEAT_CM;

        if (TARGET_IMAGE_DESCRIPTION->value().icc.present)
            shaderFeatures |= SH_FEAT_ICC;
        else {
            if (settings.needsTonemap)
                shaderFeatures |= SH_FEAT_TONEMAP;

            if (!data.finalMonitorCM && settings.needsSDRmod)
                shaderFeatures |= SH_FEAT_SDR_MOD;
        }

        if (!shader)
            shader = getShaderVariant(SH_FRAG_SURFACE, shaderFeatures | globalFeatures());
        shader = useShader(shader);

        passCMUniforms(shader, SOURCE_IMAGE_DESCRIPTION, TARGET_IMAGE_DESCRIPTION, true, g_pHyprRenderer->m_renderData.pMonitor->m_sdrMinLuminance,
                       g_pHyprRenderer->m_renderData.pMonitor->m_sdrMaxLuminance, settings);
    } else {
        if (!shader)
            shader = getShaderVariant(SH_FRAG_SURFACE, shaderFeatures | globalFeatures());
        shader = useShader(shader);
    }

    shader->setUniformFloat(SHADER_ALPHA, alpha);

    if (shaderFeatures & SH_FEAT_BLUR) {
        shader->setUniformInt(SHADER_BLURRED_BG, 1);
        shader->setUniformFloat2(SHADER_UV_OFFSET, newBox.x / data.blurredBG->m_size.x, newBox.y / data.blurredBG->m_size.y);
        shader->setUniformFloat2(SHADER_UV_SIZE, newBox.width / data.blurredBG->m_size.x, newBox.height / data.blurredBG->m_size.y);

        glActiveTexture(GL_TEXTURE0 + 1);
        data.blurredBG->bind();
    }

    if (data.discardActive) {
        shader->setUniformInt(SHADER_DISCARD_OPAQUE, !!(data.discardMode & DISCARD_OPAQUE));
        shader->setUniformInt(SHADER_DISCARD_ALPHA, !!(data.discardMode & DISCARD_ALPHA));
        shader->setUniformFloat(SHADER_DISCARD_ALPHA_VALUE, data.discardOpacity);
    } else {
        shader->setUniformInt(SHADER_DISCARD_OPAQUE, 0);
        shader->setUniformInt(SHADER_DISCARD_ALPHA, 0);
    }

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);
    // Rounded corners
    shader->setUniformFloat2(SHADER_TOP_LEFT, TOPLEFT.x, TOPLEFT.y);
    shader->setUniformFloat2(SHADER_FULL_SIZE, FULLSIZE.x, FULLSIZE.y);
    shader->setUniformFloat(SHADER_RADIUS, data.round);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);

    if (data.allowDim && g_pHyprRenderer->m_renderData.currentWindow) {
        if (g_pHyprRenderer->m_renderData.currentWindow->m_notRespondingTint->value() > 0) {
            const auto DIM = g_pHyprRenderer->m_renderData.currentWindow->m_notRespondingTint->value();
            shader->setUniformInt(SHADER_APPLY_TINT, 1);
            shader->setUniformFloat3(SHADER_TINT, 1.f - DIM, 1.f - DIM, 1.f - DIM);
        } else if (g_pHyprRenderer->m_renderData.currentWindow->m_dimPercent->value() > 0) {
            shader->setUniformInt(SHADER_APPLY_TINT, 1);
            const auto DIM = g_pHyprRenderer->m_renderData.currentWindow->m_dimPercent->value();
            shader->setUniformFloat3(SHADER_TINT, 1.f - DIM, 1.f - DIM, 1.f - DIM);
        } else
            shader->setUniformInt(SHADER_APPLY_TINT, 0);
    } else
        shader->setUniformInt(SHADER_APPLY_TINT, 0);

    return shader;
}

void CHyprOpenGLImpl::renderTextureInternal(SP<ITexture> tex, const CBox& box, const STextureRenderData& data) {
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT(tex, "Attempted to draw nullptr texture!");
    RASSERT(tex->ok(), "Attempted to draw invalid texture!");

    TRACY_GPU_ZONE("RenderTextureInternalWithDamage");

    if (data.damage->empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    // get the needed transform for this texture
    const auto                  MONITOR_INVERTED = Math::wlTransformToHyprutils(Math::invertTransform(g_pHyprRenderer->m_renderData.pMonitor->m_transform));
    Hyprutils::Math::eTransform TRANSFORM        = tex->m_transform;

    if (g_pHyprRenderer->monitorTransformEnabled())
        TRANSFORM = Math::composeTransform(MONITOR_INVERTED, TRANSFORM);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox, TRANSFORM);

    const bool  renderToOutput = m_applyFinalShader && g_pHyprRenderer->workBufferImageDescription()->id() == g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription->id();

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    tex->setTexParameter(GL_TEXTURE_WRAP_S, data.wrapX);
    tex->setTexParameter(GL_TEXTURE_WRAP_T, data.wrapY);

    if (g_pHyprRenderer->m_renderData.useNearestNeighbor) {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, tex->magFilter);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, tex->minFilter);
    }

    auto shader = renderToOutput ? renderToOutputInternal() : renderToFBInternal(tex, data, tex->m_type, newBox);

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    GLCALL(glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO)));
    GLCALL(glBindBuffer(GL_ARRAY_BUFFER, shader->getUniformLocation(SHADER_SHADER_VBO)));

    // this tells GPU can keep reading the old block for previous draws while the CPU writes to a new one.
    // to avoid stalls if renderTextureInternal is called multiple times on same renderpass
    // at the cost of some temporar vram usage.
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullVerts), nullptr, GL_DYNAMIC_DRAW);

    auto verts = fullVerts;

    if (data.allowCustomUV && data.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {
        const float u0 = data.primarySurfaceUVTopLeft.x;
        const float v0 = data.primarySurfaceUVTopLeft.y;
        const float u1 = data.primarySurfaceUVBottomRight.x;
        const float v1 = data.primarySurfaceUVBottomRight.y;

        verts[0].u = u0;
        verts[0].v = v0;
        verts[1].u = u0;
        verts[1].v = v1;
        verts[2].u = u1;
        verts[2].v = v0;
        verts[3].u = u1;
        verts[3].v = v1;
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts.data());

    if (!g_pHyprRenderer->m_renderData.clipBox.empty() || !data.clipRegion.empty()) {
        CRegion damageClip = g_pHyprRenderer->m_renderData.clipBox;

        if (!data.clipRegion.empty()) {
            if (g_pHyprRenderer->m_renderData.clipBox.empty())
                damageClip = data.clipRegion;
            else
                damageClip.intersect(data.clipRegion);
        }

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        data.damage->forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    GLCALL(glBindVertexArray(0));
    GLCALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    tex->unbind();
}

void CHyprOpenGLImpl::renderTexturePrimitive(SP<ITexture> tex, const CBox& box) {
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->ok()), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTexturePrimitive");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    // get transform
    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    // ensure the final blit uses the desired sampling filter
    // when cursor zoom is active we want nearest-neighbor (no anti-aliasing)
    if (g_pHyprRenderer->m_renderData.useNearestNeighbor) {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, tex->magFilter);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, tex->minFilter);
    }

    auto shader = useShader(getShaderVariant(SH_FRAG_PASSTHRURGBA));
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    g_pHyprRenderer->m_renderData.damage.forEachRect([this](const auto& RECT) {
        scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    });

    scissor(nullptr);
    glBindVertexArray(0);
    tex->unbind();
}

void CHyprOpenGLImpl::renderTextureMatte(SP<ITexture> tex, const CBox& box, SP<IFramebuffer> matte) {
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->ok()), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTextureMatte");

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    // get transform
    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    auto        shader = useShader(getShaderVariant(SH_FRAG_MATTE, globalFeatures()));
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    shader->setUniformInt(SHADER_ALPHA_MATTE, 1);

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    glActiveTexture(GL_TEXTURE0 + 1);
    auto matteTex = matte->getTexture();
    matteTex->bind();

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    g_pHyprRenderer->m_renderData.damage.forEachRect([this](const auto& RECT) {
        scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    });

    scissor(nullptr);
    glBindVertexArray(0);
    tex->unbind();
}

// This probably isn't the fastest
// but it works... well, I guess?
//
// Dual (or more) kawase blur
SP<IFramebuffer> CHyprOpenGLImpl::blurFramebufferWithDamage(float a, CRegion* originalDamage, CGLFramebuffer& source) {
    TRACY_GPU_ZONE("RenderBlurFramebufferWithDamage");
    auto&      m_renderData = g_pHyprRenderer->m_renderData;

    const auto BLENDBEFORE = m_blend;
    blend(false);
    setCapStatus(GL_STENCIL_TEST, false);

    // get transforms for the full monitor
    const auto  TRANSFORM  = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));
    CBox        MONITORBOX = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, TRANSFORM);

    // get the config settings
    static auto PBLURSIZE             = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES           = CConfigValue<Config::INTEGER>("decoration:blur:passes");
    static auto PBLURVIBRANCY         = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy_darkness");

    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    // prep damage
    CRegion damage{*originalDamage};
    damage.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                     m_renderData.pMonitor->m_transformedSize.y);
    damage.expand(std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES));

    // helper
    const auto PMIRRORFB     = g_pHyprRenderer->m_renderData.pMonitor->resources()->getUnusedWorkBuffer();
    const auto PMIRRORSWAPFB = g_pHyprRenderer->m_renderData.pMonitor->resources()->getUnusedWorkBuffer();

    auto       currentRenderToFB = PMIRRORFB;

    // Begin with base color adjustments - global brightness and contrast
    // TODO: make this a part of the first pass maybe to save on a drawcall?
    {
        static auto PBLURCONTRAST   = CConfigValue<Config::FLOAT>("decoration:blur:contrast");
        static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");
        static auto PBLEND          = CConfigValue<Config::INTEGER>("render:use_shader_blur_blend");

        PMIRRORSWAPFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = source.getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        WP<CShader> shader;

        // From FB to sRGB
        const bool skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
        if (!skipCM) {
            shader = useShader(getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM));
            passCMUniforms(shader, g_pHyprRenderer->workBufferImageDescription(), DEFAULT_IMAGE_DESCRIPTION);
            shader->setUniformFloat(SHADER_SDR_SATURATION,
                                    m_renderData.pMonitor->m_sdrSaturation > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrSaturation :
                                        1.0f);
            shader->setUniformFloat(SHADER_SDR_BRIGHTNESS,
                                    m_renderData.pMonitor->m_sdrBrightness > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrBrightness :
                                        1.0f);
        } else
            shader = useShader(getShaderVariant(SH_FRAG_BLURPREPARE));

        const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, *PBLEND ? HYPRUTILS_TRANSFORM_NORMAL : TRANSFORM);
        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_CONTRAST, *PBLURCONTRAST);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!damage.empty()) {
            damage.forEachRect([this](const auto& RECT) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);
        currentRenderToFB = PMIRRORSWAPFB;
    }

    // declare the draw func
    auto drawPass = [&](WP<CShader> shader, ePreparedFragmentShader frag, CRegion* pDamage) {
        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();

        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // prep two shaders
        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_RADIUS, *PBLURSIZE * a); // this makes the blursize change with a
        if (frag == SH_FRAG_BLUR1) {
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x / 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y / 2.f));
            shader->setUniformInt(SHADER_PASSES, BLUR_PASSES);
            shader->setUniformFloat(SHADER_VIBRANCY, *PBLURVIBRANCY);
            shader->setUniformFloat(SHADER_VIBRANCY_DARKNESS, *PBLURVIBRANCYDARKNESS);
        } else
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x * 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y * 2.f));
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!pDamage->empty()) {
            pDamage->forEachRect([this](const auto& RECT) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    };

    // draw the things.
    // first draw is swap -> mirr
    PMIRRORFB->bind();
    PMIRRORSWAPFB->getTexture()->bind();

    // damage region will be scaled, make a temp
    CRegion tempDamage{damage};

    // and draw
    auto shader = useShader(getShaderVariant(SH_FRAG_BLUR1));
    for (auto i = 1; i <= BLUR_PASSES; ++i) {
        tempDamage = damage.copy().scale(1.f / (1 << i));
        drawPass(shader, SH_FRAG_BLUR1, &tempDamage); // down
    }

    shader = useShader(getShaderVariant(SH_FRAG_BLUR2));
    for (auto i = BLUR_PASSES - 1; i >= 0; --i) {
        tempDamage = damage.copy().scale(1.f / (1 << i)); // when upsampling we make the region twice as big
        drawPass(shader, SH_FRAG_BLUR2, &tempDamage);     // up
    }

    // finalize the image
    {
        static auto PBLURNOISE      = CConfigValue<Config::FLOAT>("decoration:blur:noise");
        static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");

        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();

        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // From FB to sRGB
        const bool skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
        if (!skipCM) {
            shader = useShader(getShaderVariant(SH_FRAG_BLURFINISH, SH_FEAT_CM));
            passCMUniforms(shader, DEFAULT_IMAGE_DESCRIPTION, g_pHyprRenderer->workBufferImageDescription());
            shader->setUniformFloat(SHADER_SDR_SATURATION,
                                    m_renderData.pMonitor->m_sdrSaturation > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrSaturation :
                                        1.0f);
            shader->setUniformFloat(SHADER_SDR_BRIGHTNESS,
                                    m_renderData.pMonitor->m_sdrBrightness > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrBrightness :
                                        1.0f);
        } else
            shader = useShader(getShaderVariant(SH_FRAG_BLURFINISH));

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_NOISE, *PBLURNOISE);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);

        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!damage.empty()) {
            damage.forEachRect([this](const auto& RECT) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    }

    // finish
    PMIRRORFB->getTexture()->unbind();

    blend(BLENDBEFORE);

    return currentRenderToFB;
}

void CHyprOpenGLImpl::preRender(PHLMONITOR pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Config::INTEGER>("decoration:blur:xray");
    static auto PBLUR            = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    if (!*PBLURNEWOPTIMIZE || !pMonitor->m_blurFBDirty || !*PBLUR)
        return;

    // ignore if solitary present, nothing to blur
    if (!pMonitor->m_solitaryClient.expired())
        return;

    // check if we need to update the blur fb
    // if there are no windows that would benefit from it,
    // we will ignore that the blur FB is dirty.

    auto windowShouldBeBlurred = [&](PHLWINDOW pWindow) -> bool {
        if (!pWindow)
            return false;

        if (pWindow->m_ruleApplicator->noBlur().valueOrDefault())
            return false;

        if (pWindow->wlSurface()->small() && !pWindow->wlSurface()->m_fillIgnoreSmall)
            return true;

        const auto  PSURFACE = pWindow->wlSurface()->resource();

        const auto  PWORKSPACE = pWindow->m_workspace;
        const float A          = pWindow->m_alpha->value() * pWindow->m_activeInactiveAlpha->value() * PWORKSPACE->m_alpha->value();

        if (A >= 1.f) {
            // if (PSURFACE->opaque)
            //   return false;

            CRegion        inverseOpaque;

            pixman_box32_t surfbox = {0, 0, PSURFACE->m_current.size.x, PSURFACE->m_current.size.y};
            CRegion        opaqueRegion{PSURFACE->m_current.opaque};
            inverseOpaque.set(opaqueRegion).invert(&surfbox).intersect(0, 0, PSURFACE->m_current.size.x, PSURFACE->m_current.size.y);

            if (inverseOpaque.empty())
                return false;
        }

        return true;
    };

    bool hasWindows = false;
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == pMonitor->m_activeWorkspace && !w->isHidden() && w->m_isMapped && (!w->m_isFloating || *PBLURXRAY)) {

            // check if window is valid
            if (!windowShouldBeBlurred(w))
                continue;

            hasWindows = true;
            break;
        }
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        for (auto const& lsl : m->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                if (!ls->m_layerSurface || ls->m_ruleApplicator->xray().valueOrDefault() != 1)
                    continue;

                // if (ls->layerSurface->surface->opaque && ls->alpha->value() >= 1.f)
                //     continue;

                hasWindows = true;
                break;
            }
        }
    }

    if (!hasWindows)
        return;

    g_pHyprRenderer->damageMonitor(pMonitor);
    pMonitor->m_blurFBShouldRender = true;
}

void CHyprOpenGLImpl::renderTextureWithBlurInternal(SP<ITexture> tex, const CBox& box, const STextureRenderData& data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT(m_renderData.pMonitor, "Tried to render texture with blur without begin()!");

    TRACY_GPU_ZONE("RenderTextureWithBlur");

    static auto PBLEND        = CConfigValue<Config::INTEGER>("render:use_shader_blur_blend");
    const auto  NEEDS_STENCIL = data.discardMode != 0 && (!data.blockBlurOptimization || (data.discardMode & DISCARD_ALPHA));
    if (!*PBLEND) {

        if (NEEDS_STENCIL) {
            scissor(nullptr); // allow the entire window and stencil to render
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);

            setCapStatus(GL_STENCIL_TEST, true);

            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            renderTexture(tex, box,
                          STextureRenderData{
                              .damage                      = &g_pHyprRenderer->m_renderData.damage,
                              .a                           = data.a,
                              .round                       = data.round,
                              .roundingPower               = data.roundingPower,
                              .discardActive               = true,
                              .allowCustomUV               = true,
                              .wrapX                       = data.wrapX,
                              .wrapY                       = data.wrapY,
                              .discardMode                 = data.discardMode,
                              .discardOpacity              = data.discardOpacity,
                              .clipRegion                  = data.clipRegion,
                              .currentLS                   = data.currentLS,
                              .primarySurfaceUVTopLeft     = g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft,
                              .primarySurfaceUVBottomRight = g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,
                          }); // discard opaque and alpha < discardOpacity

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }

        // stencil done. Render everything.
        CBox transformedBox = box;
        transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                                 m_renderData.pMonitor->m_transformedSize.y);

        CBox        monitorSpaceBox = {transformedBox.pos().x / m_renderData.pMonitor->m_pixelSize.x * m_renderData.pMonitor->m_transformedSize.x,
                                       transformedBox.pos().y / m_renderData.pMonitor->m_pixelSize.y * m_renderData.pMonitor->m_transformedSize.y,
                                       transformedBox.width / m_renderData.pMonitor->m_pixelSize.x * m_renderData.pMonitor->m_transformedSize.x,
                                       transformedBox.height / m_renderData.pMonitor->m_pixelSize.y * m_renderData.pMonitor->m_transformedSize.y};

        static auto PBLURIGNOREOPACITY = CConfigValue<Config::INTEGER>("decoration:blur:ignore_opacity");

        g_pHyprRenderer->pushMonitorTransformEnabled(true);
        bool renderModif = g_pHyprRenderer->m_renderData.renderModif.enabled;
        if (!data.blockBlurOptimization)
            g_pHyprRenderer->m_renderData.renderModif.enabled = false;

        renderTextureInternal(data.blurredBG, box,
                              STextureRenderData{
                                  .damage         = data.damage,
                                  .a              = (*PBLURIGNOREOPACITY ? data.blurA : data.a * data.blurA) * data.overallA,
                                  .round          = data.round,
                                  .roundingPower  = data.roundingPower,
                                  .discardActive  = false,
                                  .allowCustomUV  = true,
                                  .noAA           = false,
                                  .wrapX          = data.wrapX,
                                  .wrapY          = data.wrapY,
                                  .discardMode    = data.discardMode,
                                  .discardOpacity = data.discardOpacity,
                                  .clipRegion     = data.clipRegion,
                                  .currentLS      = data.currentLS,

                                  .primarySurfaceUVTopLeft     = monitorSpaceBox.pos() / m_renderData.pMonitor->m_transformedSize,
                                  .primarySurfaceUVBottomRight = (monitorSpaceBox.pos() + monitorSpaceBox.size()) / m_renderData.pMonitor->m_transformedSize,
                              });

        g_pHyprRenderer->m_renderData.renderModif.enabled = renderModif;
        g_pHyprRenderer->popMonitorTransformEnabled();

        if (NEEDS_STENCIL)
            setCapStatus(GL_STENCIL_TEST, false);
    }

    // draw window
    renderTextureInternal(tex, box,
                          STextureRenderData{
                              .blur           = *PBLEND,
                              .blurredBG      = data.blurredBG,
                              .damage         = data.damage,
                              .a              = data.a * data.overallA,
                              .round          = data.round,
                              .roundingPower  = data.roundingPower,
                              .discardActive  = *PBLEND && NEEDS_STENCIL,
                              .allowCustomUV  = true,
                              .allowDim       = true,
                              .noAA           = false,
                              .wrapX          = data.wrapX,
                              .wrapY          = data.wrapY,
                              .discardMode    = data.discardMode,
                              .discardOpacity = data.discardOpacity,
                              .clipRegion     = data.clipRegion,
                              .currentLS      = data.currentLS,

                              .primarySurfaceUVTopLeft     = g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft,
                              .primarySurfaceUVBottomRight = g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,
                          });

    GLFB(g_pHyprRenderer->m_renderData.currentFB)->invalidate({GL_STENCIL_ATTACHMENT});
    scissor(nullptr);
}

void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * g_pHyprRenderer->m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float       round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    const auto  BLEND = m_blend;
    blend(true);

    WP<CShader> shader;

    const bool  IS_ICC = g_pHyprRenderer->workBufferImageDescription()->value().icc.present;
    const bool  skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
    if (!skipCM) {
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | SH_FEAT_CM | (IS_ICC ? SH_FEAT_ICC : SH_FEAT_TONEMAP | SH_FEAT_SDR_MOD) | globalFeatures()));
        passCMUniforms(shader, DEFAULT_IMAGE_DESCRIPTION);
    } else
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | globalFeatures()));

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniform4fv(SHADER_GRADIENT, grad.m_colorsOkLabA.size() / 4, grad.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT_LENGTH, grad.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE, sc<int>(grad.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    shader->setUniformFloat(SHADER_ALPHA, data.a);
    shader->setUniformInt(SHADER_GRADIENT2_LENGTH, 0);

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    shader->setUniformFloat(SHADER_RADIUS, round);
    shader->setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    shader->setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    // calculate the border's region, which we need to render over. No need to run the shader on
    // things outside there
    CRegion borderRegion = g_pHyprRenderer->m_renderData.damage.copy().intersect(newBox);
    borderRegion.subtract(box.copy().expand(-scaledBorderSize - round));

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0)
        borderRegion.intersect(g_pHyprRenderer->m_renderData.clipBox);

    if (!borderRegion.empty()) {
        borderRegion.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);

    blend(BLEND);
}

void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder2");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * g_pHyprRenderer->m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float       round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    const auto  BLEND = m_blend;
    blend(true);

    WP<CShader> shader;
    const bool  IS_ICC = g_pHyprRenderer->workBufferImageDescription()->value().icc.present;
    const bool  skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
    if (!skipCM) {
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | SH_FEAT_CM | (IS_ICC ? SH_FEAT_ICC : SH_FEAT_TONEMAP | SH_FEAT_SDR_MOD) | globalFeatures()));
        passCMUniforms(shader, DEFAULT_IMAGE_DESCRIPTION);
    } else
        shader = useShader(getShaderVariant(SH_FRAG_BORDER1, SH_FEAT_ROUNDING | globalFeatures()));

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniform4fv(SHADER_GRADIENT, grad1.m_colorsOkLabA.size() / 4, grad1.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT_LENGTH, grad1.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE, sc<int>(grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    if (!grad2.m_colorsOkLabA.empty())
        shader->setUniform4fv(SHADER_GRADIENT2, grad2.m_colorsOkLabA.size() / 4, grad2.m_colorsOkLabA);
    shader->setUniformInt(SHADER_GRADIENT2_LENGTH, grad2.m_colorsOkLabA.size() / 4);
    shader->setUniformFloat(SHADER_ANGLE2, sc<int>(grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    shader->setUniformFloat(SHADER_ALPHA, data.a);
    shader->setUniformFloat(SHADER_GRADIENT_LERP, lerp);

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    shader->setUniformFloat(SHADER_RADIUS, round);
    shader->setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    shader->setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    // calculate the border's region, which we need to render over. No need to run the shader on
    // things outside there
    CRegion borderRegion = g_pHyprRenderer->m_renderData.damage.copy().intersect(newBox);
    borderRegion.subtract(box.copy().expand(-scaledBorderSize - round));

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0)
        borderRegion.intersect(g_pHyprRenderer->m_renderData.clipBox);

    if (!borderRegion.empty()) {
        borderRegion.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    blend(BLEND);
}

void CHyprOpenGLImpl::renderRoundedShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, float a) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT(m_renderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box.width > 0 && box.height > 0), "Tried to render shadow with width/height < 0!");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    static auto PSHADOWPOWER = CConfigValue<Config::INTEGER>("decoration:shadow:render_power");

    const auto  SHADOWPOWER = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);

    const auto  col = color;

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    blend(true);

    const bool IS_ICC = g_pHyprRenderer->workBufferImageDescription()->value().icc.present;
    const bool skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
    auto       shader = useShader(getShaderVariant(SH_FRAG_SHADOW, skipCM ? 0 : SH_FEAT_CM | (IS_ICC ? SH_FEAT_ICC : SH_FEAT_TONEMAP | SH_FEAT_SDR_MOD) | globalFeatures()));
    if (!skipCM)
        passCMUniforms(shader, DEFAULT_IMAGE_DESCRIPTION);

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformFloat4(SHADER_COLOR, col.r, col.g, col.b, col.a * a);

    const auto TOPLEFT     = Vector2D(range + round, range + round);
    const auto BOTTOMRIGHT = Vector2D(newBox.width - (range + round), newBox.height - (range + round));
    const auto FULLSIZE    = Vector2D(newBox.width, newBox.height);

    // Rounded corners
    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_BOTTOM_RIGHT, sc<float>(BOTTOMRIGHT.x), sc<float>(BOTTOMRIGHT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat(SHADER_RADIUS, range + round);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);
    shader->setUniformFloat(SHADER_RANGE, range);
    shader->setUniformFloat(SHADER_SHADOW_POWER, SHADOWPOWER);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0) {
        CRegion damageClip{g_pHyprRenderer->m_renderData.clipBox.x, g_pHyprRenderer->m_renderData.clipBox.y, g_pHyprRenderer->m_renderData.clipBox.width,
                           g_pHyprRenderer->m_renderData.clipBox.height};
        damageClip.intersect(g_pHyprRenderer->m_renderData.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        g_pHyprRenderer->m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
}

void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, int glowPower, float a) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    RASSERT(m_renderData.pMonitor, "Tried to render inner glow without begin()!");
    RASSERT((box.width > 0 && box.height > 0), "Tried to render inner glow with width/height < 0!");

    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderInnerGlow");

    CBox newBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    const auto  col = color;

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(newBox);

    blend(true);

    const bool IS_ICC = g_pHyprRenderer->workBufferImageDescription()->value().icc.present;
    const bool skipCM = !m_cmSupported || g_pHyprRenderer->workBufferImageDescription()->id() == DEFAULT_IMAGE_DESCRIPTION->id();
    auto       shader = useShader(getShaderVariant(SH_FRAG_INNER_GLOW, skipCM ? 0 : SH_FEAT_CM | (IS_ICC ? SH_FEAT_ICC : SH_FEAT_TONEMAP | SH_FEAT_SDR_MOD)));
    if (!skipCM)
        passCMUniforms(shader, DEFAULT_IMAGE_DESCRIPTION);

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformFloat4(SHADER_COLOR, col.r, col.g, col.b, col.a * a);

    const auto TOPLEFT     = Vector2D(round, round);
    const auto BOTTOMRIGHT = Vector2D(newBox.width - round, newBox.height - round);
    const auto FULLSIZE    = Vector2D(newBox.width, newBox.height);

    shader->setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    shader->setUniformFloat2(SHADER_BOTTOM_RIGHT, sc<float>(BOTTOMRIGHT.x), sc<float>(BOTTOMRIGHT.y));
    shader->setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    shader->setUniformFloat(SHADER_RADIUS, round);
    shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);
    shader->setUniformFloat(SHADER_RANGE, range);
    shader->setUniformFloat(SHADER_SHADOW_POWER, glowPower);

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

    if (g_pHyprRenderer->m_renderData.clipBox.width != 0 && g_pHyprRenderer->m_renderData.clipBox.height != 0) {
        CRegion damageClip{g_pHyprRenderer->m_renderData.clipBox.x, g_pHyprRenderer->m_renderData.clipBox.y, g_pHyprRenderer->m_renderData.clipBox.width,
                           g_pHyprRenderer->m_renderData.clipBox.height};
        damageClip.intersect(g_pHyprRenderer->m_renderData.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        g_pHyprRenderer->m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
}

void CHyprOpenGLImpl::saveBufferForMirror(const CBox& box) {
    const auto TEX = g_pHyprRenderer->m_renderData.pMonitor->resources()->m_mirrorTex ? g_pHyprRenderer->m_renderData.pMonitor->resources()->m_mirrorTex :
                                                                                        g_pHyprRenderer->m_renderData.currentFB->getTexture();
    if (!TEX) {
        Log::logger->log(Log::ERR, "Invalid source texture for mirror");
        return;
    }
    auto guard = g_pHyprRenderer->bindTempFB(g_pHyprRenderer->m_renderData.pMonitor->resources()->mirrorFB());

    blend(false);

    renderTexture(TEX, box,
                  STextureRenderData{
                      .damage        = &g_pHyprRenderer->m_renderData.finalDamage,
                      .a             = 1.F,
                      .round         = 0,
                      .discardActive = false,
                      .allowCustomUV = false,
                      .cmBackToSRGB  = true,
                  });

    blend(true);
}

WP<CShader> CHyprOpenGLImpl::useShader(WP<CShader> prog) {
    if (m_currentProgram == prog->program())
        return prog;

    glUseProgram(prog->program());
    m_currentProgram = prog->program();

    return prog;
}

void CHyprOpenGLImpl::destroyMonitorResources(PHLMONITORREF pMonitor) {
    makeEGLCurrent();

    if (!g_pHyprOpenGL)
        return;

    auto TEXIT = g_pHyprOpenGL->m_monitorBGFBs.find(pMonitor);
    if (TEXIT != g_pHyprOpenGL->m_monitorBGFBs.end()) {
        TEXIT->second.reset();
        g_pHyprOpenGL->m_monitorBGFBs.erase(TEXIT);
    }

    if (pMonitor)
        Log::logger->log(Log::DEBUG, "Monitor {} -> destroyed all render data", pMonitor->m_name);
}

void CHyprOpenGLImpl::renderOffToMain(SP<IFramebuffer> off) {
    CBox monbox = {0, 0, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.y};
    renderTexturePrimitive(off->getTexture(), monbox);
}

void CHyprOpenGLImpl::setViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (m_lastViewport.x == x && m_lastViewport.y == y && m_lastViewport.width == width && m_lastViewport.height == height)
        return;

    glViewport(x, y, width, height);
    m_lastViewport = {.x = x, .y = y, .width = width, .height = height};
}

void CHyprOpenGLImpl::setCapStatus(int cap, bool status) {
    const auto getCapIndex = [cap]() {
        switch (cap) {
            case GL_BLEND: return CAP_STATUS_BLEND;
            case GL_SCISSOR_TEST: return CAP_STATUS_SCISSOR_TEST;
            case GL_STENCIL_TEST: return CAP_STATUS_STENCIL_TEST;
            default: return CAP_STATUS_END;
        }
    };

    auto idx = getCapIndex();

    if (idx == CAP_STATUS_END) {
        if (status)
            GLCALL(glEnable(cap))
        else
            GLCALL(glDisable(cap));

        return;
    }

    if (m_capStatus[idx] == status)
        return;

    if (status) {
        m_capStatus[idx] = status;
        GLCALL(glEnable(cap));
    } else {
        m_capStatus[idx] = status;
        GLCALL(glDisable(cap));
    }
}

std::vector<uint64_t> CHyprOpenGLImpl::getDRMFormatModifiers(DRMFormat drmFormat) {
    SDRMFormat format;

    for (const auto& fmt : m_drmFormats) {
        if (fmt.drmFormat == drmFormat) {
            format = fmt;
            break;
        }
    }

    return format.modifiers;
}

bool CHyprOpenGLImpl::explicitSyncSupported() {
    return m_exts.EGL_ANDROID_native_fence_sync_ext;
}

WP<CShader> CHyprOpenGLImpl::getShaderVariant(ePreparedFragmentShader frag, ShaderFeatureFlags features) {
    auto& variants = m_shaders->fragVariants[frag];
    auto  it       = variants.find(features);

    if (it == variants.end()) {
        auto shader = makeShared<CShader>();

        Log::logger->log(Log::INFO, "compiling feature set {} for {}", features, FRAG_SHADERS[frag]);

        const auto fragSrc = g_pShaderLoader->getVariantSource(frag, features);

        if (!shader->createProgram(m_shaders->TEXVERTSRC, fragSrc, true, true))
            Log::logger->log(Log::ERR, "shader features {} failed for {}", features, FRAG_SHADERS[frag]);

        it = variants.emplace(features, std::move(shader)).first;
        return it->second;
    }

    ASSERT(it->second);
    return it->second;
}

std::vector<SDRMFormat> CHyprOpenGLImpl::getDRMFormats() {
    return m_drmFormats;
}

void SRenderModifData::applyToBox(CBox& box) {
    if (!enabled)
        return;

    for (auto const& [type, val] : modifs) {
        try {
            switch (type) {
                case RMOD_TYPE_SCALE: box.scale(std::any_cast<float>(val)); break;
                case RMOD_TYPE_SCALECENTER: box.scaleFromCenter(std::any_cast<float>(val)); break;
                case RMOD_TYPE_TRANSLATE: box.translate(std::any_cast<Vector2D>(val)); break;
                case RMOD_TYPE_ROTATE: box.rot += std::any_cast<float>(val); break;
                case RMOD_TYPE_ROTATECENTER: {
                    const auto   THETA = std::any_cast<float>(val);
                    const double COS   = std::cos(THETA);
                    const double SIN   = std::sin(THETA);
                    box.rot += THETA;
                    const auto OLDPOS = box.pos();
                    box.x             = OLDPOS.x * COS - OLDPOS.y * SIN;
                    box.y             = OLDPOS.y * COS + OLDPOS.x * SIN;
                }
            }
        } catch (std::bad_any_cast& e) { Log::logger->log(Log::ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::applyToBox!"); }
    }
}

void SRenderModifData::applyToRegion(CRegion& rg) {
    if (!enabled)
        return;

    for (auto const& [type, val] : modifs) {
        try {
            switch (type) {
                case RMOD_TYPE_SCALE: rg.scale(std::any_cast<float>(val)); break;
                case RMOD_TYPE_SCALECENTER: rg.scale(std::any_cast<float>(val)); break;
                case RMOD_TYPE_TRANSLATE: rg.translate(std::any_cast<Vector2D>(val)); break;
                case RMOD_TYPE_ROTATE: /* TODO */
                case RMOD_TYPE_ROTATECENTER: break;
            }
        } catch (std::bad_any_cast& e) { Log::logger->log(Log::ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::applyToRegion!"); }
    }
}

float SRenderModifData::combinedScale() {
    if (!enabled)
        return 1;

    float scale = 1.f;
    for (auto const& [type, val] : modifs) {
        try {
            switch (type) {
                case RMOD_TYPE_SCALE: scale *= std::any_cast<float>(val); break;
                case RMOD_TYPE_SCALECENTER:
                case RMOD_TYPE_TRANSLATE:
                case RMOD_TYPE_ROTATE:
                case RMOD_TYPE_ROTATECENTER: break;
            }
        } catch (std::bad_any_cast& e) { Log::logger->log(Log::ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::combinedScale!"); }
    }
    return scale;
}

UP<CEGLSync> CEGLSync::create() {
    RASSERT(g_pHyprOpenGL->m_exts.EGL_ANDROID_native_fence_sync_ext, "Tried to create an EGL sync when syncs are not supported on the gpu");

    EGLSyncKHR sync = g_pHyprOpenGL->m_proc.eglCreateSyncKHR(g_pHyprOpenGL->m_eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);

    if UNLIKELY (sync == EGL_NO_SYNC_KHR) {
        Log::logger->log(Log::ERR, "eglCreateSyncKHR failed");
        return nullptr;
    }

    // we need to flush otherwise we might not get a valid fd
    glFlush();

    int fd = g_pHyprOpenGL->m_proc.eglDupNativeFenceFDANDROID(g_pHyprOpenGL->m_eglDisplay, sync);
    if UNLIKELY (fd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
        Log::logger->log(Log::ERR, "eglDupNativeFenceFDANDROID failed");
        return nullptr;
    }

    UP<CEGLSync> eglSync(new CEGLSync);
    eglSync->m_fd    = CFileDescriptor(fd);
    eglSync->m_sync  = sync;
    eglSync->m_valid = true;

    return eglSync;
}

CEGLSync::~CEGLSync() {
    if UNLIKELY (m_sync == EGL_NO_SYNC_KHR)
        return;

    if UNLIKELY (g_pHyprOpenGL && g_pHyprOpenGL->m_proc.eglDestroySyncKHR(g_pHyprOpenGL->m_eglDisplay, m_sync) != EGL_TRUE)
        Log::logger->log(Log::ERR, "eglDestroySyncKHR failed");
}

bool CEGLSync::isValid() {
    return m_valid && m_sync != EGL_NO_SYNC_KHR && m_fd.isValid();
}
