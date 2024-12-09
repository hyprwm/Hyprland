#include "Pass.hpp"
#include "../OpenGL.hpp"
#include <algorithm>
#include <ranges>

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
    m_vPassElements.emplace_back(makeShared<SPassElementData>(CRegion{}, el));
}

void CRenderPass::simplify() {
    std::vector<SP<SPassElementData>> toRemove;
    CRegion                           newDamage = damage.copy().intersect(CBox{{}, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize});
    for (auto& el : m_vPassElements | std::views::reverse) {

        if (newDamage.empty()) {
            toRemove.emplace_back(el);
            continue;
        }

        el->elementDamage = newDamage;
        auto bb           = el->element->boundingBox();
        if (!bb)
            continue;

        // drop if empty
        if (CRegion copy = newDamage.copy(); copy.intersect(*bb).empty()) {
            toRemove.emplace_back(el);
            continue;
        }

        const auto opaque = el->element->opaqueRegion();

        if (!opaque.empty())
            newDamage.subtract(opaque);
    }

    std::erase_if(m_vPassElements, [&toRemove](const auto& el) { return std::find(toRemove.begin(), toRemove.end(), el) != toRemove.end(); });
}

void CRenderPass::clear() {
    m_vPassElements.clear();
}

void CRenderPass::render(const CRegion& damage_) {
    damage = damage_.copy();

    if (std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->disableSimplification(); })) {
        for (auto& el : m_vPassElements) {
            el->elementDamage = damage;
        }
    } else
        simplify();

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsPrecomputeBlur(); });

    if (m_vPassElements.empty())
        return;

    for (auto& el : m_vPassElements) {
        g_pHyprOpenGL->m_RenderData.damage = el->elementDamage;
        el->element->draw(el->elementDamage);
    }

    g_pHyprOpenGL->m_RenderData.damage = damage;
}
