#include "Pass.hpp"
#include "../OpenGL.hpp"
#include <algorithm>
#include <ranges>
#include "../../config/ConfigValue.hpp"

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
    // TODO: use precompute blur for instances where there is nothing in between

    // if there is live blur, we need to NOT occlude any area where it will be influenced
    const auto WILLBLUR = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    CRegion    newDamage = damage.copy().intersect(CBox{{}, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize});
    for (auto& el : m_vPassElements | std::views::reverse) {

        if (newDamage.empty()) {
            el->discard = true;
            continue;
        }

        el->elementDamage = newDamage;
        auto bb           = el->element->boundingBox();
        if (!bb)
            continue;

        // drop if empty
        if (CRegion copy = newDamage.copy(); copy.intersect(*bb).empty()) {
            el->discard = true;
            continue;
        }

        auto opaque = el->element->opaqueRegion();

        if (!opaque.empty()) {
            // if this intersects the liveBlur region, allow live blur to operate correctly.
            // do not occlude a border near it.
            if (WILLBLUR) {
                CRegion liveBlurRegion;
                for (auto& el2 : m_vPassElements) {
                    // if we reach self, no problem, we can break.
                    // if the blur is above us, we don't care, it will work fine.
                    if (el2 == el)
                        break;

                    if (!el2->element->needsLiveBlur())
                        continue;

                    const auto BB = el2->element->boundingBox();
                    RASSERT(BB, "No bounding box for an element with live blur is illegal");

                    liveBlurRegion.add(*BB);
                }

                if (auto infringement = opaque.copy().intersect(liveBlurRegion); !infringement.empty()) {
                    // eh, this is not the correct solution, but it will do...
                    // TODO: is this *easily* fixable?
                    opaque.subtract(infringement.expand(oneBlurRadius()));
                }
            }
            newDamage.subtract(opaque);
        }
    }
}

void CRenderPass::clear() {
    m_vPassElements.clear();
}

CRegion CRenderPass::render(const CRegion& damage_) {
    const auto WILLBLUR = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    damage = damage_.copy();

    if (WILLBLUR) {
        // combine blur regions into one that will be expanded
        CRegion blurRegion;
        for (auto& el : m_vPassElements) {
            if (!el->element->needsLiveBlur())
                continue;

            const auto BB = el->element->boundingBox();
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            blurRegion.add(*BB);
        }

        blurRegion.intersect(damage).expand(oneBlurRadius());

        g_pHyprOpenGL->m_RenderData.finalDamage = blurRegion.copy().add(damage);

        // FIXME: why does this break on * 1.F ?
        // used to work when we expand all the damage... I think? Well, before pass.
        // moving a window over blur shows the edges being wonk.
        blurRegion.expand(oneBlurRadius() * 1.5F);

        damage = blurRegion.copy().add(damage);
    } else
        g_pHyprOpenGL->m_RenderData.finalDamage = damage;

    if (std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->disableSimplification(); })) {
        for (auto& el : m_vPassElements) {
            el->elementDamage = damage;
        }
    } else
        simplify();

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsPrecomputeBlur(); });

    if (m_vPassElements.empty())
        return {};

    for (auto& el : m_vPassElements) {
        if (el->discard) {
            el->element->discard();
            continue;
        }

        g_pHyprOpenGL->m_RenderData.damage = el->elementDamage;
        el->element->draw(el->elementDamage);
    }

    g_pHyprOpenGL->m_RenderData.damage = damage;
    return damage;
}

float CRenderPass::oneBlurRadius() {
    // TODO: is this exact range correct?
    static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    return *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES); // is this 2^pass? I don't know but it works... I think.
}
