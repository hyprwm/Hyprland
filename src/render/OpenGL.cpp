#include <GLES3/gl32.h>
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>
#include <numbers>
#include <random>
#include <pango/pangocairo.h>
#include "OpenGL.hpp"
#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/types/ColorManagement.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "../helpers/MainLoopExecutor.hpp"
#include "debug/HyprNotificationOverlay.hpp"
#include "hyprerror/HyprError.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/PreBlurElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "render/Shader.hpp"
#include "AsyncResourceGatherer.hpp"
#include <string>
#include <xf86drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <filesystem>
#include "./shaders/Shaders.hpp"

using namespace Hyprutils::OS;
using namespace NColorManagement;

const std::vector<const char*> ASSET_PATHS = {
#ifdef DATAROOTDIR
    DATAROOTDIR,
#endif
    "/usr/share",
    "/usr/local/share",
};

static inline void loadGLProc(void* pProc, const char* name) {
    void* proc = rc<void*>(eglGetProcAddress(name));
    if (proc == nullptr) {
        Debug::log(CRIT, "[Tracy GPU Profiling] eglGetProcAddress({}) failed", name);
        abort();
    }
    *sc<void**>(pProc) = proc;
}

static enum eLogLevel eglLogToLevel(EGLint type) {
    switch (type) {
        case EGL_DEBUG_MSG_CRITICAL_KHR: return CRIT;
        case EGL_DEBUG_MSG_ERROR_KHR: return ERR;
        case EGL_DEBUG_MSG_WARN_KHR: return WARN;
        case EGL_DEBUG_MSG_INFO_KHR: return LOG;
        default: return LOG;
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
    Debug::log(eglLogToLevel(type), "[EGL] Command {} errored out with {} (0x{}): {}", command, eglErrorToString(error), error, msg);
}

static int openRenderNode(int drmFd) {
    auto renderName = drmGetRenderDeviceNameFromFd(drmFd);
    if (!renderName) {
        // This can happen on split render/display platforms, fallback to
        // primary node
        renderName = drmGetPrimaryDeviceNameFromFd(drmFd);
        if (!renderName) {
            Debug::log(ERR, "drmGetPrimaryDeviceNameFromFd failed");
            return -1;
        }
        Debug::log(LOG, "DRM dev {} has no render node, falling back to primary", renderName);

        drmVersion* render_version = drmGetVersion(drmFd);
        if (render_version && render_version->name) {
            Debug::log(LOG, "DRM dev versionName", render_version->name);
            if (strcmp(render_version->name, "evdi") == 0) {
                free(renderName);
                renderName = strdup("/dev/dri/card0");
            }
            drmFreeVersion(render_version);
        }
    }

    Debug::log(LOG, "openRenderNode got drm device {}", renderName);

    int renderFD = open(renderName, O_RDWR | O_CLOEXEC);
    if (renderFD < 0)
        Debug::log(ERR, "openRenderNode failed to open drm device {}", renderName);

    free(renderName);
    return renderFD;
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

    if (m_exts.IMG_context_priority) {
        Debug::log(LOG, "EGL: IMG_context_priority supported, requesting high");
        attrs.push_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
        attrs.push_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }

    if (m_exts.EXT_create_context_robustness) {
        Debug::log(LOG, "EGL: EXT_create_context_robustness supported, requesting lose on reset");
        attrs.push_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
        attrs.push_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);
    }

    auto attrsNoVer = attrs;

    attrs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    attrs.push_back(3);
    attrs.push_back(EGL_CONTEXT_MINOR_VERSION);
    attrs.push_back(2);
    attrs.push_back(EGL_NONE);

    m_eglContext = eglCreateContext(m_eglDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrs.data());
    if (m_eglContext == EGL_NO_CONTEXT) {
        Debug::log(WARN, "EGL: Failed to create a context with GLES3.2, retrying 3.0");

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
            Debug::log(ERR, "EGL: Failed to obtain a high priority context");
        else
            Debug::log(LOG, "EGL: Got a high priority context");
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
        Debug::log(ERR, "eglDeviceFromDRMFD: eglQueryDevicesEXT failed");
        return EGL_NO_DEVICE_EXT;
    }

    if (nDevices <= 0) {
        Debug::log(ERR, "eglDeviceFromDRMFD: no devices");
        return EGL_NO_DEVICE_EXT;
    }

    std::vector<EGLDeviceEXT> devices;
    devices.resize(nDevices);

    if (!m_proc.eglQueryDevicesEXT(nDevices, devices.data(), &nDevices)) {
        Debug::log(ERR, "eglDeviceFromDRMFD: eglQueryDevicesEXT failed (2)");
        return EGL_NO_DEVICE_EXT;
    }

    drmDevice* drmDev = nullptr;
    if (int ret = drmGetDevice(drmFD, &drmDev); ret < 0) {
        Debug::log(ERR, "eglDeviceFromDRMFD: drmGetDevice failed");
        return EGL_NO_DEVICE_EXT;
    }

    for (auto const& d : devices) {
        auto devName = m_proc.eglQueryDeviceStringEXT(d, EGL_DRM_DEVICE_FILE_EXT);
        if (!devName)
            continue;

        if (drmDeviceHasName(drmDev, devName)) {
            Debug::log(LOG, "eglDeviceFromDRMFD: Using device {}", devName);
            drmFreeDevice(&drmDev);
            return d;
        }
    }

    drmFreeDevice(&drmDev);
    Debug::log(LOG, "eglDeviceFromDRMFD: No drm devices found");
    return EGL_NO_DEVICE_EXT;
}

CHyprOpenGLImpl::CHyprOpenGLImpl() : m_drmFD(g_pCompositor->m_drmRenderNode.fd >= 0 ? g_pCompositor->m_drmRenderNode.fd : g_pCompositor->m_drm.fd) {
    const std::string EGLEXTENSIONS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    Debug::log(LOG, "Supported EGL global extensions: ({}) {}", std::ranges::count(EGLEXTENSIONS, ' '), EGLEXTENSIONS);

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

    if (EGLEXTENSIONS.contains("EGL_KHR_debug")) {
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
        Debug::log(WARN, "EGL: EXT_platform_device or EGL_EXT_device_query not supported, using gbm");
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

    Debug::log(LOG, "Creating the Hypr OpenGL Renderer!");
    Debug::log(LOG, "Using: {}", rc<const char*>(glGetString(GL_VERSION)));
    Debug::log(LOG, "Vendor: {}", rc<const char*>(glGetString(GL_VENDOR)));
    Debug::log(LOG, "Renderer: {}", rc<const char*>(glGetString(GL_RENDERER)));
    Debug::log(LOG, "Supported extensions: ({}) {}", std::ranges::count(m_extensions, ' '), m_extensions);

    m_exts.EXT_read_format_bgra = m_extensions.contains("GL_EXT_read_format_bgra");

    RASSERT(m_extensions.contains("GL_EXT_texture_format_BGRA8888"), "GL_EXT_texture_format_BGRA8888 support by the GPU driver is required");

    if (!m_exts.EXT_read_format_bgra)
        Debug::log(WARN, "Your GPU does not support GL_EXT_read_format_bgra, this may cause issues with texture importing");
    if (!m_exts.EXT_image_dma_buf_import || !m_exts.EXT_image_dma_buf_import_modifiers)
        Debug::log(WARN, "Your GPU does not support DMABUFs, this will possibly cause issues and will take a hit on the performance.");

    const std::string EGLEXTENSIONS_DISPLAY = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);

    Debug::log(LOG, "Supported EGL display extensions: ({}) {}", std::ranges::count(EGLEXTENSIONS_DISPLAY, ' '), EGLEXTENSIONS_DISPLAY);

#if defined(__linux__)
    m_exts.EGL_ANDROID_native_fence_sync_ext = EGLEXTENSIONS_DISPLAY.contains("EGL_ANDROID_native_fence_sync");

    if (!m_exts.EGL_ANDROID_native_fence_sync_ext)
        Debug::log(WARN, "Your GPU does not support explicit sync via the EGL_ANDROID_native_fence_sync extension.");
#else
    m_exts.EGL_ANDROID_native_fence_sync_ext = false;
    Debug::log(WARN, "Forcefully disabling explicit sync: BSD is missing support for proper timeline export");
#endif

#ifdef USE_TRACY_GPU

    loadGLProc(&glQueryCounter, "glQueryCounterEXT");
    loadGLProc(&glGetQueryObjectiv, "glGetQueryObjectivEXT");
    loadGLProc(&glGetQueryObjectui64v, "glGetQueryObjectui64vEXT");

#endif

    TRACY_GPU_CONTEXT;

    initDRMFormats();

    initAssets();

    static auto P = g_pHookSystem->hookDynamic("preRender", [&](void* self, SCallbackInfo& info, std::any data) { preRender(std::any_cast<PHLMONITOR>(data)); });

    RASSERT(eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Couldn't unset current EGL!");

    m_globalTimer.reset();

    pushMonitorTransformEnabled(false);
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
        Debug::log(ERR, "EGL: Failed to query mods");
        return std::nullopt;
    }

    if (len <= 0)
        return std::vector<uint64_t>{};

    std::vector<uint64_t>   mods;
    std::vector<EGLBoolean> external;

    mods.resize(len);
    external.resize(len);

    m_proc.eglQueryDmaBufModifiersEXT(m_eglDisplay, format, len, mods.data(), external.data(), &len);

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
    if (!linearIsExternal && std::ranges::find(mods, DRM_FORMAT_MOD_LINEAR) == mods.end() && mods.empty())
        mods.push_back(DRM_FORMAT_MOD_LINEAR);

    return result;
}

void CHyprOpenGLImpl::initDRMFormats() {
    const auto DISABLE_MODS = envEnabled("HYPRLAND_EGL_NO_MODIFIERS");
    if (DISABLE_MODS)
        Debug::log(WARN, "HYPRLAND_EGL_NO_MODIFIERS set, disabling modifiers");

    if (!m_exts.EXT_image_dma_buf_import) {
        Debug::log(ERR, "EGL: No dmabuf import, DMABufs will not work.");
        return;
    }

    std::vector<EGLint> formats;

    if (!m_exts.EXT_image_dma_buf_import_modifiers || !m_proc.eglQueryDmaBufFormatsEXT) {
        formats.push_back(DRM_FORMAT_ARGB8888);
        formats.push_back(DRM_FORMAT_XRGB8888);
        Debug::log(WARN, "EGL: No mod support");
    } else {
        EGLint len = 0;
        m_proc.eglQueryDmaBufFormatsEXT(m_eglDisplay, 0, nullptr, &len);
        formats.resize(len);
        m_proc.eglQueryDmaBufFormatsEXT(m_eglDisplay, len, formats.data(), &len);
    }

    if (formats.empty()) {
        Debug::log(ERR, "EGL: Failed to get formats, DMABufs will not work.");
        return;
    }

    Debug::log(LOG, "Supported DMA-BUF formats:");

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
        Debug::log(LOG, "EGL: GPU Supports Format {} (0x{:x})", fmtName ? fmtName : "?unknown?", fmt);
        for (auto const& mod : mods) {
            auto modName = drmGetFormatModifierName(mod);
            modifierData.emplace_back(std::make_pair<>(mod, modName ? modName : "?unknown?"));
            free(modName);
        }
        free(fmtName);

        mods.clear();
        std::ranges::sort(modifierData, [](const auto& a, const auto& b) {
            if (a.first == 0)
                return false;
            if (a.second.contains("DCC"))
                return false;
            return true;
        });

        for (auto const& [m, name] : modifierData) {
            Debug::log(LOG, "EGL: | with modifier {} (0x{:x})", name, m);
            mods.emplace_back(m);
        }
    }

    Debug::log(LOG, "EGL: {} formats found in total. Some modifiers may be omitted as they are external-only.", dmaFormats.size());

    if (dmaFormats.empty())
        Debug::log(WARN,
                   "EGL: WARNING: No dmabuf formats were found, dmabuf will be disabled. This will degrade performance, but is most likely a driver issue or a very old GPU.");

    m_drmFormats = dmaFormats;
}

EGLImageKHR CHyprOpenGLImpl::createEGLImage(const Aquamarine::SDMABUFAttrs& attrs) {
    std::array<uint32_t, 50> attribs;
    size_t                   idx = 0;

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

    EGLImageKHR image = m_proc.eglCreateImageKHR(m_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, rc<int*>(attribs.data()));
    if (image == EGL_NO_IMAGE_KHR) {
        Debug::log(ERR, "EGL: EGLCreateImageKHR failed: {}", eglGetError());
        return EGL_NO_IMAGE_KHR;
    }

    return image;
}

void CHyprOpenGLImpl::logShaderError(const GLuint& shader, bool program, bool silent) {
    GLint maxLength = 0;
    if (program)
        glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    else
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    std::vector<GLchar> errorLog(maxLength);
    if (program)
        glGetProgramInfoLog(shader, maxLength, &maxLength, errorLog.data());
    else
        glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog.data());
    std::string errorStr(errorLog.begin(), errorLog.end());

    const auto  FULLERROR = (program ? "Screen shader parser: Error linking program:" : "Screen shader parser: Error compiling shader: ") + errorStr;

    Debug::log(ERR, "Failed to link shader: {}", FULLERROR);

    if (!silent)
        g_pConfigManager->addParseError(FULLERROR);
}

GLuint CHyprOpenGLImpl::createProgram(const std::string& vert, const std::string& frag, bool dynamic, bool silent) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic, silent);
    if (dynamic) {
        if (vertCompiled == 0)
            return 0;
    } else
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX nullptr! Shader source:\n\n{}", vert);

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic, silent);
    if (dynamic) {
        if (fragCompiled == 0)
            return 0;
    } else
        RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT nullptr! Shader source:\n\n{}", frag);

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(prog, true, silent);
            return 0;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(prog, true);
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    return prog;
}

