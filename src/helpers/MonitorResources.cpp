#include "MonitorResources.hpp"
#include "render/Renderer.hpp"
#include <algorithm>
#include <format>

using namespace Monitor;

static const int MAX_WORK_BUFFERS   = 8;
static const int MAX_UNUSED_SECONDS = 5;

CMonitorResources::CMonitorResources(WP<CMonitor> monitor, DRMFormat format, Vector2D size, NColorManagement::PImageDescription imageDescription) :
    m_stencilTex(g_pHyprRenderer->createStencilTexture(monitor->m_pixelSize.x, monitor->m_pixelSize.y)),
    m_blurFB(g_pHyprRenderer->createFB(std::format("Monitor {} blur FB", monitor->m_name))), m_monitor(monitor), m_drmFormat(format), m_size(size),
    m_imageDescription(imageDescription) {
    initFB(m_blurFB);
}

void CMonitorResources::initFB(SP<Render::IFramebuffer> fb) {
    fb->addStencil(m_stencilTex);
    fb->alloc(m_size.x, m_size.y, m_drmFormat);
    fb->getTexture()->m_imageDescription = m_imageDescription;
}

SP<Render::IFramebuffer> CMonitorResources::getUnusedWorkBuffer() {
    std::erase_if(m_workBuffers, [](const auto& res) { return res.lastUsed.getSeconds() >= MAX_UNUSED_SECONDS; });

    auto found = std::ranges::find_if(m_workBuffers, [](const auto& res) { return res.buffer.strongRef() < 2; });
    if (found != m_workBuffers.end()) {
        found->lastUsed.reset();
        return found->buffer;
    }
    if (m_workBuffers.size() >= MAX_WORK_BUFFERS)
        return nullptr;

    auto& res = m_workBuffers.emplace_back(g_pHyprRenderer->createFB(std::format("Monitor {} workbuffer", m_monitor->m_name)));
    initFB(res.buffer);
    res.lastUsed.reset();
    return res.buffer;
}

void CMonitorResources::forEachUnusedFB(std::function<void(SP<Render::IFramebuffer>)> callback, bool includeNamed) {
    for (const auto& res : m_workBuffers) {
        if (res.buffer.strongRef() > 1)
            continue;

        callback(res.buffer);
    }
    if (includeNamed) {
        if (m_blurFB && m_blurFB->isAllocated() && m_blurFB.strongRef() < 2)
            callback(m_blurFB);
        if (hasMirrorFB() && m_monitorMirrorFB.strongRef() < 2)
            callback(m_monitorMirrorFB);
    }
}

bool CMonitorResources::hasMirrorFB() {
    return m_monitorMirrorFB && m_monitorMirrorFB->isAllocated();
}

SP<Render::IFramebuffer> CMonitorResources::mirrorFB() {
    if (!m_monitorMirrorFB)
        m_monitorMirrorFB = g_pHyprRenderer->createFB(std::format("Monitor {} mirror FB", m_monitor->m_name));

    if (!m_monitorMirrorFB->isAllocated())
        m_monitorMirrorFB->alloc(m_size.x, m_size.y, m_drmFormat);

    return m_monitorMirrorFB;
}
