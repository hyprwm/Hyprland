#include "Pass.hpp"
#include "../OpenGL.hpp"

bool CRenderPass::empty() const {
    return false;
}

bool CRenderPass::single() const {
    return m_vPassElements.size() == 1;
}

bool CRenderPass::needsIntrospection() const {
    return true;
}

void CRenderPass::add(SP<IPassElement> el) {
    m_vPassElements.emplace_back(el);
}

void CRenderPass::simplify() {}

void CRenderPass::clear() {
    m_vPassElements.clear();
}

void CRenderPass::render() {
    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = false; // TODO:

    for (auto& el : m_vPassElements) {
        el->draw(g_pHyprOpenGL->m_RenderData.damage);
    }
}