GLuint CHyprOpenGLImpl::compileShader(const GLuint& type, std::string src, bool dynamic, bool silent) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(shader, false, silent);
            return 0;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(shader, false);
        RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");
    }

    return shader;
}

void CHyprOpenGLImpl::beginSimple(PHLMONITOR pMonitor, const CRegion& damage, SP<CRenderbuffer> rb, CFramebuffer* fb) {
    m_renderData.pMonitor = pMonitor;

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

    m_renderData.projection = Mat3x3::outputProjection(pMonitor->m_pixelSize, HYPRUTILS_TRANSFORM_NORMAL);

    m_renderData.monitorProjection = Mat3x3::identity();
    if (pMonitor->m_transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        const Vector2D tfmd = pMonitor->m_transform % 2 == 1 ? Vector2D{FBO->m_size.y, FBO->m_size.x} : FBO->m_size;
        m_renderData.monitorProjection.translate(FBO->m_size / 2.0).transform(wlTransformToHyprutils(pMonitor->m_transform)).translate(-tfmd / 2.0);
    }

    m_renderData.pCurrentMonData = &m_monitorRenderResources[pMonitor];

    if (!m_shadersInitialized)
        initShaders();

    m_renderData.damage.set(damage);
    m_renderData.finalDamage.set(damage);

    m_fakeFrame = true;

    m_renderData.currentFB = FBO;
    FBO->bind();
    m_offloadedFramebuffer = false;

    m_renderData.mainFB = m_renderData.currentFB;
    m_renderData.outFB  = FBO;

    m_renderData.simplePass = true;

    pushMonitorTransformEnabled(false);
}

void CHyprOpenGLImpl::begin(PHLMONITOR pMonitor, const CRegion& damage_, CFramebuffer* fb, std::optional<CRegion> finalDamage) {
    m_renderData.pMonitor = pMonitor;

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

    m_renderData.projection = Mat3x3::outputProjection(pMonitor->m_pixelSize, HYPRUTILS_TRANSFORM_NORMAL);

    m_renderData.monitorProjection = pMonitor->m_projMatrix;

    if (m_monitorRenderResources.contains(pMonitor) && m_monitorRenderResources.at(pMonitor).offloadFB.m_size != pMonitor->m_pixelSize)
        destroyMonitorResources(pMonitor);

    m_renderData.pCurrentMonData = &m_monitorRenderResources[pMonitor];

    if (!m_shadersInitialized)
        initShaders();

    const auto DRM_FORMAT = fb ? fb->m_drmFormat : pMonitor->m_output->state->state().drmFormat;

    // ensure a framebuffer for the monitor exists
    if (m_renderData.pCurrentMonData->offloadFB.m_size != pMonitor->m_pixelSize || DRM_FORMAT != m_renderData.pCurrentMonData->offloadFB.m_drmFormat) {
        m_renderData.pCurrentMonData->stencilTex->allocate();

        m_renderData.pCurrentMonData->offloadFB.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, DRM_FORMAT);
        m_renderData.pCurrentMonData->mirrorFB.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, DRM_FORMAT);
        m_renderData.pCurrentMonData->mirrorSwapFB.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, DRM_FORMAT);

        m_renderData.pCurrentMonData->offloadFB.addStencil(m_renderData.pCurrentMonData->stencilTex);
        m_renderData.pCurrentMonData->mirrorFB.addStencil(m_renderData.pCurrentMonData->stencilTex);
        m_renderData.pCurrentMonData->mirrorSwapFB.addStencil(m_renderData.pCurrentMonData->stencilTex);
    }

    if (m_renderData.pCurrentMonData->monitorMirrorFB.isAllocated() && m_renderData.pMonitor->m_mirrors.empty())
        m_renderData.pCurrentMonData->monitorMirrorFB.release();

    m_renderData.damage.set(damage_);
    m_renderData.finalDamage.set(finalDamage.value_or(damage_));

    m_fakeFrame = fb;

    if (m_reloadScreenShader) {
        m_reloadScreenShader = false;
        static auto PSHADER  = CConfigValue<std::string>("decoration:screen_shader");
        applyScreenShader(*PSHADER);
    }

    m_renderData.pCurrentMonData->offloadFB.bind();
    m_renderData.currentFB = &m_renderData.pCurrentMonData->offloadFB;
    m_offloadedFramebuffer = true;

    m_renderData.mainFB = m_renderData.currentFB;
    m_renderData.outFB  = fb ? fb : g_pHyprRenderer->getCurrentRBO()->getFB();

    pushMonitorTransformEnabled(false);
}

void CHyprOpenGLImpl::end() {
    static auto PZOOMRIGID = CConfigValue<Hyprlang::INT>("cursor:zoom_rigid");

    TRACY_GPU_ZONE("RenderEnd");

    // end the render, copy the data to the main framebuffer
    if (m_offloadedFramebuffer) {
        m_renderData.damage = m_renderData.finalDamage;
        pushMonitorTransformEnabled(true);

        CBox monbox = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

        if (m_renderData.mouseZoomFactor != 1.f) {
            const auto ZOOMCENTER = m_renderData.mouseZoomUseMouse ?
                (g_pInputManager->getMouseCoordsInternal() - m_renderData.pMonitor->m_position) * m_renderData.pMonitor->m_scale :
                m_renderData.pMonitor->m_transformedSize / 2.f;

            monbox.translate(-ZOOMCENTER).scale(m_renderData.mouseZoomFactor).translate(*PZOOMRIGID ? m_renderData.pMonitor->m_transformedSize / 2.0 : ZOOMCENTER);

            monbox.x = std::min(monbox.x, 0.0);
            monbox.y = std::min(monbox.y, 0.0);
            if (monbox.x + monbox.width < m_renderData.pMonitor->m_transformedSize.x)
                monbox.x = m_renderData.pMonitor->m_transformedSize.x - monbox.width;
            if (monbox.y + monbox.height < m_renderData.pMonitor->m_transformedSize.y)
                monbox.y = m_renderData.pMonitor->m_transformedSize.y - monbox.height;
        }

        m_applyFinalShader = !m_renderData.blockScreenShader;
        if (m_renderData.mouseZoomUseMouse)
            m_renderData.useNearestNeighbor = true;

        // copy the damaged areas into the mirror buffer
        // we can't use the offloadFB for mirroring, as it contains artifacts from blurring
        if (!m_renderData.pMonitor->m_mirrors.empty() && !m_fakeFrame)
            saveBufferForMirror(monbox);

        m_renderData.outFB->bind();
        blend(false);

        if (m_finalScreenShader.program < 1 && !g_pHyprRenderer->m_crashingInProgress)
            renderTexturePrimitive(m_renderData.pCurrentMonData->offloadFB.getTexture(), monbox);
        else
            renderTexture(m_renderData.pCurrentMonData->offloadFB.getTexture(), monbox, {});

        blend(true);

        m_renderData.useNearestNeighbor = false;
        m_applyFinalShader              = false;
        popMonitorTransformEnabled();
    }

    // reset our data
    m_renderData.pMonitor.reset();
    m_renderData.mouseZoomFactor   = 1.f;
    m_renderData.mouseZoomUseMouse = true;
    m_renderData.blockScreenShader = false;
    m_renderData.currentFB         = nullptr;
    m_renderData.mainFB            = nullptr;
    m_renderData.outFB             = nullptr;
    popMonitorTransformEnabled();

    // if we dropped to offMain, release it now.
    // if there is a plugin constantly using it, this might be a bit slow,
    // but I haven't seen a single plugin yet use these, so it's better to drop a bit of vram.
    if (m_renderData.pCurrentMonData->offMainFB.isAllocated())
        m_renderData.pCurrentMonData->offMainFB.release();

    // check for gl errors
    const GLenum ERR = glGetError();

    if (ERR == GL_CONTEXT_LOST) /* We don't have infra to recover from this */
        RASSERT(false, "glGetError at Opengl::end() returned GL_CONTEXT_LOST. Cannot continue until proper GPU reset handling is implemented.");
}

void CHyprOpenGLImpl::setDamage(const CRegion& damage_, std::optional<CRegion> finalDamage) {
    m_renderData.damage.set(damage_);
    m_renderData.finalDamage.set(finalDamage.value_or(damage_));
}

