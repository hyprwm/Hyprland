#include "Context.hpp"
#include "Renderbuffer.hpp"
#include "pass/BackdropScopePassElement.hpp"

using namespace Render;

CRenderContext::CRenderContext()  = default;
CRenderContext::~CRenderContext() = default;

bool CRenderContext::begin() {
    if (m_active)
        return false;

    reset();
    m_active = true;
    return true;
}

bool CRenderContext::active() const {
    return m_active;
}

void CRenderContext::reset() {
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
    m_blockSurfaceFeedback = false;
    m_renderingSnapshot    = false;
    m_swapchainAcquired    = false;
    m_gl                   = {};
    m_active               = false;
}
