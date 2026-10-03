#include "SceneResources.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "../output/Monitor.hpp"
#include "../output/MonitorResources.hpp"
#include <cmath>
#include <limits>

using namespace Render;

CSceneResources::CSceneResources(PHLMONITORREF monitor) : m_monitor(monitor) {
    ;
}

CSceneResources::CSceneResources(SP<IFramebuffer> blurFramebuffer) : m_blurFramebuffer(std::move(blurFramebuffer)), m_isolated(true) {
    ;
}

CSceneResources::~CSceneResources() {
    if (m_blurFramebuffer && GL::g_pHyprOpenGL)
        GL::g_pHyprOpenGL->makeEGLCurrent();
}

bool CSceneResources::isolated() const {
    return m_isolated;
}

bool CSceneResources::prepare(const Vector2D& size, DRMFormat format, NColorManagement::PImageDescription description) {
    if (!m_isolated)
        return false;

    m_dirty                   = true;
    m_queued                  = false;
    m_valid                   = false;
    m_prepared                = false;
    const auto validDimension = [](double value) { return std::isfinite(value) && value > 0 && value <= std::numeric_limits<int>::max() && std::floor(value) == value; };
    if (!m_blurFramebuffer || !validDimension(size.x) || !validDimension(size.y) || !m_blurFramebuffer->alloc(sc<int>(size.x), sc<int>(size.y), format))
        return false;

    m_blurFramebuffer->setImageDescription(description);
    m_prepared = true;
    return true;
}

SP<IFramebuffer> CSceneResources::blurFramebuffer() const {
    if (m_isolated)
        return m_blurFramebuffer;
    return m_monitor ? m_monitor->resources()->m_blurFB : nullptr;
}

SP<ITexture> CSceneResources::blurTexture() const {
    if (m_isolated && !m_valid)
        return nullptr;
    const auto FB = blurFramebuffer();
    return FB ? FB->getTexture() : nullptr;
}

bool CSceneResources::blurDirty() const {
    return m_isolated ? m_dirty : m_monitor && m_monitor->m_blurFBDirty;
}

bool CSceneResources::canPrecomputeBlur() const {
    const auto FB = blurFramebuffer();
    return (!m_isolated || m_prepared) && FB && FB->isAllocated();
}

SP<IFramebuffer> CSceneResources::prepareWorkBuffer(SP<IFramebuffer> framebuffer) const {
    if (!framebuffer)
        return nullptr;
    if (m_isolated)
        framebuffer->disableMirror();
    return framebuffer->isAllocated() ? framebuffer : nullptr;
}

void CSceneResources::setBlurDirty(bool dirty) {
    if (m_isolated) {
        m_dirty = dirty;
        if (dirty)
            m_valid = false;
    } else if (m_monitor)
        m_monitor->m_blurFBDirty = dirty;
}

bool CSceneResources::blurQueued() const {
    return m_isolated ? m_queued : m_monitor && m_monitor->m_blurFBShouldRender;
}

void CSceneResources::setBlurQueued(bool queued) {
    if (m_isolated)
        m_queued = queued;
    else if (m_monitor)
        m_monitor->m_blurFBShouldRender = queued;
}

void CSceneResources::completePreBlur() {
    if (m_isolated) {
        if (!m_prepared || !m_blurFramebuffer || !m_blurFramebuffer->isAllocated())
            return;
        m_valid = true;
    }
    setBlurDirty(false);
    setBlurQueued(false);
}