// TODO notify user if bundled shader is newer than ~/.config override
static std::string loadShader(const std::string& filename) {
    const auto home = Hyprutils::Path::getHome();
    if (home.has_value()) {
        const auto src = NFsUtils::readFileAsString(home.value() + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    for (auto& e : ASSET_PATHS) {
        const auto src = NFsUtils::readFileAsString(std::string{e} + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    if (SHADERS.contains(filename))
        return SHADERS.at(filename);
    throw std::runtime_error(std::format("Couldn't load shader {}", filename));
}

static void loadShaderInclude(const std::string& filename, std::map<std::string, std::string>& includes) {
    includes.insert({filename, loadShader(filename)});
}

static void processShaderIncludes(std::string& source, const std::map<std::string, std::string>& includes) {
    for (auto it = includes.begin(); it != includes.end(); ++it) {
        Hyprutils::String::replaceInString(source, "#include \"" + it->first + "\"", it->second);
    }
}

static std::string processShader(const std::string& filename, const std::map<std::string, std::string>& includes) {
    auto source = loadShader(filename);
    processShaderIncludes(source, includes);
    return source;
}

// shader has #include "CM.glsl"
static void getCMShaderUniforms(SShader& shader) {
    shader.uniformLocations[SHADER_SKIP_CM]           = glGetUniformLocation(shader.program, "skipCM");
    shader.uniformLocations[SHADER_SOURCE_TF]         = glGetUniformLocation(shader.program, "sourceTF");
    shader.uniformLocations[SHADER_TARGET_TF]         = glGetUniformLocation(shader.program, "targetTF");
    shader.uniformLocations[SHADER_SRC_TF_RANGE]      = glGetUniformLocation(shader.program, "srcTFRange");
    shader.uniformLocations[SHADER_DST_TF_RANGE]      = glGetUniformLocation(shader.program, "dstTFRange");
    shader.uniformLocations[SHADER_TARGET_PRIMARIES]  = glGetUniformLocation(shader.program, "targetPrimaries");
    shader.uniformLocations[SHADER_MAX_LUMINANCE]     = glGetUniformLocation(shader.program, "maxLuminance");
    shader.uniformLocations[SHADER_DST_MAX_LUMINANCE] = glGetUniformLocation(shader.program, "dstMaxLuminance");
    shader.uniformLocations[SHADER_DST_REF_LUMINANCE] = glGetUniformLocation(shader.program, "dstRefLuminance");
    shader.uniformLocations[SHADER_SDR_SATURATION]    = glGetUniformLocation(shader.program, "sdrSaturation");
    shader.uniformLocations[SHADER_SDR_BRIGHTNESS]    = glGetUniformLocation(shader.program, "sdrBrightnessMultiplier");
    shader.uniformLocations[SHADER_CONVERT_MATRIX]    = glGetUniformLocation(shader.program, "convertMatrix");
}

// shader has #include "rounding.glsl"
static void getRoundingShaderUniforms(SShader& shader) {
    shader.uniformLocations[SHADER_TOP_LEFT]       = glGetUniformLocation(shader.program, "topLeft");
    shader.uniformLocations[SHADER_FULL_SIZE]      = glGetUniformLocation(shader.program, "fullSize");
    shader.uniformLocations[SHADER_RADIUS]         = glGetUniformLocation(shader.program, "radius");
    shader.uniformLocations[SHADER_ROUNDING_POWER] = glGetUniformLocation(shader.program, "roundingPower");
}

bool CHyprOpenGLImpl::initShaders() {
    auto              shaders   = makeShared<SPreparedShaders>();
    const bool        isDynamic = m_shadersInitialized;
    static const auto PCM       = CConfigValue<Hyprlang::INT>("render:cm_enabled");

    try {
        std::map<std::string, std::string> includes;
        loadShaderInclude("rounding.glsl", includes);
        loadShaderInclude("CM.glsl", includes);

        shaders->TEXVERTSRC    = processShader("tex300.vert", includes);
        shaders->TEXVERTSRC320 = processShader("tex320.vert", includes);

        GLuint prog;

        if (!*PCM)
            m_cmSupported = false;
        else {
            const auto TEXFRAGSRCCM = processShader("CM.frag", includes);

            prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCCM, true, true);
            if (m_shadersInitialized && m_cmSupported && prog == 0)
                g_pHyprNotificationOverlay->addNotification("CM shader reload failed, falling back to rgba/rgbx", CHyprColor{}, 15000, ICON_WARNING);

            m_cmSupported = prog > 0;
            if (m_cmSupported) {
                shaders->m_shCM.program = prog;
                getCMShaderUniforms(shaders->m_shCM);
                getRoundingShaderUniforms(shaders->m_shCM);
                shaders->m_shCM.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(prog, "proj");
                shaders->m_shCM.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(prog, "tex");
                shaders->m_shCM.uniformLocations[SHADER_TEX_TYPE]            = glGetUniformLocation(prog, "texType");
                shaders->m_shCM.uniformLocations[SHADER_ALPHA_MATTE]         = glGetUniformLocation(prog, "texMatte");
                shaders->m_shCM.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(prog, "alpha");
                shaders->m_shCM.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(prog, "texcoord");
                shaders->m_shCM.uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = glGetAttribLocation(prog, "texcoordMatte");
                shaders->m_shCM.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(prog, "pos");
                shaders->m_shCM.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(prog, "discardOpaque");
                shaders->m_shCM.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(prog, "discardAlpha");
                shaders->m_shCM.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(prog, "discardAlphaValue");
                shaders->m_shCM.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(prog, "applyTint");
                shaders->m_shCM.uniformLocations[SHADER_TINT]                = glGetUniformLocation(prog, "tint");
                shaders->m_shCM.uniformLocations[SHADER_USE_ALPHA_MATTE]     = glGetUniformLocation(prog, "useAlphaMatte");
                shaders->m_shCM.createVao();
            } else
                Debug::log(ERR,
                           "WARNING: CM Shader failed compiling, color management will not work. It's likely because your GPU is an old piece of garbage, don't file bug reports "
                           "about this!");
        }

        const auto FRAGSHADOW             = processShader("shadow.frag", includes);
        const auto FRAGBORDER1            = processShader("border.frag", includes);
        const auto FRAGBLURPREPARE        = processShader("blurprepare.frag", includes);
        const auto FRAGBLURFINISH         = processShader("blurfinish.frag", includes);
        const auto QUADFRAGSRC            = processShader("quad.frag", includes);
        const auto TEXFRAGSRCRGBA         = processShader("rgba.frag", includes);
        const auto TEXFRAGSRCRGBAPASSTHRU = processShader("passthru.frag", includes);
        const auto TEXFRAGSRCRGBAMATTE    = processShader("rgbamatte.frag", includes);
        const auto FRAGGLITCH             = processShader("glitch.frag", includes);
        const auto TEXFRAGSRCRGBX         = processShader("rgbx.frag", includes);
        const auto TEXFRAGSRCEXT          = processShader("ext.frag", includes);
        const auto FRAGBLUR1              = processShader("blur1.frag", includes);
        const auto FRAGBLUR2              = processShader("blur2.frag", includes);

        prog = createProgram(shaders->TEXVERTSRC, QUADFRAGSRC, isDynamic);
        if (!prog)
            return false;
        shaders->m_shQUAD.program = prog;
        getRoundingShaderUniforms(shaders->m_shQUAD);
        shaders->m_shQUAD.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shQUAD.uniformLocations[SHADER_COLOR]      = glGetUniformLocation(prog, "color");
        shaders->m_shQUAD.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shQUAD.createVao();

        prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCRGBA, isDynamic);
        if (!prog)
            return false;
        shaders->m_shRGBA.program = prog;
        getRoundingShaderUniforms(shaders->m_shRGBA);
        shaders->m_shRGBA.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(prog, "proj");
        shaders->m_shRGBA.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(prog, "tex");
        shaders->m_shRGBA.uniformLocations[SHADER_ALPHA_MATTE]         = glGetUniformLocation(prog, "texMatte");
        shaders->m_shRGBA.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(prog, "alpha");
        shaders->m_shRGBA.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(prog, "texcoord");
        shaders->m_shRGBA.uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = glGetAttribLocation(prog, "texcoordMatte");
        shaders->m_shRGBA.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(prog, "pos");
        shaders->m_shRGBA.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(prog, "discardOpaque");
        shaders->m_shRGBA.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(prog, "discardAlpha");
        shaders->m_shRGBA.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(prog, "discardAlphaValue");
        shaders->m_shRGBA.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(prog, "applyTint");
        shaders->m_shRGBA.uniformLocations[SHADER_TINT]                = glGetUniformLocation(prog, "tint");
        shaders->m_shRGBA.uniformLocations[SHADER_USE_ALPHA_MATTE]     = glGetUniformLocation(prog, "useAlphaMatte");
        shaders->m_shRGBA.createVao();

        prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCRGBAPASSTHRU, isDynamic);
        if (!prog)
            return false;
        shaders->m_shPASSTHRURGBA.program                             = prog;
        shaders->m_shPASSTHRURGBA.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shPASSTHRURGBA.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
        shaders->m_shPASSTHRURGBA.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
        shaders->m_shPASSTHRURGBA.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shPASSTHRURGBA.createVao();

        prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCRGBAMATTE, isDynamic);
        if (!prog)
            return false;
        shaders->m_shMATTE.program                              = prog;
        shaders->m_shMATTE.uniformLocations[SHADER_PROJ]        = glGetUniformLocation(prog, "proj");
        shaders->m_shMATTE.uniformLocations[SHADER_TEX]         = glGetUniformLocation(prog, "tex");
        shaders->m_shMATTE.uniformLocations[SHADER_ALPHA_MATTE] = glGetUniformLocation(prog, "texMatte");
        shaders->m_shMATTE.uniformLocations[SHADER_TEX_ATTRIB]  = glGetAttribLocation(prog, "texcoord");
        shaders->m_shMATTE.uniformLocations[SHADER_POS_ATTRIB]  = glGetAttribLocation(prog, "pos");
        shaders->m_shMATTE.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGGLITCH, isDynamic);
        if (!prog)
            return false;
        shaders->m_shGLITCH.program                             = prog;
        shaders->m_shGLITCH.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shGLITCH.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
        shaders->m_shGLITCH.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
        shaders->m_shGLITCH.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shGLITCH.uniformLocations[SHADER_DISTORT]    = glGetUniformLocation(prog, "distort");
        shaders->m_shGLITCH.uniformLocations[SHADER_TIME]       = glGetUniformLocation(prog, "time");
        shaders->m_shGLITCH.uniformLocations[SHADER_FULL_SIZE]  = glGetUniformLocation(prog, "screenSize");
        shaders->m_shGLITCH.createVao();

        prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCRGBX, isDynamic);
        if (!prog)
            return false;
        shaders->m_shRGBX.program = prog;
        getRoundingShaderUniforms(shaders->m_shRGBX);
        shaders->m_shRGBX.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(prog, "tex");
        shaders->m_shRGBX.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(prog, "proj");
        shaders->m_shRGBX.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(prog, "alpha");
        shaders->m_shRGBX.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(prog, "texcoord");
        shaders->m_shRGBX.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(prog, "pos");
        shaders->m_shRGBX.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(prog, "discardOpaque");
        shaders->m_shRGBX.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(prog, "discardAlpha");
        shaders->m_shRGBX.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(prog, "discardAlphaValue");
        shaders->m_shRGBX.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(prog, "applyTint");
        shaders->m_shRGBX.uniformLocations[SHADER_TINT]                = glGetUniformLocation(prog, "tint");
        shaders->m_shRGBX.createVao();

        prog = createProgram(shaders->TEXVERTSRC, TEXFRAGSRCEXT, isDynamic);
        if (!prog)
            return false;
        shaders->m_shEXT.program = prog;
        getRoundingShaderUniforms(shaders->m_shEXT);
        shaders->m_shEXT.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(prog, "tex");
        shaders->m_shEXT.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(prog, "proj");
        shaders->m_shEXT.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(prog, "alpha");
        shaders->m_shEXT.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(prog, "pos");
        shaders->m_shEXT.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(prog, "texcoord");
        shaders->m_shEXT.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(prog, "discardOpaque");
        shaders->m_shEXT.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(prog, "discardAlpha");
        shaders->m_shEXT.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(prog, "discardAlphaValue");
        shaders->m_shEXT.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(prog, "applyTint");
        shaders->m_shEXT.uniformLocations[SHADER_TINT]                = glGetUniformLocation(prog, "tint");
        shaders->m_shEXT.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGBLUR1, isDynamic);
        if (!prog)
            return false;
        shaders->m_shBLUR1.program                                    = prog;
        shaders->m_shBLUR1.uniformLocations[SHADER_TEX]               = glGetUniformLocation(prog, "tex");
        shaders->m_shBLUR1.uniformLocations[SHADER_ALPHA]             = glGetUniformLocation(prog, "alpha");
        shaders->m_shBLUR1.uniformLocations[SHADER_PROJ]              = glGetUniformLocation(prog, "proj");
        shaders->m_shBLUR1.uniformLocations[SHADER_POS_ATTRIB]        = glGetAttribLocation(prog, "pos");
        shaders->m_shBLUR1.uniformLocations[SHADER_TEX_ATTRIB]        = glGetAttribLocation(prog, "texcoord");
        shaders->m_shBLUR1.uniformLocations[SHADER_RADIUS]            = glGetUniformLocation(prog, "radius");
        shaders->m_shBLUR1.uniformLocations[SHADER_HALFPIXEL]         = glGetUniformLocation(prog, "halfpixel");
        shaders->m_shBLUR1.uniformLocations[SHADER_PASSES]            = glGetUniformLocation(prog, "passes");
        shaders->m_shBLUR1.uniformLocations[SHADER_VIBRANCY]          = glGetUniformLocation(prog, "vibrancy");
        shaders->m_shBLUR1.uniformLocations[SHADER_VIBRANCY_DARKNESS] = glGetUniformLocation(prog, "vibrancy_darkness");
        shaders->m_shBLUR1.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGBLUR2, isDynamic);
        if (!prog)
            return false;
        shaders->m_shBLUR2.program                             = prog;
        shaders->m_shBLUR2.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
        shaders->m_shBLUR2.uniformLocations[SHADER_ALPHA]      = glGetUniformLocation(prog, "alpha");
        shaders->m_shBLUR2.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shBLUR2.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shBLUR2.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
        shaders->m_shBLUR2.uniformLocations[SHADER_RADIUS]     = glGetUniformLocation(prog, "radius");
        shaders->m_shBLUR2.uniformLocations[SHADER_HALFPIXEL]  = glGetUniformLocation(prog, "halfpixel");
        shaders->m_shBLUR2.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGBLURPREPARE, isDynamic);
        if (!prog)
            return false;
        shaders->m_shBLURPREPARE.program = prog;
        getCMShaderUniforms(shaders->m_shBLURPREPARE);

        shaders->m_shBLURPREPARE.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
        shaders->m_shBLURPREPARE.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shBLURPREPARE.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shBLURPREPARE.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
        shaders->m_shBLURPREPARE.uniformLocations[SHADER_CONTRAST]   = glGetUniformLocation(prog, "contrast");
        shaders->m_shBLURPREPARE.uniformLocations[SHADER_BRIGHTNESS] = glGetUniformLocation(prog, "brightness");
        shaders->m_shBLURPREPARE.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGBLURFINISH, isDynamic);
        if (!prog)
            return false;
        shaders->m_shBLURFINISH.program = prog;
        // getCMShaderUniforms(shaders->m_shBLURFINISH);

        shaders->m_shBLURFINISH.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
        shaders->m_shBLURFINISH.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
        shaders->m_shBLURFINISH.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
        shaders->m_shBLURFINISH.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
        shaders->m_shBLURFINISH.uniformLocations[SHADER_BRIGHTNESS] = glGetUniformLocation(prog, "brightness");
        shaders->m_shBLURFINISH.uniformLocations[SHADER_NOISE]      = glGetUniformLocation(prog, "noise");
        shaders->m_shBLURFINISH.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGSHADOW, isDynamic);
        if (!prog)
            return false;

        shaders->m_shSHADOW.program = prog;
        getCMShaderUniforms(shaders->m_shSHADOW);
        getRoundingShaderUniforms(shaders->m_shSHADOW);
        shaders->m_shSHADOW.uniformLocations[SHADER_PROJ]         = glGetUniformLocation(prog, "proj");
        shaders->m_shSHADOW.uniformLocations[SHADER_POS_ATTRIB]   = glGetAttribLocation(prog, "pos");
        shaders->m_shSHADOW.uniformLocations[SHADER_TEX_ATTRIB]   = glGetAttribLocation(prog, "texcoord");
        shaders->m_shSHADOW.uniformLocations[SHADER_BOTTOM_RIGHT] = glGetUniformLocation(prog, "bottomRight");
        shaders->m_shSHADOW.uniformLocations[SHADER_RANGE]        = glGetUniformLocation(prog, "range");
        shaders->m_shSHADOW.uniformLocations[SHADER_SHADOW_POWER] = glGetUniformLocation(prog, "shadowPower");
        shaders->m_shSHADOW.uniformLocations[SHADER_COLOR]        = glGetUniformLocation(prog, "color");
        shaders->m_shSHADOW.createVao();

        prog = createProgram(shaders->TEXVERTSRC, FRAGBORDER1, isDynamic);
        if (!prog)
            return false;

        shaders->m_shBORDER1.program = prog;
        getCMShaderUniforms(shaders->m_shBORDER1);
        getRoundingShaderUniforms(shaders->m_shBORDER1);
        shaders->m_shBORDER1.uniformLocations[SHADER_PROJ]                    = glGetUniformLocation(prog, "proj");
        shaders->m_shBORDER1.uniformLocations[SHADER_THICK]                   = glGetUniformLocation(prog, "thick");
        shaders->m_shBORDER1.uniformLocations[SHADER_POS_ATTRIB]              = glGetAttribLocation(prog, "pos");
        shaders->m_shBORDER1.uniformLocations[SHADER_TEX_ATTRIB]              = glGetAttribLocation(prog, "texcoord");
        shaders->m_shBORDER1.uniformLocations[SHADER_BOTTOM_RIGHT]            = glGetUniformLocation(prog, "bottomRight");
        shaders->m_shBORDER1.uniformLocations[SHADER_FULL_SIZE_UNTRANSFORMED] = glGetUniformLocation(prog, "fullSizeUntransformed");
        shaders->m_shBORDER1.uniformLocations[SHADER_RADIUS_OUTER]            = glGetUniformLocation(prog, "radiusOuter");
        shaders->m_shBORDER1.uniformLocations[SHADER_GRADIENT]                = glGetUniformLocation(prog, "gradient");
        shaders->m_shBORDER1.uniformLocations[SHADER_GRADIENT2]               = glGetUniformLocation(prog, "gradient2");
        shaders->m_shBORDER1.uniformLocations[SHADER_GRADIENT_LENGTH]         = glGetUniformLocation(prog, "gradientLength");
        shaders->m_shBORDER1.uniformLocations[SHADER_GRADIENT2_LENGTH]        = glGetUniformLocation(prog, "gradient2Length");
        shaders->m_shBORDER1.uniformLocations[SHADER_ANGLE]                   = glGetUniformLocation(prog, "angle");
        shaders->m_shBORDER1.uniformLocations[SHADER_ANGLE2]                  = glGetUniformLocation(prog, "angle2");
        shaders->m_shBORDER1.uniformLocations[SHADER_GRADIENT_LERP]           = glGetUniformLocation(prog, "gradientLerp");
        shaders->m_shBORDER1.uniformLocations[SHADER_ALPHA]                   = glGetUniformLocation(prog, "alpha");
        shaders->m_shBORDER1.createVao();

    } catch (const std::exception& e) {
        if (!m_shadersInitialized)
            throw e;

        Debug::log(ERR, "Shaders update failed: {}", e.what());
        return false;
    }

    m_shaders            = shaders;
    m_shadersInitialized = true;

    Debug::log(LOG, "Shaders initialized successfully.");
    return true;
}

