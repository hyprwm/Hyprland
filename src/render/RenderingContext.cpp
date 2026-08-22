#include "types.hpp"

#include "../output/Monitor.hpp"
#include "pass/Pass.hpp"

using namespace Render;

CRenderingContext::CRenderingContext(PHLMONITORREF sceneMonitor_, CRenderPass& pass, PHLMONITORREF outputMonitor_) :
    sceneMonitor(sceneMonitor_), outputMonitor(outputMonitor_ ? outputMonitor_ : sceneMonitor_), precomputeBlur(sceneMonitor_ && sceneMonitor_->m_blurFBShouldRender),
    cmSettingsCache(makeShared<SCMSettingsCache>()), m_pass(pass) {
    ;
}

CRenderingContext::CRenderingContext(const CRenderingContext& parent, CRenderPass& pass) : CRenderingContext(parent, pass, parent.sceneMonitor) {
    ;
}

CRenderingContext::CRenderingContext(const CRenderingContext& parent, CRenderPass& pass, PHLMONITORREF sceneMonitor_) : CRenderingContext(parent) {
    m_pass                  = pass;
    updatesMonitorBlurState = false;

    if (sceneMonitor == sceneMonitor_)
        return;

    sceneMonitor    = sceneMonitor_;
    precomputeBlur  = false;
    cmSettingsCache = makeShared<SCMSettingsCache>();
    backdropCaptures.clear();
}

CRenderPass& CRenderingContext::renderPass() const {
    return m_pass.get();
}
