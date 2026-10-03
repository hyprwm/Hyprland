#include "Context.hpp"
#include "Renderbuffer.hpp"
#include "SceneResources.hpp"
#include "pass/BackdropScopePassElement.hpp"
#include "../output/Monitor.hpp"

using namespace Render;

CRenderContext::CRenderContext() = default;
CRenderContext::~CRenderContext() {
    RASSERT(m_usedAsyncBuffers.empty(), "Render context destroyed with untransferred source buffer uses");
}

bool CRenderContext::begin(SP<CSceneResources> resources) {
    if (m_active)
        return false;

    reset();
    m_sceneResources = std::move(resources);
    m_frameTime      = Time::steadyNow();
    m_active         = true;
    return true;
}

bool CRenderContext::active() const {
    return m_active;
}

const SP<CSceneResources>& CRenderContext::sceneResources() const {
    return m_sceneResources;
}

bool CRenderContext::readOnlyEffects() const {
    return m_sceneResources && m_sceneResources->isolated();
}

Time::steady_tp CRenderContext::effectTime() const {
    return readOnlyEffects() ? m_frameTime : Time::steadyNow();
}

CRenderDataScope CRenderContext::saveDrawState() {
    return CRenderDataScope{*this};
}

CRenderDataScope::CRenderDataScope(CRenderContext& ctx) :
    m_ctx(ctx), m_data(ctx.m_data), m_backdropDepth(ctx.m_backdropCaptures.size()),
    m_blurShouldRender(ctx.sceneResources() ? ctx.sceneResources()->blurQueued() : m_data.pMonitor && m_data.pMonitor->m_blurFBShouldRender) {
    ;
}

CRenderDataScope::~CRenderDataScope() {
    // Nested draws may only pop captures they pushed, never inherited captures.
    RASSERT(m_ctx.m_backdropCaptures.size() >= m_backdropDepth, "Nested draw popped an inherited backdrop capture");
    m_ctx.m_data = std::move(m_data);
    m_ctx.m_backdropCaptures.resize(m_backdropDepth);
    if (m_ctx.sceneResources())
        m_ctx.sceneResources()->setBlurQueued(m_blurShouldRender);
    else if (m_ctx.m_data.pMonitor)
        m_ctx.m_data.pMonitor->m_blurFBShouldRender = m_blurShouldRender;
}

void CRenderContext::reset() {
    RASSERT(m_usedAsyncBuffers.empty(), "Render context reset with untransferred source buffer uses");

    // Keep the scratch vector's allocation, but release the values it contains.
    auto modifs = std::move(m_data.renderModif.modifs);
    modifs.clear();
    m_data                    = {};
    m_data.renderModif.modifs = std::move(modifs);

    m_pass.clear();
    m_currentPass = nullptr;
    m_mode        = RENDER_MODE_NORMAL;
    m_currentBuffer.reset();
    m_currentRenderbuffer.reset();
    m_backdropCaptures.clear();
    m_cmSettingsCache.clear();
    m_sceneResources.reset();
    m_frameTime            = {};
    m_blockSurfaceFeedback = false;
    m_renderingSnapshot    = false;
    m_swapchainAcquired    = false;
    m_gl                   = {};
    m_active               = false;
}