void CHyprOpenGLImpl::applyScreenShader(const std::string& path) {

    static auto PDT = CConfigValue<Hyprlang::INT>("debug:damage_tracking");

    m_finalScreenShader.destroy();

    if (path.empty() || path == STRVAL_EMPTY)
        return;

    std::ifstream infile(absolutePath(path, g_pConfigManager->getMainConfigPath()));

    if (!infile.good()) {
        g_pConfigManager->addParseError("Screen shader parser: Screen shader path not found");
        return;
    }

    std::string fragmentShader((std::istreambuf_iterator<char>(infile)), (std::istreambuf_iterator<char>()));

    m_finalScreenShader.program = createProgram(      //
        fragmentShader.starts_with("#version 320 es") // do not break existing custom shaders
            ?
            m_shaders->TEXVERTSRC320 :
            m_shaders->TEXVERTSRC,
        fragmentShader, true);

    if (!m_finalScreenShader.program) {
        // Error will have been sent by now by the underlying cause
        return;
    }

    m_finalScreenShader.uniformLocations[SHADER_POINTER] = glGetUniformLocation(m_finalScreenShader.program, "pointer_position");
    m_finalScreenShader.uniformLocations[SHADER_PROJ]    = glGetUniformLocation(m_finalScreenShader.program, "proj");
    m_finalScreenShader.uniformLocations[SHADER_TEX]     = glGetUniformLocation(m_finalScreenShader.program, "tex");
    m_finalScreenShader.uniformLocations[SHADER_TIME]    = glGetUniformLocation(m_finalScreenShader.program, "time");
    if (m_finalScreenShader.uniformLocations[SHADER_TIME] != -1)
        m_finalScreenShader.initialTime = m_globalTimer.getSeconds();
    m_finalScreenShader.uniformLocations[SHADER_WL_OUTPUT] = glGetUniformLocation(m_finalScreenShader.program, "wl_output");
    m_finalScreenShader.uniformLocations[SHADER_FULL_SIZE] = glGetUniformLocation(m_finalScreenShader.program, "screen_size");
    if (m_finalScreenShader.uniformLocations[SHADER_FULL_SIZE] == -1)
        m_finalScreenShader.uniformLocations[SHADER_FULL_SIZE] = glGetUniformLocation(m_finalScreenShader.program, "screenSize");
    if (m_finalScreenShader.uniformLocations[SHADER_TIME] != -1 && *PDT != 0 && !g_pHyprRenderer->m_crashingInProgress) {
        // The screen shader uses the "time" uniform
        // Since the screen shader could change every frame, damage tracking *needs* to be disabled
        g_pConfigManager->addParseError("Screen shader: Screen shader uses uniform 'time', which requires debug:damage_tracking to be switched off.\n"
                                        "WARNING: Disabling damage tracking will *massively* increase GPU utilization!");
    }
    m_finalScreenShader.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(m_finalScreenShader.program, "texcoord");
    m_finalScreenShader.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(m_finalScreenShader.program, "pos");
    if (m_finalScreenShader.uniformLocations[SHADER_POINTER] != -1 && *PDT != 0 && !g_pHyprRenderer->m_crashingInProgress) {
        // The screen shader uses the "pointer_position" uniform
        // Since the screen shader could change every frame, damage tracking *needs* to be disabled
        g_pConfigManager->addParseError("Screen shader: Screen shader uses uniform 'pointerPosition', which requires debug:damage_tracking to be switched off.\n"
                                        "WARNING: Disabling damage tracking will *massively* increase GPU utilization!");
    }
    m_finalScreenShader.createVao();
}

void CHyprOpenGLImpl::clear(const CHyprColor& color) {
    RASSERT(m_renderData.pMonitor, "Tried to render without begin()!");

    TRACY_GPU_ZONE("RenderClear");

    glClearColor(color.r, color.g, color.b, color.a);

    if (!m_renderData.damage.empty()) {
        m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glClear(GL_COLOR_BUFFER_BIT);
        });
    }

    scissor(nullptr);
}

void CHyprOpenGLImpl::blend(bool enabled) {
    if (enabled) {
        setCapStatus(GL_BLEND, true);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // everything is premultiplied
    } else
        setCapStatus(GL_BLEND, false);

    m_blend = enabled;
}

void CHyprOpenGLImpl::scissor(const CBox& originalBox, bool transform) {
    RASSERT(m_renderData.pMonitor, "Tried to scissor without begin()!");

    // only call glScissor if the box has changed
    static CBox m_lastScissorBox = {};

    if (transform) {
        CBox       box = originalBox;
        const auto TR  = wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform));
        box.transform(TR, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y);

        if (box != m_lastScissorBox) {
            glScissor(box.x, box.y, box.width, box.height);
            m_lastScissorBox = box;
        }

        setCapStatus(GL_SCISSOR_TEST, true);
        return;
    }

    if (originalBox != m_lastScissorBox) {
        glScissor(originalBox.x, originalBox.y, originalBox.width, originalBox.height);
        m_lastScissorBox = originalBox;
    }

    setCapStatus(GL_SCISSOR_TEST, true);
}

void CHyprOpenGLImpl::scissor(const pixman_box32* pBox, bool transform) {
    RASSERT(m_renderData.pMonitor, "Tried to scissor without begin()!");

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
        data.damage = &m_renderData.damage;

    if (data.blur)
        renderRectWithBlurInternal(box, col, data);
    else
        renderRectWithDamageInternal(box, col, data);
}

