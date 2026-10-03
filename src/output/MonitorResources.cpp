#include "MonitorResources.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../helpers/cm/ColorManagement.hpp"
#include "../render/Renderer.hpp"
#include "../render/SceneResources.hpp"
#include "../render/OpenGL.hpp"
#include "../config/ConfigValue.hpp"
#include <cstdint>
#include <format>
#include <limits>

using namespace Monitor;
using namespace NColorManagement;

static void prepareWorkBufferRelease() {
    // Timer expiry also runs when no frame has made the rendering context current.
    if (Render::GL::g_pHyprOpenGL)
        Render::GL::g_pHyprOpenGL->makeEGLCurrent();
}

CMonitorResources::CMonitorResources(WP<CMonitor> monitor, DRMFormat format, Vector2D size, NColorManagement::PImageDescription imageDescription) :
    m_stencilTex(g_pHyprRenderer->createStencilTexture(size.x, size.y)), m_blurFB(g_pHyprRenderer->createFB(std::format("Monitor {} blur FB", monitor->m_name))),
    m_monitor(monitor), m_drmFormat(format), m_size(size), m_imageDescription(imageDescription),
    m_workBuffers(
        [this] {
            auto buffer = g_pHyprRenderer->createFB(std::format("Monitor {} workbuffer", m_monitor->m_name));
            if (buffer)
                buffer->addStencil(m_stencilTex);
            return buffer;
        },
        prepareWorkBufferRelease, std::numeric_limits<uint64_t>::max()),
    m_sizedWorkBuffers([this] { return g_pHyprRenderer->createFB(std::format("Monitor {} sized workbuffer", m_monitor->m_name)); }, prepareWorkBufferRelease) {
    initFB(m_blurFB);
    monitor->m_blurFBDirty = true;
    m_sceneResources       = makeShared<Render::CSceneResources>(monitor);
}

const SP<Render::CSceneResources>& CMonitorResources::sceneResources() const {
    return m_sceneResources;
}

bool CMonitorResources::prepareSceneResources(Render::CSceneResources& resources) const {
    return resources.prepare(m_size, m_drmFormat, m_imageDescription);
}

void CMonitorResources::initFB(SP<Render::IFramebuffer> fb) {
    fb->addStencil(m_stencilTex);
    fb->alloc(m_size.x, m_size.y, m_drmFormat);
    fb->setImageDescription(m_imageDescription);
}

void CMonitorResources::setImageDescription(NColorManagement::PImageDescription imageDescription) {
    if (m_imageDescription == imageDescription)
        return;
    m_imageDescription = imageDescription;
    m_blurFB->setImageDescription(imageDescription);
    m_workBuffers.setImageDescription(imageDescription);
    m_sizedWorkBuffers.setImageDescription(imageDescription);
    if (m_monitorMirrorFB)
        m_monitorMirrorFB->setImageDescription(getMirrorTexImageDescription());
    if (m_mirrorTex)
        m_mirrorTex->m_imageDescription = getMirrorTexImageDescription();
    invalidateMirrorFB();
}

SP<Render::IFramebuffer> CMonitorResources::getUnusedWorkBuffer() {
    return m_workBuffers.acquire(m_size, m_drmFormat, m_imageDescription);
}

SP<Render::IFramebuffer> CMonitorResources::getUnusedWorkBuffer(const Vector2D& size) {
    return m_sizedWorkBuffers.acquire(size, m_drmFormat, m_imageDescription);
}

void CMonitorResources::forEachUnusedFB(std::function<void(SP<Render::IFramebuffer>)> callback, bool includeNamed) {
    m_workBuffers.forEachUnused(callback);
    m_sizedWorkBuffers.forEachUnused(callback);
    if (includeNamed) {
        if (m_blurFB && m_blurFB->isAllocated() && m_blurFB.strongRef() < 2)
            callback(m_blurFB);
        if (hasMirrorFB() && m_monitorMirrorFB.strongRef() < 2)
            callback(m_monitorMirrorFB);
    }
}

bool CMonitorResources::hasMirrorFB() const {
    return m_monitorMirrorFB && m_monitorMirrorFB->isAllocated();
}

bool CMonitorResources::shouldKeepMirrorFB() const {
    return !m_monitor->m_mirrors.empty() || Screenshare::mgr()->isOutputBeingSSd(m_monitor.lock());
}