void CHyprOpenGLImpl::renderRectWithBlurInternal(const CBox& box, const CHyprColor& col, const SRectRenderData& data) {
    if (data.damage->empty())
        return;

    CRegion damage{m_renderData.damage};
    damage.intersect(box);

    CFramebuffer* POUTFB = data.xray ? &m_renderData.pCurrentMonData->blurFB : blurMainFramebufferWithDamage(data.blurA, &damage);

    m_renderData.currentFB->bind();

    // make a stencil for rounded corners to work with blur
    scissor(nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    setCapStatus(GL_STENCIL_TEST, true);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    renderRect(box, CHyprColor(0, 0, 0, 0), SRectRenderData{.round = data.round, .roundingPower = data.roundingPower});
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    scissor(box);
    CBox MONITORBOX = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
    pushMonitorTransformEnabled(true);
    const auto SAVEDRENDERMODIF = m_renderData.renderModif;
    m_renderData.renderModif    = {}; // fix shit
    renderTexture(POUTFB->getTexture(), MONITORBOX,
                  STextureRenderData{.damage = &damage, .a = data.blurA, .round = 0, .roundingPower = 2.0f, .allowCustomUV = false, .allowDim = false, .noAA = false});
    popMonitorTransformEnabled();
    m_renderData.renderModif = SAVEDRENDERMODIF;

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    setCapStatus(GL_STENCIL_TEST, false);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    scissor(nullptr);

    renderRectWithDamageInternal(box, col, data);
}

void CHyprOpenGLImpl::renderRectWithDamageInternal(const CBox& box, const CHyprColor& col, const SRectRenderData& data) {
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderRectWithDamage");

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    Mat3x3 matrix = m_renderData.monitorProjection.projectBox(
        newBox, wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform)), newBox.rot);
    Mat3x3 glMatrix = m_renderData.projection.copy().multiply(matrix);

    useProgram(m_shaders->m_shQUAD.program);
    m_shaders->m_shQUAD.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());

    // premultiply the color as well as we don't work with straight alpha
    m_shaders->m_shQUAD.setUniformFloat4(SHADER_COLOR, col.r * col.a, col.g * col.a, col.b * col.a, col.a);

    CBox transformedBox = box;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    // Rounded corners
    m_shaders->m_shQUAD.setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    m_shaders->m_shQUAD.setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    m_shaders->m_shQUAD.setUniformFloat(SHADER_RADIUS, data.round);
    m_shaders->m_shQUAD.setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);

    glBindVertexArray(m_shaders->m_shQUAD.uniformLocations[SHADER_SHADER_VAO]);

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0) {
        CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
        damageClip.intersect(*data.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        data.damage->forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    scissor(nullptr);
}

void CHyprOpenGLImpl::renderTexture(SP<CTexture> tex, const CBox& box, STextureRenderData data) {
    RASSERT(m_renderData.pMonitor, "Tried to render texture without begin()!");

    if (!data.damage) {
        if (m_renderData.damage.empty())
            return;

        data.damage = &m_renderData.damage;
    }

    if (data.blur)
        renderTextureWithBlurInternal(tex, box, data);
    else
        renderTextureInternal(tex, box, data);

    scissor(nullptr);
}

static std::map<std::pair<uint32_t, uint32_t>, std::array<GLfloat, 9>> primariesConversionCache;

static bool isSDR2HDR(const NColorManagement::SImageDescription& imageDescription, const NColorManagement::SImageDescription& targetImageDescription) {
    // might be too strict
    return imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_SRGB &&
        (targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ||
         targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_HLG);
}

void CHyprOpenGLImpl::passCMUniforms(SShader& shader, const NColorManagement::SImageDescription& imageDescription,
                                     const NColorManagement::SImageDescription& targetImageDescription, bool modifySDR, float sdrMinLuminance, int sdrMaxLuminance) {
    shader.setUniformInt(SHADER_SOURCE_TF, imageDescription.transferFunction);
    shader.setUniformInt(SHADER_TARGET_TF, targetImageDescription.transferFunction);

    const auto                   targetPrimaries = targetImageDescription.primariesNameSet || targetImageDescription.primaries == SPCPRimaries{} ?
                          getPrimaries(targetImageDescription.primariesNamed) :
                          targetImageDescription.primaries;

    const std::array<GLfloat, 8> glTargetPrimaries = {
        targetPrimaries.red.x,  targetPrimaries.red.y,  targetPrimaries.green.x, targetPrimaries.green.y,
        targetPrimaries.blue.x, targetPrimaries.blue.y, targetPrimaries.white.x, targetPrimaries.white.y,
    };
    shader.setUniformMatrix4x2fv(SHADER_TARGET_PRIMARIES, 1, false, glTargetPrimaries);

    const bool needsSDRmod = modifySDR && isSDR2HDR(imageDescription, targetImageDescription);

    shader.setUniformFloat2(SHADER_SRC_TF_RANGE, imageDescription.getTFMinLuminance(needsSDRmod ? sdrMinLuminance : -1),
                            imageDescription.getTFMaxLuminance(needsSDRmod ? sdrMaxLuminance : -1));
    shader.setUniformFloat2(SHADER_DST_TF_RANGE, targetImageDescription.getTFMinLuminance(needsSDRmod ? sdrMinLuminance : -1),
                            targetImageDescription.getTFMaxLuminance(needsSDRmod ? sdrMaxLuminance : -1));

    const float maxLuminance = imageDescription.luminances.max > 0 ? imageDescription.luminances.max : imageDescription.luminances.reference;
    shader.setUniformFloat(SHADER_MAX_LUMINANCE, maxLuminance * targetImageDescription.luminances.reference / imageDescription.luminances.reference);
    shader.setUniformFloat(SHADER_DST_MAX_LUMINANCE, targetImageDescription.luminances.max > 0 ? targetImageDescription.luminances.max : 10000);
    shader.setUniformFloat(SHADER_DST_REF_LUMINANCE, targetImageDescription.luminances.reference);
    shader.setUniformFloat(SHADER_SDR_SATURATION, needsSDRmod && m_renderData.pMonitor->m_sdrSaturation > 0 ? m_renderData.pMonitor->m_sdrSaturation : 1.0f);
    shader.setUniformFloat(SHADER_SDR_BRIGHTNESS, needsSDRmod && m_renderData.pMonitor->m_sdrBrightness > 0 ? m_renderData.pMonitor->m_sdrBrightness : 1.0f);
    const auto cacheKey = std::make_pair(imageDescription.getId(), targetImageDescription.getId());
    if (!primariesConversionCache.contains(cacheKey)) {
        const auto                   mat             = imageDescription.getPrimaries().convertMatrix(targetImageDescription.getPrimaries()).mat();
        const std::array<GLfloat, 9> glConvertMatrix = {
            mat[0][0], mat[1][0], mat[2][0], //
            mat[0][1], mat[1][1], mat[2][1], //
            mat[0][2], mat[1][2], mat[2][2], //
        };
        primariesConversionCache.insert(std::make_pair(cacheKey, glConvertMatrix));
    }
    shader.setUniformMatrix3fv(SHADER_CONVERT_MATRIX, 1, false, primariesConversionCache[cacheKey]);
}

void CHyprOpenGLImpl::passCMUniforms(SShader& shader, const SImageDescription& imageDescription) {
    passCMUniforms(shader, imageDescription, m_renderData.pMonitor->m_imageDescription, true, m_renderData.pMonitor->m_sdrMinLuminance, m_renderData.pMonitor->m_sdrMaxLuminance);
}

void CHyprOpenGLImpl::renderTextureInternal(SP<CTexture> tex, const CBox& box, const STextureRenderData& data) {
    RASSERT(m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->m_texID > 0), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTextureInternalWithDamage");

    float alpha = std::clamp(data.a, 0.f, 1.f);

    if (data.damage->empty())
        return;

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    static const auto PDT       = CConfigValue<Hyprlang::INT>("debug:damage_tracking");
    static const auto PPASS     = CConfigValue<Hyprlang::INT>("render:cm_fs_passthrough");
    static const auto PENABLECM = CConfigValue<Hyprlang::INT>("render:cm_enabled");

    // get the needed transform for this texture
    const bool TRANSFORMS_MATCH = wlTransformToHyprutils(m_renderData.pMonitor->m_transform) == tex->m_transform; // FIXME: combine them properly!!!
    eTransform TRANSFORM        = HYPRUTILS_TRANSFORM_NORMAL;
    if (m_monitorTransformEnabled || TRANSFORMS_MATCH)
        TRANSFORM = wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform));

    Mat3x3     matrix   = m_renderData.monitorProjection.projectBox(newBox, TRANSFORM, newBox.rot);
    Mat3x3     glMatrix = m_renderData.projection.copy().multiply(matrix);

    SShader*   shader = nullptr;

    bool       usingFinalShader = false;

    const bool CRASHING = m_applyFinalShader && g_pHyprRenderer->m_crashingInProgress;

    auto       texType = tex->m_type;

    if (CRASHING) {
        shader           = &m_shaders->m_shGLITCH;
        usingFinalShader = true;
    } else if (m_applyFinalShader && m_finalScreenShader.program) {
        shader           = &m_finalScreenShader;
        usingFinalShader = true;
    } else {
        if (m_applyFinalShader) {
            shader           = &m_shaders->m_shPASSTHRURGBA;
            usingFinalShader = true;
        } else {
            switch (tex->m_type) {
                case TEXTURE_RGBA: shader = &m_shaders->m_shRGBA; break;
                case TEXTURE_RGBX: shader = &m_shaders->m_shRGBX; break;

                case TEXTURE_EXTERNAL: shader = &m_shaders->m_shEXT; break; // might be unused
                default: RASSERT(false, "tex->m_iTarget unsupported!");
            }
        }
    }

    if (m_renderData.currentWindow && m_renderData.currentWindow->m_windowData.RGBX.valueOrDefault()) {
        shader  = &m_shaders->m_shRGBX;
        texType = TEXTURE_RGBX;
    }

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    tex->setTexParameter(GL_TEXTURE_WRAP_S, data.wrapX);
    tex->setTexParameter(GL_TEXTURE_WRAP_T, data.wrapY);

    if (m_renderData.useNearestNeighbor) {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
        tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    const bool isHDRSurface      = m_renderData.surface.valid() && m_renderData.surface->m_colorManagement.valid() ? m_renderData.surface->m_colorManagement->isHDR() : false;
    const bool canPassHDRSurface = isHDRSurface && !m_renderData.surface->m_colorManagement->isWindowsScRGB(); // windows scRGB requires CM shader

    auto       imageDescription = m_renderData.surface.valid() && m_renderData.surface->m_colorManagement.valid() ?
              m_renderData.surface->m_colorManagement->imageDescription() :
              (data.cmBackToSRGB ? data.cmBackToSRGBSource->m_imageDescription : SImageDescription{});

    const bool skipCM = !*PENABLECM || !m_cmSupported                                            /* CM unsupported or disabled */
        || m_renderData.pMonitor->doesNoShaderCM()                                               /* no shader needed */
        || (imageDescription == m_renderData.pMonitor->m_imageDescription && !data.cmBackToSRGB) /* Source and target have the same image description */
        || (((*PPASS && canPassHDRSurface) || (*PPASS == 1 && !isHDRSurface)) && m_renderData.pMonitor->inFullscreenMode()) /* Fullscreen window with pass cm enabled */;

    if (!skipCM && !usingFinalShader && (texType == TEXTURE_RGBA || texType == TEXTURE_RGBX))
        shader = &m_shaders->m_shCM;

    useProgram(shader->program);

    if (shader == &m_shaders->m_shCM) {
        shader->setUniformInt(SHADER_TEX_TYPE, texType);
        if (data.cmBackToSRGB) {
            // revert luma changes to avoid black screenshots.
            // this will likely not be 1:1, and might cause screenshots to be too bright, but it's better than pitch black.
            imageDescription.luminances = {};
            passCMUniforms(*shader, imageDescription, NColorManagement::SImageDescription{}, true, -1, -1);
        } else
            passCMUniforms(*shader, imageDescription);
    }

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);

    if ((usingFinalShader && *PDT == 0) || CRASHING)
        shader->setUniformFloat(SHADER_TIME, m_globalTimer.getSeconds() - shader->initialTime);
    else if (usingFinalShader)
        shader->setUniformFloat(SHADER_TIME, 0.f);

    if (usingFinalShader) {
        shader->setUniformInt(SHADER_WL_OUTPUT, m_renderData.pMonitor->m_id);
        shader->setUniformFloat2(SHADER_FULL_SIZE, m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y);
    }

    if (usingFinalShader && *PDT == 0) {
        PHLMONITORREF pMonitor = m_renderData.pMonitor;
        Vector2D      p        = ((g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position) * pMonitor->m_scale);
        p                      = p.transform(wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_pixelSize);
        shader->setUniformFloat2(SHADER_POINTER, p.x / pMonitor->m_pixelSize.x, p.y / pMonitor->m_pixelSize.y);
    } else if (usingFinalShader)
        shader->setUniformFloat2(SHADER_POINTER, 0.f, 0.f);

    if (CRASHING) {
        shader->setUniformFloat(SHADER_DISTORT, g_pHyprRenderer->m_crashingDistort);
        shader->setUniformFloat2(SHADER_FULL_SIZE, m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y);
    }

    if (!usingFinalShader) {
        shader->setUniformFloat(SHADER_ALPHA, alpha);

        if (data.discardActive) {
            shader->setUniformInt(SHADER_DISCARD_OPAQUE, !!(m_renderData.discardMode & DISCARD_OPAQUE));
            shader->setUniformInt(SHADER_DISCARD_ALPHA, !!(m_renderData.discardMode & DISCARD_ALPHA));
            shader->setUniformFloat(SHADER_DISCARD_ALPHA_VALUE, m_renderData.discardOpacity);
        } else {
            shader->setUniformInt(SHADER_DISCARD_OPAQUE, 0);
            shader->setUniformInt(SHADER_DISCARD_ALPHA, 0);
        }
    }

    CBox transformedBox = newBox;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    if (!usingFinalShader) {
        // Rounded corners
        shader->setUniformFloat2(SHADER_TOP_LEFT, TOPLEFT.x, TOPLEFT.y);
        shader->setUniformFloat2(SHADER_FULL_SIZE, FULLSIZE.x, FULLSIZE.y);
        shader->setUniformFloat(SHADER_RADIUS, data.round);
        shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);

        if (data.allowDim && m_renderData.currentWindow) {
            if (m_renderData.currentWindow->m_notRespondingTint->value() > 0) {
                const auto DIM = m_renderData.currentWindow->m_notRespondingTint->value();
                shader->setUniformInt(SHADER_APPLY_TINT, 1);
                shader->setUniformFloat3(SHADER_TINT, 1.f - DIM, 1.f - DIM, 1.f - DIM);
            } else if (m_renderData.currentWindow->m_dimPercent->value() > 0) {
                shader->setUniformInt(SHADER_APPLY_TINT, 1);
                const auto DIM = m_renderData.currentWindow->m_dimPercent->value();
                shader->setUniformFloat3(SHADER_TINT, 1.f - DIM, 1.f - DIM, 1.f - DIM);
            } else
                shader->setUniformInt(SHADER_APPLY_TINT, 0);
        } else
            shader->setUniformInt(SHADER_APPLY_TINT, 0);
    }

    glBindVertexArray(shader->uniformLocations[SHADER_SHADER_VAO]);
    if (data.allowCustomUV && m_renderData.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {
        const float customUVs[] = {
            m_renderData.primarySurfaceUVBottomRight.x, m_renderData.primarySurfaceUVTopLeft.y,     m_renderData.primarySurfaceUVTopLeft.x,
            m_renderData.primarySurfaceUVTopLeft.y,     m_renderData.primarySurfaceUVBottomRight.x, m_renderData.primarySurfaceUVBottomRight.y,
            m_renderData.primarySurfaceUVTopLeft.x,     m_renderData.primarySurfaceUVBottomRight.y,
        };

        glBindBuffer(GL_ARRAY_BUFFER, shader->uniformLocations[SHADER_SHADER_VBO_UV]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(customUVs), customUVs);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, shader->uniformLocations[SHADER_SHADER_VBO_UV]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fullVerts), fullVerts);
    }

    if (!m_renderData.clipBox.empty() || !m_renderData.clipRegion.empty()) {
        CRegion damageClip = m_renderData.clipBox;

        if (!m_renderData.clipRegion.empty()) {
            if (m_renderData.clipBox.empty())
                damageClip = m_renderData.clipRegion;
            else
                damageClip.intersect(m_renderData.clipRegion);
        }

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        data.damage->forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    tex->unbind();
}

void CHyprOpenGLImpl::renderTexturePrimitive(SP<CTexture> tex, const CBox& box) {
    RASSERT(m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->m_texID > 0), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTexturePrimitive");

    if (m_renderData.damage.empty())
        return;

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    // get transform
    const auto TRANSFORM = wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform));
    Mat3x3     matrix    = m_renderData.monitorProjection.projectBox(newBox, TRANSFORM, newBox.rot);
    Mat3x3     glMatrix  = m_renderData.projection.copy().multiply(matrix);

    SShader*   shader = &m_shaders->m_shPASSTHRURGBA;

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    useProgram(shader->program);
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    glBindVertexArray(shader->uniformLocations[SHADER_SHADER_VAO]);

    m_renderData.damage.forEachRect([this](const auto& RECT) {
        scissor(&RECT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    });

    scissor(nullptr);
    glBindVertexArray(0);
    tex->unbind();
}

void CHyprOpenGLImpl::renderTextureMatte(SP<CTexture> tex, const CBox& box, CFramebuffer& matte) {
    RASSERT(m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->m_texID > 0), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTextureMatte");

    if (m_renderData.damage.empty())
        return;

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    // get transform
    const auto TRANSFORM = wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform));
    Mat3x3     matrix    = m_renderData.monitorProjection.projectBox(newBox, TRANSFORM, newBox.rot);
    Mat3x3     glMatrix  = m_renderData.projection.copy().multiply(matrix);

    SShader*   shader = &m_shaders->m_shMATTE;

    useProgram(shader->program);
    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    shader->setUniformInt(SHADER_ALPHA_MATTE, 1);

    glActiveTexture(GL_TEXTURE0);
    tex->bind();

    glActiveTexture(GL_TEXTURE0 + 1);
    auto matteTex = matte.getTexture();
    matteTex->bind();

    glBindVertexArray(shader->uniformLocations[SHADER_SHADER_VAO]);

    m_renderData.damage.forEachRect([this](const auto& RECT) {
        scissor(&RECT);
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
CFramebuffer* CHyprOpenGLImpl::blurMainFramebufferWithDamage(float a, CRegion* originalDamage) {
    if (!m_renderData.currentFB->getTexture()) {
        Debug::log(ERR, "BUG THIS: null fb texture while attempting to blur main fb?! (introspection off?!)");
        return &m_renderData.pCurrentMonData->mirrorFB; // return something to sample from at least
    }

    return blurFramebufferWithDamage(a, originalDamage, *m_renderData.currentFB);
}

CFramebuffer* CHyprOpenGLImpl::blurFramebufferWithDamage(float a, CRegion* originalDamage, CFramebuffer& source) {
    TRACY_GPU_ZONE("RenderBlurFramebufferWithDamage");

    const auto BLENDBEFORE = m_blend;
    blend(false);
    setCapStatus(GL_STENCIL_TEST, false);

    // get transforms for the full monitor
    const auto TRANSFORM  = wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform));
    CBox       MONITORBOX = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
    Mat3x3     matrix     = m_renderData.monitorProjection.projectBox(MONITORBOX, TRANSFORM);
    Mat3x3     glMatrix   = m_renderData.projection.copy().multiply(matrix);

    // get the config settings
    static auto PBLURSIZE             = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES           = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    static auto PBLURVIBRANCY         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy_darkness");

    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    // prep damage
    CRegion damage{*originalDamage};
    damage.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                     m_renderData.pMonitor->m_transformedSize.y);
    damage.expand(std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES));

    // helper
    const auto    PMIRRORFB     = &m_renderData.pCurrentMonData->mirrorFB;
    const auto    PMIRRORSWAPFB = &m_renderData.pCurrentMonData->mirrorSwapFB;

    CFramebuffer* currentRenderToFB = PMIRRORFB;

    // Begin with base color adjustments - global brightness and contrast
    // TODO: make this a part of the first pass maybe to save on a drawcall?
    {
        static auto PBLURCONTRAST   = CConfigValue<Hyprlang::FLOAT>("decoration:blur:contrast");
        static auto PBLURBRIGHTNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");

        PMIRRORSWAPFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = source.getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        useProgram(m_shaders->m_shBLURPREPARE.program);

        // From FB to sRGB
        const bool skipCM = !m_cmSupported || m_renderData.pMonitor->m_imageDescription == SImageDescription{};
        m_shaders->m_shBLURPREPARE.setUniformInt(SHADER_SKIP_CM, skipCM);
        if (!skipCM) {
            passCMUniforms(m_shaders->m_shBLURPREPARE, m_renderData.pMonitor->m_imageDescription, SImageDescription{});
            m_shaders->m_shBLURPREPARE.setUniformFloat(SHADER_SDR_SATURATION,
                                                       m_renderData.pMonitor->m_sdrSaturation > 0 &&
                                                               m_renderData.pMonitor->m_imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                                           m_renderData.pMonitor->m_sdrSaturation :
                                                           1.0f);
            m_shaders->m_shBLURPREPARE.setUniformFloat(SHADER_SDR_BRIGHTNESS,
                                                       m_renderData.pMonitor->m_sdrBrightness > 0 &&
                                                               m_renderData.pMonitor->m_imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                                           m_renderData.pMonitor->m_sdrBrightness :
                                                           1.0f);
        }

        m_shaders->m_shBLURPREPARE.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        m_shaders->m_shBLURPREPARE.setUniformFloat(SHADER_CONTRAST, *PBLURCONTRAST);
        m_shaders->m_shBLURPREPARE.setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        m_shaders->m_shBLURPREPARE.setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(m_shaders->m_shBLURPREPARE.uniformLocations[SHADER_SHADER_VAO]);

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
    auto drawPass = [&](SShader* pShader, CRegion* pDamage) {
        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();

        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        useProgram(pShader->program);

        // prep two shaders
        pShader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        pShader->setUniformFloat(SHADER_RADIUS, *PBLURSIZE * a); // this makes the blursize change with a
        if (pShader == &m_shaders->m_shBLUR1) {
            m_shaders->m_shBLUR1.setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x / 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y / 2.f));
            m_shaders->m_shBLUR1.setUniformInt(SHADER_PASSES, BLUR_PASSES);
            m_shaders->m_shBLUR1.setUniformFloat(SHADER_VIBRANCY, *PBLURVIBRANCY);
            m_shaders->m_shBLUR1.setUniformFloat(SHADER_VIBRANCY_DARKNESS, *PBLURVIBRANCYDARKNESS);
        } else
            m_shaders->m_shBLUR2.setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x * 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y * 2.f));
        pShader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(pShader->uniformLocations[SHADER_SHADER_VAO]);

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
    for (auto i = 1; i <= BLUR_PASSES; ++i) {
        tempDamage = damage.copy().scale(1.f / (1 << i));
        drawPass(&m_shaders->m_shBLUR1, &tempDamage); // down
    }

    for (auto i = BLUR_PASSES - 1; i >= 0; --i) {
        tempDamage = damage.copy().scale(1.f / (1 << i)); // when upsampling we make the region twice as big
        drawPass(&m_shaders->m_shBLUR2, &tempDamage);     // up
    }

    // finalize the image
    {
        static auto PBLURNOISE      = CConfigValue<Hyprlang::FLOAT>("decoration:blur:noise");
        static auto PBLURBRIGHTNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");

        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();

        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        useProgram(m_shaders->m_shBLURFINISH.program);
        m_shaders->m_shBLURFINISH.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        m_shaders->m_shBLURFINISH.setUniformFloat(SHADER_NOISE, *PBLURNOISE);
        m_shaders->m_shBLURFINISH.setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);

        m_shaders->m_shBLURFINISH.setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(m_shaders->m_shBLURFINISH.uniformLocations[SHADER_SHADER_VAO]);

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

void CHyprOpenGLImpl::markBlurDirtyForMonitor(PHLMONITOR pMonitor) {
    m_monitorRenderResources[pMonitor].blurFBDirty = true;
}

void CHyprOpenGLImpl::preRender(PHLMONITOR pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    if (!*PBLURNEWOPTIMIZE || !m_monitorRenderResources[pMonitor].blurFBDirty || !*PBLUR)
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

        if (pWindow->m_windowData.noBlur.valueOrDefault())
            return false;

        if (pWindow->m_wlSurface->small() && !pWindow->m_wlSurface->m_fillIgnoreSmall)
            return true;

        const auto  PSURFACE = pWindow->m_wlSurface->resource();

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
                if (!ls->m_layerSurface || ls->m_xray != 1)
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
    m_monitorRenderResources[pMonitor].blurFBShouldRender = true;
}

void CHyprOpenGLImpl::preBlurForCurrentMonitor() {

    TRACY_GPU_ZONE("RenderPreBlurForCurrentMonitor");

    const auto SAVEDRENDERMODIF = m_renderData.renderModif;
    m_renderData.renderModif    = {}; // fix shit

    // make the fake dmg
    CRegion    fakeDamage{0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
    const auto POUTFB = blurMainFramebufferWithDamage(1, &fakeDamage);

    // render onto blurFB
    m_renderData.pCurrentMonData->blurFB.alloc(m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y,
                                               m_renderData.pMonitor->m_output->state->state().drmFormat);
    m_renderData.pCurrentMonData->blurFB.bind();

    clear(CHyprColor(0, 0, 0, 0));

    pushMonitorTransformEnabled(true);
    renderTextureInternal(POUTFB->getTexture(), CBox{0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y},
                          STextureRenderData{.damage = &fakeDamage, .a = 1, .round = 0, .roundingPower = 2.F, .discardActive = false, .allowCustomUV = false, .noAA = true});
    popMonitorTransformEnabled();

    m_renderData.currentFB->bind();

    m_renderData.pCurrentMonData->blurFBDirty = false;

    m_renderData.renderModif = SAVEDRENDERMODIF;

    m_monitorRenderResources[m_renderData.pMonitor].blurFBShouldRender = false;
}

void CHyprOpenGLImpl::preWindowPass() {
    if (!preBlurQueued())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CPreBlurElement>());
}

bool CHyprOpenGLImpl::preBlurQueued() {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    return m_renderData.pCurrentMonData->blurFBDirty && *PBLURNEWOPTIMIZE && *PBLUR && m_renderData.pCurrentMonData->blurFBShouldRender;
}

bool CHyprOpenGLImpl::shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");

    if (!m_renderData.pCurrentMonData->blurFB.getTexture())
        return false;

    if (pWindow && pWindow->m_windowData.xray.hasValue() && !pWindow->m_windowData.xray.valueOrDefault())
        return false;

    if (pLayer && pLayer->m_xray == 0)
        return false;

    if ((*PBLURNEWOPTIMIZE && pWindow && !pWindow->m_isFloating && !pWindow->onSpecialWorkspace()) || *PBLURXRAY)
        return true;

    if ((pLayer && pLayer->m_xray == 1) || (pWindow && pWindow->m_windowData.xray.valueOrDefault()))
        return true;

    return false;
}

void CHyprOpenGLImpl::renderTextureWithBlurInternal(SP<CTexture> tex, const CBox& box, const STextureRenderData& data) {
    RASSERT(m_renderData.pMonitor, "Tried to render texture with blur without begin()!");

    TRACY_GPU_ZONE("RenderTextureWithBlur");

    // make a damage region for this window
    CRegion texDamage{m_renderData.damage};
    texDamage.intersect(box.x, box.y, box.width, box.height);

    // While renderTextureInternalWithDamage will clip the blur as well,
    // clipping texDamage here allows blur generation to be optimized.
    if (!m_renderData.clipRegion.empty())
        texDamage.intersect(m_renderData.clipRegion);

    if (texDamage.empty())
        return;

    m_renderData.renderModif.applyToRegion(texDamage);

    // amazing hack: the surface has an opaque region!
    CRegion inverseOpaque;
    if (data.a >= 1.f && data.surface && std::round(data.surface->m_current.size.x * m_renderData.pMonitor->m_scale) == box.w &&
        std::round(data.surface->m_current.size.y * m_renderData.pMonitor->m_scale) == box.h) {
        pixman_box32_t surfbox = {0, 0, data.surface->m_current.size.x * data.surface->m_current.scale, data.surface->m_current.size.y * data.surface->m_current.scale};
        inverseOpaque          = data.surface->m_current.opaque;
        inverseOpaque.invert(&surfbox).intersect(0, 0, data.surface->m_current.size.x * data.surface->m_current.scale,
                                                 data.surface->m_current.size.y * data.surface->m_current.scale);

        if (inverseOpaque.empty()) {
            renderTextureInternal(tex, box, data);
            return;
        }
    } else
        inverseOpaque = {0, 0, box.width, box.height};

    inverseOpaque.scale(m_renderData.pMonitor->m_scale);

    //   vvv TODO: layered blur fbs?
    const bool    USENEWOPTIMIZE = shouldUseNewBlurOptimizations(m_renderData.currentLS.lock(), m_renderData.currentWindow.lock()) && !data.blockBlurOptimization;

    CFramebuffer* POUTFB = nullptr;
    if (!USENEWOPTIMIZE) {
        inverseOpaque.translate(box.pos());
        m_renderData.renderModif.applyToRegion(inverseOpaque);
        inverseOpaque.intersect(texDamage);
        POUTFB = blurMainFramebufferWithDamage(data.a, &inverseOpaque);
    } else
        POUTFB = &m_renderData.pCurrentMonData->blurFB;

    m_renderData.currentFB->bind();

    // make a stencil for rounded corners to work with blur
    scissor(nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    setCapStatus(GL_STENCIL_TEST, true);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    if (USENEWOPTIMIZE && !(m_renderData.discardMode & DISCARD_ALPHA))
        renderRect(box, CHyprColor(0, 0, 0, 0), SRectRenderData{.round = data.round, .roundingPower = data.roundingPower});
    else
        renderTexture(tex, box,
                      STextureRenderData{.a             = data.a,
                                         .round         = data.round,
                                         .roundingPower = data.roundingPower,
                                         .discardActive = true,
                                         .allowCustomUV = true,
                                         .wrapX         = data.wrapX,
                                         .wrapY         = data.wrapY}); // discard opaque
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // stencil done. Render everything.
    const auto LASTTL = m_renderData.primarySurfaceUVTopLeft;
    const auto LASTBR = m_renderData.primarySurfaceUVBottomRight;

    CBox       transformedBox = box;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    CBox monitorSpaceBox = {transformedBox.pos().x / m_renderData.pMonitor->m_pixelSize.x * m_renderData.pMonitor->m_transformedSize.x,
                            transformedBox.pos().y / m_renderData.pMonitor->m_pixelSize.y * m_renderData.pMonitor->m_transformedSize.y,
                            transformedBox.width / m_renderData.pMonitor->m_pixelSize.x * m_renderData.pMonitor->m_transformedSize.x,
                            transformedBox.height / m_renderData.pMonitor->m_pixelSize.y * m_renderData.pMonitor->m_transformedSize.y};

    m_renderData.primarySurfaceUVTopLeft     = monitorSpaceBox.pos() / m_renderData.pMonitor->m_transformedSize;
    m_renderData.primarySurfaceUVBottomRight = (monitorSpaceBox.pos() + monitorSpaceBox.size()) / m_renderData.pMonitor->m_transformedSize;

    static auto PBLURIGNOREOPACITY = CConfigValue<Hyprlang::INT>("decoration:blur:ignore_opacity");
    pushMonitorTransformEnabled(true);
    if (!USENEWOPTIMIZE)
        setRenderModifEnabled(false);
    renderTextureInternal(POUTFB->getTexture(), box,
                          STextureRenderData{
                              .damage        = &texDamage,
                              .a             = (*PBLURIGNOREOPACITY ? data.blurA : data.a * data.blurA) * data.overallA,
                              .round         = data.round,
                              .roundingPower = data.roundingPower,
                              .discardActive = false,
                              .allowCustomUV = true,
                              .noAA          = false,
                              .wrapX         = data.wrapX,
                              .wrapY         = data.wrapY,
                          });
    if (!USENEWOPTIMIZE)
        setRenderModifEnabled(true);
    popMonitorTransformEnabled();

    m_renderData.primarySurfaceUVTopLeft     = LASTTL;
    m_renderData.primarySurfaceUVBottomRight = LASTBR;

    // render the window, but clear stencil
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    // draw window
    setCapStatus(GL_STENCIL_TEST, false);
    renderTextureInternal(tex, box,
                          STextureRenderData{
                              .damage        = &texDamage,
                              .a             = data.a * data.overallA,
                              .round         = data.round,
                              .roundingPower = data.roundingPower,
                              .discardActive = false,
                              .allowCustomUV = true,
                              .allowDim      = true,
                              .noAA          = false,
                              .wrapX         = data.wrapX,
                              .wrapY         = data.wrapY,
                          });

    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    scissor(nullptr);
}

void CHyprOpenGLImpl::renderBorder(const CBox& box, const CGradientValueData& grad, SBorderRenderData data) {
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder");

    if (m_renderData.damage.empty() || (m_renderData.currentWindow && m_renderData.currentWindow->m_windowData.noBorder.valueOrDefault()))
        return;

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float  round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    Mat3x3 matrix = m_renderData.monitorProjection.projectBox(
        newBox, wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform)), newBox.rot);
    Mat3x3     glMatrix = m_renderData.projection.copy().multiply(matrix);

    const auto BLEND = m_blend;
    blend(true);

    useProgram(m_shaders->m_shBORDER1.program);

    const bool skipCM = !m_cmSupported || m_renderData.pMonitor->m_imageDescription == SImageDescription{};
    m_shaders->m_shBORDER1.setUniformInt(SHADER_SKIP_CM, skipCM);
    if (!skipCM)
        passCMUniforms(m_shaders->m_shBORDER1, SImageDescription{});

    m_shaders->m_shBORDER1.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    m_shaders->m_shBORDER1.setUniform4fv(SHADER_GRADIENT, grad.m_colorsOkLabA.size() / 4, grad.m_colorsOkLabA);
    m_shaders->m_shBORDER1.setUniformInt(SHADER_GRADIENT_LENGTH, grad.m_colorsOkLabA.size() / 4);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ANGLE, sc<int>(grad.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ALPHA, data.a);
    m_shaders->m_shBORDER1.setUniformInt(SHADER_GRADIENT2_LENGTH, 0);

    CBox transformedBox = newBox;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_RADIUS, round);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(m_shaders->m_shBORDER1.uniformLocations[SHADER_SHADER_VAO]);

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0) {
        CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
        damageClip.intersect(m_renderData.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);

    blend(BLEND);
}