void CMonitorResources::releaseMirrorFB() {
    if (m_monitorMirrorFB)
        m_monitorMirrorFB->release();

    invalidateMirrorFB();
}

void CMonitorResources::invalidateMirrorFB() {
    m_mirrorFBValid            = false;
    m_mirrorFBNeedsFullRefresh = true;
    m_mirrorFBStaleDamage.clear();
}

void CMonitorResources::markMirrorFBStale(const CRegion& damage) {
    if (damage.empty() || !hasMirrorFB() || !m_mirrorFBValid)
        return;

    m_mirrorFBStaleDamage.add(damage).intersect(CBox{{}, mirrorFBDamageSize()});
}

void CMonitorResources::markMirrorFBStale() {
    if (!hasMirrorFB() || !m_mirrorFBValid)
        return;

    m_mirrorFBNeedsFullRefresh = true;
    m_mirrorFBStaleDamage.clear();
}

void CMonitorResources::markMirrorFBUpdated() {
    m_mirrorFBValid            = true;
    m_mirrorFBNeedsFullRefresh = false;
    m_mirrorFBStaleDamage.clear();
}

CRegion CMonitorResources::pendingMirrorFBDamage() const {
    const auto DAMAGE_SIZE = mirrorFBDamageSize();
    if (!hasMirrorFB() || !m_mirrorFBValid || m_mirrorFBNeedsFullRefresh)
        return CRegion{0, 0, DAMAGE_SIZE.x, DAMAGE_SIZE.y};

    return m_mirrorFBStaleDamage.copy();
}

SP<Render::IFramebuffer> CMonitorResources::mirrorFB() {
    if (!m_monitorMirrorFB)
        m_monitorMirrorFB = g_pHyprRenderer->createFB(std::format("Monitor {} mirror FB", m_monitor->m_name));

    if (!m_monitorMirrorFB->isAllocated()) {
        m_monitorMirrorFB->alloc(m_size.x, m_size.y, m_monitor->m_activeMonitorRule.m_enable10bit ? DRM_FORMAT_XRGB2101010 : DRM_FORMAT_XRGB8888);
        m_monitorMirrorFB->setImageDescription(getMirrorTexImageDescription());
    }

    return m_monitorMirrorFB;
}

SP<Render::ITexture> CMonitorResources::getMirrorTexture() {
    return hasMirrorFB() ? mirrorFB()->getTexture() : nullptr;
}

NColorManagement::PImageDescription CMonitorResources::getMirrorTexImageDescription() {
    const auto TF = m_imageDescription->value().transferFunction;
    if (TF == CM_TRANSFER_FUNCTION_GAMMA22 || TF == CM_TRANSFER_FUNCTION_SRGB)
        return m_imageDescription;

    return DEFAULT_SRGB_IMAGE_DESCRIPTION;
}

Vector2D CMonitorResources::mirrorFBDamageSize() const {
    return m_monitor->m_transformedSize;
}

void CMonitorResources::enableMirror() {
    if (m_mirrorTex)
        return;
    m_mirrorTex = g_pHyprRenderer->createTexture();
    m_mirrorTex->allocate({m_size.x, m_size.y}, m_monitor->m_activeMonitorRule.m_enable10bit ? DRM_FORMAT_XRGB2101010 : DRM_FORMAT_XRGB8888);
    m_mirrorTex->m_imageDescription = getMirrorTexImageDescription();
    m_monitor->m_blurFBDirty        = true;
}

void CMonitorResources::disableMirror() {
    if (m_mirrorTex)
        m_monitor->m_blurFBDirty = true;
    m_mirrorTex.reset();
}

void CMonitorResources::refreshBlurFB() {
    static auto PBLURENABLED = CConfigValue<Config::BOOL>("decoration:blur:enabled");

    // TODO: this is super naive. It does free VRAM, but I don't want to make an auto-initializing wrapper
    // for now and this is good enough.
    if (!*PBLURENABLED) {
        m_blurFB->addStencil(nullptr);
        m_blurFB->alloc(1, 1, m_drmFormat);
    } else {
        m_blurFB->addStencil(m_stencilTex);
        m_blurFB->alloc(m_size.x, m_size.y, m_drmFormat);
    }
}