void CHyprOpenGLImpl::renderBorder(const CBox& box, const CGradientValueData& grad1, const CGradientValueData& grad2, float lerp, SBorderRenderData data) {
    RASSERT((box.width > 0 && box.height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_renderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder2");

    if (m_renderData.damage.empty() || (m_renderData.currentWindow && m_renderData.currentWindow->m_windowData.noBorder.valueOrDefault()))
        return;

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float  round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    Mat3x3 matrix = m_renderData.monitorProjection.projectBox(
        newBox, wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform)), newBox.rot);
    Mat3x3     glMatrix = m_renderData.projection.copy().multiply(matrix);

    const auto BLEND = m_blend;
    blend(true);

    useProgram(m_shaders->m_shBORDER1.program);

    const bool skipCM = !m_cmSupported || m_renderData.pMonitor->m_imageDescription == SImageDescription{};
    m_shaders->m_shBORDER1.setUniformInt(SHADER_SKIP_CM, skipCM);
    if (!skipCM)
        passCMUniforms(m_shaders->m_shBORDER1, SImageDescription{});

    m_shaders->m_shBORDER1.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    m_shaders->m_shBORDER1.setUniform4fv(SHADER_GRADIENT, grad1.m_colorsOkLabA.size() / 4, grad1.m_colorsOkLabA);
    m_shaders->m_shBORDER1.setUniformInt(SHADER_GRADIENT_LENGTH, grad1.m_colorsOkLabA.size() / 4);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ANGLE, sc<int>(grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    if (!grad2.m_colorsOkLabA.empty())
        m_shaders->m_shBORDER1.setUniform4fv(SHADER_GRADIENT2, grad2.m_colorsOkLabA.size() / 4, grad2.m_colorsOkLabA);
    m_shaders->m_shBORDER1.setUniformInt(SHADER_GRADIENT2_LENGTH, grad2.m_colorsOkLabA.size() / 4);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ANGLE2, sc<int>(grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0));
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ALPHA, data.a);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_GRADIENT_LERP, lerp);

    CBox transformedBox = newBox;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    m_shaders->m_shBORDER1.setUniformFloat2(SHADER_FULL_SIZE_UNTRANSFORMED, sc<float>(newBox.width), sc<float>(newBox.height));
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_RADIUS, round);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_RADIUS_OUTER, data.outerRound == -1 ? round : data.outerRound);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);
    m_shaders->m_shBORDER1.setUniformFloat(SHADER_THICK, scaledBorderSize);

    glBindVertexArray(m_shaders->m_shBORDER1.uniformLocations[SHADER_SHADER_VAO]);

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0) {
        CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
        damageClip.intersect(m_renderData.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
    blend(BLEND);
}

void CHyprOpenGLImpl::renderRoundedShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, float a) {
    RASSERT(m_renderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box.width > 0 && box.height > 0), "Tried to render shadow with width/height < 0!");
    RASSERT(m_renderData.currentWindow, "Tried to render shadow without a window!");

    if (m_renderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    static auto PSHADOWPOWER = CConfigValue<Hyprlang::INT>("decoration:shadow:render_power");

    const auto  SHADOWPOWER = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);

    const auto  col = color;

    Mat3x3      matrix = m_renderData.monitorProjection.projectBox(
        newBox, wlTransformToHyprutils(invertTransform(!m_monitorTransformEnabled ? WL_OUTPUT_TRANSFORM_NORMAL : m_renderData.pMonitor->m_transform)), newBox.rot);
    Mat3x3 glMatrix = m_renderData.projection.copy().multiply(matrix);

    blend(true);

    useProgram(m_shaders->m_shSHADOW.program);
    const bool skipCM = !m_cmSupported || m_renderData.pMonitor->m_imageDescription == SImageDescription{};
    m_shaders->m_shSHADOW.setUniformInt(SHADER_SKIP_CM, skipCM);
    if (!skipCM)
        passCMUniforms(m_shaders->m_shSHADOW, SImageDescription{});

    m_shaders->m_shSHADOW.setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    m_shaders->m_shSHADOW.setUniformFloat4(SHADER_COLOR, col.r, col.g, col.b, col.a * a);

    const auto TOPLEFT     = Vector2D(range + round, range + round);
    const auto BOTTOMRIGHT = Vector2D(newBox.width - (range + round), newBox.height - (range + round));
    const auto FULLSIZE    = Vector2D(newBox.width, newBox.height);

    // Rounded corners
    m_shaders->m_shSHADOW.setUniformFloat2(SHADER_TOP_LEFT, sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y));
    m_shaders->m_shSHADOW.setUniformFloat2(SHADER_BOTTOM_RIGHT, sc<float>(BOTTOMRIGHT.x), sc<float>(BOTTOMRIGHT.y));
    m_shaders->m_shSHADOW.setUniformFloat2(SHADER_FULL_SIZE, sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y));
    m_shaders->m_shSHADOW.setUniformFloat(SHADER_RADIUS, range + round);
    m_shaders->m_shSHADOW.setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);
    m_shaders->m_shSHADOW.setUniformFloat(SHADER_RANGE, range);
    m_shaders->m_shSHADOW.setUniformFloat(SHADER_SHADOW_POWER, SHADOWPOWER);

    glBindVertexArray(m_shaders->m_shSHADOW.uniformLocations[SHADER_SHADER_VAO]);

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0) {
        CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
        damageClip.intersect(m_renderData.damage);

        if (!damageClip.empty()) {
            damageClip.forEachRect([this](const auto& RECT) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }
    } else {
        m_renderData.damage.forEachRect([this](const auto& RECT) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        });
    }

    glBindVertexArray(0);
}

void CHyprOpenGLImpl::saveBufferForMirror(const CBox& box) {

    if (!m_renderData.pCurrentMonData->monitorMirrorFB.isAllocated())
        m_renderData.pCurrentMonData->monitorMirrorFB.alloc(m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y,
                                                            m_renderData.pMonitor->m_output->state->state().drmFormat);

    m_renderData.pCurrentMonData->monitorMirrorFB.bind();

    blend(false);

    renderTexture(m_renderData.currentFB->getTexture(), box,
                  STextureRenderData{
                      .a             = 1.f,
                      .round         = 0,
                      .discardActive = false,
                      .allowCustomUV = false,
                  });

    blend(true);

    m_renderData.currentFB->bind();
}

void CHyprOpenGLImpl::renderMirrored() {

    auto         monitor  = m_renderData.pMonitor;
    auto         mirrored = monitor->m_mirrorOf;

    const double scale  = std::min(monitor->m_transformedSize.x / mirrored->m_transformedSize.x, monitor->m_transformedSize.y / mirrored->m_transformedSize.y);
    CBox         monbox = {0, 0, mirrored->m_transformedSize.x * scale, mirrored->m_transformedSize.y * scale};

    // transform box as it will be drawn on a transformed projection
    monbox.transform(wlTransformToHyprutils(mirrored->m_transform), mirrored->m_transformedSize.x * scale, mirrored->m_transformedSize.y * scale);

    monbox.x = (monitor->m_transformedSize.x - monbox.w) / 2;
    monbox.y = (monitor->m_transformedSize.y - monbox.h) / 2;

    const auto PFB = &m_monitorRenderResources[mirrored].monitorMirrorFB;
    if (!PFB->isAllocated() || !PFB->getTexture())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)}));

    CTexPassElement::SRenderData data;
    data.tex               = PFB->getTexture();
    data.box               = monbox;
    data.replaceProjection = Mat3x3::identity()
                                 .translate(monitor->m_pixelSize / 2.0)
                                 .transform(wlTransformToHyprutils(monitor->m_transform))
                                 .transform(wlTransformToHyprutils(invertTransform(mirrored->m_transform)))
                                 .translate(-monitor->m_transformedSize / 2.0);

    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void CHyprOpenGLImpl::renderSplash(cairo_t* const CAIRO, cairo_surface_t* const CAIROSURFACE, double offsetY, const Vector2D& size) {
    static auto           PSPLASHCOLOR = CConfigValue<Hyprlang::INT>("misc:col.splash");
    static auto           PSPLASHFONT  = CConfigValue<std::string>("misc:splash_font_family");
    static auto           FALLBACKFONT = CConfigValue<std::string>("misc:font_family");

    const auto            FONTFAMILY = *PSPLASHFONT != STRVAL_EMPTY ? *PSPLASHFONT : *FALLBACKFONT;
    const auto            FONTSIZE   = sc<int>(size.y / 76);
    const auto            COLOR      = CHyprColor(*PSPLASHCOLOR);

    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layoutText, pangoFD);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int textW = 0, textH = 0;
    pango_layout_set_text(layoutText, g_pCompositor->m_currentSplash.c_str(), -1);
    pango_layout_get_size(layoutText, &textW, &textH);
    textW /= PANGO_SCALE;
    textH /= PANGO_SCALE;

    cairo_move_to(CAIRO, (size.x - textW) / 2.0, size.y - textH - offsetY);
    pango_cairo_show_layout(CAIRO, layoutText);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);
}

std::string CHyprOpenGLImpl::resolveAssetPath(const std::string& filename) {
    std::string fullPath;
    for (auto& e : ASSET_PATHS) {
        std::string     p = std::string{e} + "/hypr/" + filename;
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            fullPath = p;
            break;
        } else
            Debug::log(LOG, "resolveAssetPath: looking at {} unsuccessful: ec {}", filename, ec.message());
    }

    if (fullPath.empty()) {
        m_failedAssetsNo++;
        Debug::log(ERR, "resolveAssetPath: looking for {} failed (no provider found)", filename);
        return "";
    }

    return fullPath;
}

SP<CTexture> CHyprOpenGLImpl::loadAsset(const std::string& filename) {

    const std::string fullPath = resolveAssetPath(filename);

    if (fullPath.empty())
        return m_missingAssetTexture;

    const auto CAIROSURFACE = cairo_image_surface_create_from_png(fullPath.c_str());

    if (!CAIROSURFACE) {
        m_failedAssetsNo++;
        Debug::log(ERR, "loadAsset: failed to load {} (corrupt / inaccessible / not png)", fullPath);
        return m_missingAssetTexture;
    }

    auto tex = texFromCairo(CAIROSURFACE);

    cairo_surface_destroy(CAIROSURFACE);

    return tex;
}

SP<CTexture> CHyprOpenGLImpl::texFromCairo(cairo_surface_t* cairo) {
    const auto CAIROFORMAT = cairo_image_surface_get_format(cairo);
    auto       tex         = makeShared<CTexture>();

    tex->allocate();
    tex->m_size = {cairo_image_surface_get_width(cairo), cairo_image_surface_get_height(cairo)};

    const GLint glIFormat = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB32F : GL_RGBA;
    const GLint glFormat  = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB : GL_RGBA;
    const GLint glType    = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_FLOAT : GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(cairo);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (CAIROFORMAT != CAIRO_FORMAT_RGB96F) {
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    return tex;
}

SP<CTexture> CHyprOpenGLImpl::renderText(const std::string& text, CHyprColor col, int pt, bool italic, const std::string& fontFamily, int maxWidth, int weight) {
    SP<CTexture>          tex = makeShared<CTexture>();

    static auto           FONT = CConfigValue<std::string>("misc:font_family");

    const auto            FONTFAMILY = fontFamily.empty() ? *FONT : fontFamily;
    const auto            FONTSIZE   = pt;
    const auto            COLOR      = col;

    auto                  CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1920, 1080 /* arbitrary, just for size */);
    auto                  CAIRO        = cairo_create(CAIROSURFACE);

    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, sc<PangoWeight>(weight));
    pango_layout_set_font_description(layoutText, pangoFD);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int textW = 0, textH = 0;
    pango_layout_set_text(layoutText, text.c_str(), -1);

    if (maxWidth > 0) {
        pango_layout_set_width(layoutText, maxWidth * PANGO_SCALE);
        pango_layout_set_ellipsize(layoutText, PANGO_ELLIPSIZE_END);
    }

    pango_layout_get_size(layoutText, &textW, &textH);
    textW /= PANGO_SCALE;
    textH /= PANGO_SCALE;

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textW, textH);
    CAIRO        = cairo_create(CAIROSURFACE);

    layoutText = pango_cairo_create_layout(CAIRO);
    pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, sc<PangoWeight>(weight));
    pango_layout_set_font_description(layoutText, pangoFD);
    pango_layout_set_text(layoutText, text.c_str(), -1);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    cairo_move_to(CAIRO, 0, 0);
    pango_cairo_show_layout(CAIRO, layoutText);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);

    tex->allocate();
    tex->m_size = {cairo_image_surface_get_width(CAIROSURFACE), cairo_image_surface_get_height(CAIROSURFACE)};

    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->m_size.x, tex->m_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    return tex;
}

void CHyprOpenGLImpl::initMissingAssetTexture() {
    SP<CTexture> tex = makeShared<CTexture>();
    tex->allocate();

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 512, 512);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_antialias(CAIRO, CAIRO_ANTIALIAS_NONE);
    cairo_save(CAIRO);
    cairo_set_source_rgba(CAIRO, 0, 0, 0, 1);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_SOURCE);
    cairo_paint(CAIRO);
    cairo_set_source_rgba(CAIRO, 1, 0, 1, 1);
    cairo_rectangle(CAIRO, 256, 0, 256, 256);
    cairo_fill(CAIRO);
    cairo_rectangle(CAIRO, 0, 256, 256, 256);
    cairo_fill(CAIRO);
    cairo_restore(CAIRO);

    cairo_surface_flush(CAIROSURFACE);

    tex->m_size = {512, 512};

    // copy the data to an OpenGL texture we have
    const GLint glFormat = GL_RGBA;
    const GLint glType   = GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);

    m_missingAssetTexture = tex;
}

void CHyprOpenGLImpl::useProgram(GLuint prog) {
    if (m_currentProgram == prog)
        return;

    glUseProgram(prog);
    m_currentProgram = prog;
}

void CHyprOpenGLImpl::initAssets() {
    initMissingAssetTexture();

    m_screencopyDeniedTexture = renderText("Permission denied to share screen", Colors::WHITE, 20);
}

void CHyprOpenGLImpl::ensureLockTexturesRendered(bool load) {
    static bool loaded = false;

    if (loaded == load)
        return;

    loaded = load;

    if (load) {
        // this will cause a small hitch. I don't think we can do much, other than wasting VRAM and having this loaded all the time.
        m_lockDeadTexture  = loadAsset("lockdead.png");
        m_lockDead2Texture = loadAsset("lockdead2.png");

        const auto VT = g_pCompositor->getVTNr();

        m_lockTtyTextTexture = renderText(std::format("Running on tty {}", VT.has_value() ? std::to_string(*VT) : "unknown"), CHyprColor{0.9F, 0.9F, 0.9F, 0.7F}, 20, true);
    } else {
        m_lockDeadTexture.reset();
        m_lockDead2Texture.reset();
        m_lockTtyTextTexture.reset();
    }
}

void CHyprOpenGLImpl::requestBackgroundResource() {
    if (m_backgroundResource)
        return;

    static auto PNOWALLPAPER    = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PFORCEWALLPAPER = CConfigValue<Hyprlang::INT>("misc:force_default_wallpaper");

    const auto  FORCEWALLPAPER = std::clamp(*PFORCEWALLPAPER, sc<int64_t>(-1), sc<int64_t>(2));

    if (*PNOWALLPAPER)
        return;

    static bool        once    = true;
    static std::string texPath = "wall";

    if (once) {
        // get the adequate tex
        if (FORCEWALLPAPER == -1) {
            std::mt19937_64                 engine(time(nullptr));
            std::uniform_int_distribution<> distribution(0, 2);

            texPath += std::to_string(distribution(engine));
        } else
            texPath += std::to_string(std::clamp(*PFORCEWALLPAPER, sc<int64_t>(0), sc<int64_t>(2)));

        texPath += ".png";

        texPath = resolveAssetPath(texPath);

        once = false;
    }

    if (texPath.empty()) {
        m_backgroundResourceFailed = true;
        return;
    }

    m_backgroundResource = makeAtomicShared<Hyprgraphics::CImageResource>(texPath);

    // doesn't have to be ASP as it's passed
    SP<CMainLoopExecutor> executor = makeShared<CMainLoopExecutor>([] {
        for (const auto& m : g_pCompositor->m_monitors) {
            g_pHyprRenderer->damageMonitor(m);
        }
    });

    m_backgroundResource->m_events.finished.listenStatic([executor] {
        // this is in the worker thread.
        executor->signal();
    });

    g_pAsyncResourceGatherer->enqueue(m_backgroundResource);
}

void CHyprOpenGLImpl::createBGTextureForMonitor(PHLMONITOR pMonitor) {
    RASSERT(m_renderData.pMonitor, "Tried to createBGTex without begin()!");

    Debug::log(LOG, "Creating a texture for BGTex");

    static auto PRENDERTEX = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PNOSPLASH  = CConfigValue<Hyprlang::INT>("misc:disable_splash_rendering");

    if (*PRENDERTEX || m_backgroundResourceFailed)
        return;

    if (!m_backgroundResource) {
        // queue the asset to be created
        requestBackgroundResource();
        return;
    }

    if (!m_backgroundResource->m_ready)
        return;

    // release the last tex if exists
    const auto PFB = &m_monitorBGFBs[pMonitor];
    PFB->release();

    PFB->alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, pMonitor->m_output->state->state().drmFormat);

    // create a new one with cairo
    SP<CTexture> tex = makeShared<CTexture>();

    tex->allocate();

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_antialias(CAIRO, CAIRO_ANTIALIAS_GOOD);
    cairo_save(CAIRO);
    cairo_set_source_rgba(CAIRO, 0, 0, 0, 0);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_SOURCE);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    if (!*PNOSPLASH)
        renderSplash(CAIRO, CAIROSURFACE, 0.02 * pMonitor->m_pixelSize.y, pMonitor->m_pixelSize);

    cairo_surface_flush(CAIROSURFACE);

    tex->m_size = pMonitor->m_pixelSize;

    // copy the data to an OpenGL texture we have
    const GLint glFormat = GL_RGBA;
    const GLint glType   = GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);

    // render the texture to our fb
    PFB->bind();
    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    blend(true);
    clear(CHyprColor{0, 0, 0, 1});

    SP<CTexture> backgroundTexture = texFromCairo(m_backgroundResource->m_asset.cairoSurface->cairo());

    // first render the background
    if (backgroundTexture) {
        const double MONRATIO = m_renderData.pMonitor->m_transformedSize.x / m_renderData.pMonitor->m_transformedSize.y;
        const double WPRATIO  = backgroundTexture->m_size.x / backgroundTexture->m_size.y;
        Vector2D     origin;
        double       scale = 1.0;

        if (MONRATIO > WPRATIO) {
            scale    = m_renderData.pMonitor->m_transformedSize.x / backgroundTexture->m_size.x;
            origin.y = (m_renderData.pMonitor->m_transformedSize.y - backgroundTexture->m_size.y * scale) / 2.0;
        } else {
            scale    = m_renderData.pMonitor->m_transformedSize.y / backgroundTexture->m_size.y;
            origin.x = (m_renderData.pMonitor->m_transformedSize.x - backgroundTexture->m_size.x * scale) / 2.0;
        }

        CBox texbox = CBox{origin, backgroundTexture->m_size * scale};
        renderTextureInternal(backgroundTexture, texbox, {.damage = &fakeDamage, .a = 1.0});
    }

    CBox monbox = {{}, pMonitor->m_pixelSize};
    renderTextureInternal(tex, monbox, {.damage = &fakeDamage, .a = 1.0});

    // bind back
    if (m_renderData.currentFB)
        m_renderData.currentFB->bind();

    Debug::log(LOG, "Background created for monitor {}", pMonitor->m_name);

    // clear the resource after we're done using it
    g_pEventLoopManager->doLater([this] { m_backgroundResource.reset(); });

    // set the animation to start for fading this background in nicely
    pMonitor->m_backgroundOpacity->setValueAndWarp(0.F);
    *pMonitor->m_backgroundOpacity = 1.F;
}

void CHyprOpenGLImpl::clearWithTex() {
    RASSERT(m_renderData.pMonitor, "Tried to render BGtex without begin()!");

    static auto PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");

    auto        TEXIT = m_monitorBGFBs.find(m_renderData.pMonitor);

    if (TEXIT == m_monitorBGFBs.end()) {
        createBGTextureForMonitor(m_renderData.pMonitor.lock());
        g_pHyprRenderer->m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));
    }

    if (TEXIT != m_monitorBGFBs.end()) {
        CTexPassElement::SRenderData data;
        data.box          = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
        data.a            = m_renderData.pMonitor->m_backgroundOpacity->value();
        data.flipEndFrame = true;
        data.tex          = TEXIT->second.getTexture();
        g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
    }
}

void CHyprOpenGLImpl::destroyMonitorResources(PHLMONITORREF pMonitor) {
    g_pHyprRenderer->makeEGLCurrent();

    if (!g_pHyprOpenGL)
        return;

    auto RESIT = g_pHyprOpenGL->m_monitorRenderResources.find(pMonitor);
    if (RESIT != g_pHyprOpenGL->m_monitorRenderResources.end()) {
        RESIT->second.mirrorFB.release();
        RESIT->second.offloadFB.release();
        RESIT->second.mirrorSwapFB.release();
        RESIT->second.monitorMirrorFB.release();
        RESIT->second.blurFB.release();
        RESIT->second.offMainFB.release();
        RESIT->second.stencilTex->destroyTexture();
        g_pHyprOpenGL->m_monitorRenderResources.erase(RESIT);
    }

    auto TEXIT = g_pHyprOpenGL->m_monitorBGFBs.find(pMonitor);
    if (TEXIT != g_pHyprOpenGL->m_monitorBGFBs.end()) {
        TEXIT->second.release();
        g_pHyprOpenGL->m_monitorBGFBs.erase(TEXIT);
    }

    if (pMonitor)
        Debug::log(LOG, "Monitor {} -> destroyed all render data", pMonitor->m_name);
}

void CHyprOpenGLImpl::saveMatrix() {
    m_renderData.savedProjection = m_renderData.projection;
}

void CHyprOpenGLImpl::setMatrixScaleTranslate(const Vector2D& translate, const float& scale) {
    m_renderData.projection.scale(scale).translate(translate);
}

void CHyprOpenGLImpl::restoreMatrix() {
    m_renderData.projection = m_renderData.savedProjection;
}

void CHyprOpenGLImpl::bindOffMain() {
    if (!m_renderData.pCurrentMonData->offMainFB.isAllocated()) {
        m_renderData.pCurrentMonData->offMainFB.alloc(m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y,
                                                      m_renderData.pMonitor->m_output->state->state().drmFormat);

        m_renderData.pCurrentMonData->offMainFB.addStencil(m_renderData.pCurrentMonData->stencilTex);
    }

    m_renderData.pCurrentMonData->offMainFB.bind();
    clear(CHyprColor(0, 0, 0, 0));
    m_renderData.currentFB = &m_renderData.pCurrentMonData->offMainFB;
}

void CHyprOpenGLImpl::renderOffToMain(CFramebuffer* off) {
    CBox monbox = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
    renderTexturePrimitive(off->getTexture(), monbox);
}

void CHyprOpenGLImpl::bindBackOnMain() {
    m_renderData.mainFB->bind();
    m_renderData.currentFB = m_renderData.mainFB;
}

void CHyprOpenGLImpl::pushMonitorTransformEnabled(bool enabled) {
    m_monitorTransformStack.push(enabled);
    m_monitorTransformEnabled = enabled;
}

void CHyprOpenGLImpl::popMonitorTransformEnabled() {
    m_monitorTransformStack.pop();
    m_monitorTransformEnabled = m_monitorTransformStack.top();
}

void CHyprOpenGLImpl::setRenderModifEnabled(bool enabled) {
    m_renderData.renderModif.enabled = enabled;
}

void CHyprOpenGLImpl::setViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (m_lastViewport.x == x && m_lastViewport.y == y && m_lastViewport.width == width && m_lastViewport.height == height)
        return;

    glViewport(x, y, width, height);
    m_lastViewport = {.x = x, .y = y, .width = width, .height = height};
}

void CHyprOpenGLImpl::setCapStatus(int cap, bool status) {
    // check if the capability status is already set to the desired status
    auto it            = m_capStatus.find(cap);
    bool currentStatus = (it != m_capStatus.end()) ? it->second : false; // default to 'false' if not found

    if (currentStatus == status)
        return;

    m_capStatus[cap] = status;

    // Enable or disable the capability based on status
    auto func = status ? [](int c) { glEnable(c); } : [](int c) { glDisable(c); };
    func(cap);
}

uint32_t CHyprOpenGLImpl::getPreferredReadFormat(PHLMONITOR pMonitor) {
    static const auto PFORCE8BIT = CConfigValue<Hyprlang::INT>("misc:screencopy_force_8b");

    if (!*PFORCE8BIT)
        return pMonitor->m_output->state->state().drmFormat;

    auto fmt = pMonitor->m_output->state->state().drmFormat;

    if (fmt == DRM_FORMAT_BGRA1010102 || fmt == DRM_FORMAT_ARGB2101010 || fmt == DRM_FORMAT_XRGB2101010 || fmt == DRM_FORMAT_BGRX1010102)
        return DRM_FORMAT_XRGB8888;

    return fmt;
}

bool CHyprOpenGLImpl::explicitSyncSupported() {
    return m_exts.EGL_ANDROID_native_fence_sync_ext;
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
        } catch (std::bad_any_cast& e) { Debug::log(ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::applyToBox!"); }
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
        } catch (std::bad_any_cast& e) { Debug::log(ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::applyToRegion!"); }
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
        } catch (std::bad_any_cast& e) { Debug::log(ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::combinedScale!"); }
    }
    return scale;
}

UP<CEGLSync> CEGLSync::create() {
    RASSERT(g_pHyprOpenGL->m_exts.EGL_ANDROID_native_fence_sync_ext, "Tried to create an EGL sync when syncs are not supported on the gpu");

    EGLSyncKHR sync = g_pHyprOpenGL->m_proc.eglCreateSyncKHR(g_pHyprOpenGL->m_eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);

    if (sync == EGL_NO_SYNC_KHR) {
        Debug::log(ERR, "eglCreateSyncKHR failed");
        return nullptr;
    }

    // we need to flush otherwise we might not get a valid fd
    glFlush();

    int fd = g_pHyprOpenGL->m_proc.eglDupNativeFenceFDANDROID(g_pHyprOpenGL->m_eglDisplay, sync);
    if (fd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
        Debug::log(ERR, "eglDupNativeFenceFDANDROID failed");
        return nullptr;
    }

    UP<CEGLSync> eglSync(new CEGLSync);
    eglSync->m_fd    = CFileDescriptor(fd);
    eglSync->m_sync  = sync;
    eglSync->m_valid = true;

    return eglSync;
}

CEGLSync::~CEGLSync() {
    if (m_sync == EGL_NO_SYNC_KHR)
        return;

    if (g_pHyprOpenGL && g_pHyprOpenGL->m_proc.eglDestroySyncKHR(g_pHyprOpenGL->m_eglDisplay, m_sync) != EGL_TRUE)
        Debug::log(ERR, "eglDestroySyncKHR failed");
}

CFileDescriptor& CEGLSync::fd() {
    return m_fd;
}

CFileDescriptor&& CEGLSync::takeFd() {
    return std::move(m_fd);
}

bool CEGLSync::isValid() {
    return m_valid && m_sync != EGL_NO_SYNC_KHR && m_fd.isValid();
}
