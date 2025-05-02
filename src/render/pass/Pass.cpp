#include "Pass.hpp"
#include "../OpenGL.hpp"
#include <algorithm>
#include <ranges>
#include "../../config/ConfigValue.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/core/Compositor.hpp"

bool CRenderPass::empty() const {
    return false;
}

bool CRenderPass::single() const {
    return m_vPassElements.size() == 1;
}

void CRenderPass::add(SP<IPassElement> el) {
    m_vPassElements.emplace_back(makeShared<SPassElementData>(CRegion{}, el));
}

void CRenderPass::simplify() {
    static auto PDEBUGPASS = CConfigValue<Hyprlang::INT>("debug:pass");

    // TODO: use precompute blur for instances where there is nothing in between

    // if there is live blur, we need to NOT occlude any area where it will be influenced
    const auto WILLBLUR = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    CRegion    newDamage = damage.copy().intersect(CBox{{}, g_pHyprOpenGL->m_RenderData.pMonitor->m_transformedSize});
    for (auto& el : m_vPassElements | std::views::reverse) {

        if (newDamage.empty() && !el->element->undiscardable()) {
            el->discard = true;
            continue;
        }

        el->elementDamage = newDamage;
        auto bb1          = el->element->boundingBox();
        if (!bb1 || newDamage.empty())
            continue;

        auto bb = bb1->scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);

        // drop if empty
        if (CRegion copy = newDamage.copy(); copy.intersect(bb).empty()) {
            el->discard = true;
            continue;
        }

        auto opaque = el->element->opaqueRegion();

        if (!opaque.empty()) {
            opaque.scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);

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

                // expand the region: this area needs to be proper to blur it right.
                liveBlurRegion.scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale).expand(oneBlurRadius() * 2.F);

                if (auto infringement = opaque.copy().intersect(liveBlurRegion); !infringement.empty()) {
                    // eh, this is not the correct solution, but it will do...
                    // TODO: is this *easily* fixable?
                    opaque.subtract(infringement);
                }
            }
            newDamage.subtract(opaque);
            if (*PDEBUGPASS)
                occludedRegions.emplace_back(opaque);
        }
    }

    if (*PDEBUGPASS) {
        for (auto& el2 : m_vPassElements) {
            if (!el2->element->needsLiveBlur())
                continue;

            const auto BB = el2->element->boundingBox();
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            totalLiveBlurRegion.add(BB->copy().scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale));
        }
    }
}

void CRenderPass::clear() {
    m_vPassElements.clear();
}

CRegion CRenderPass::render(const CRegion& damage_) {
    static auto PDEBUGPASS = CConfigValue<Hyprlang::INT>("debug:pass");

    const auto  WILLBLUR = std::ranges::any_of(m_vPassElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    damage = *PDEBUGPASS ? CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}} : damage_.copy();
    if (*PDEBUGPASS) {
        occludedRegions.clear();
        totalLiveBlurRegion = CRegion{};
    }

    if (damage.empty()) {
        g_pHyprOpenGL->m_RenderData.damage      = damage;
        g_pHyprOpenGL->m_RenderData.finalDamage = damage;
        return damage;
    }

    if (!*PDEBUGPASS && debugData.present)
        debugData = {false};
    else if (*PDEBUGPASS && !debugData.present) {
        debugData.present           = true;
        debugData.keyboardFocusText = g_pHyprOpenGL->renderText("keyboard", Colors::WHITE, 12);
        debugData.pointerFocusText  = g_pHyprOpenGL->renderText("pointer", Colors::WHITE, 12);
        debugData.lastWindowText    = g_pHyprOpenGL->renderText("lastWindow", Colors::WHITE, 12);
    }

    if (WILLBLUR && !*PDEBUGPASS) {
        // combine blur regions into one that will be expanded
        CRegion blurRegion;
        for (auto& el : m_vPassElements) {
            if (!el->element->needsLiveBlur())
                continue;

            const auto BB = el->element->boundingBox();
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            blurRegion.add(*BB);
        }

        blurRegion.scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);

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

    if (*PDEBUGPASS) {
        renderDebugData();
        g_pEventLoopManager->doLater([] {
            for (auto& m : g_pCompositor->m_monitors) {
                g_pHyprRenderer->damageMonitor(m);
            }
        });
    }

    g_pHyprOpenGL->m_RenderData.damage = damage;
    return damage;
}

void CRenderPass::renderDebugData() {
    CBox box = {{}, g_pHyprOpenGL->m_RenderData.pMonitor->m_transformedSize};
    for (const auto& rg : occludedRegions) {
        g_pHyprOpenGL->renderRectWithDamage(box, Colors::RED.modifyA(0.1F), rg);
    }
    g_pHyprOpenGL->renderRectWithDamage(box, Colors::GREEN.modifyA(0.1F), totalLiveBlurRegion);

    std::unordered_map<CWLSurfaceResource*, float> offsets;

    // render focus stuff
    auto renderHLSurface = [&offsets](SP<CTexture> texture, SP<CWLSurfaceResource> surface, const CHyprColor& color) {
        if (!surface || !texture)
            return;

        auto hlSurface = CWLSurface::fromResource(surface);
        if (!hlSurface)
            return;

        auto bb = hlSurface->getSurfaceBoxGlobal();

        if (!bb.has_value())
            return;

        CBox box = bb->copy().translate(-g_pHyprOpenGL->m_RenderData.pMonitor->m_position).scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);

        if (box.intersection(CBox{{}, g_pHyprOpenGL->m_RenderData.pMonitor->m_size}).empty())
            return;

        g_pHyprOpenGL->renderRectWithDamage(box, color, CRegion{0, 0, INT32_MAX, INT32_MAX});

        if (offsets.contains(surface.get()))
            box.translate(Vector2D{0.F, offsets[surface.get()]});
        else
            offsets[surface.get()] = 0;

        box = {box.pos(), texture->m_vSize};
        g_pHyprOpenGL->renderRectWithDamage(box, CHyprColor{0.F, 0.F, 0.F, 0.2F}, CRegion{0, 0, INT32_MAX, INT32_MAX}, std::min(5.0, box.size().y));
        g_pHyprOpenGL->renderTexture(texture, box, 1.F);

        offsets[surface.get()] += texture->m_vSize.y;
    };

    renderHLSurface(debugData.keyboardFocusText, g_pSeatManager->m_state.keyboardFocus.lock(), Colors::PURPLE.modifyA(0.1F));
    renderHLSurface(debugData.pointerFocusText, g_pSeatManager->m_state.pointerFocus.lock(), Colors::ORANGE.modifyA(0.1F));
    if (g_pCompositor->m_lastWindow)
        renderHLSurface(debugData.lastWindowText, g_pCompositor->m_lastWindow->m_wlSurface->resource(), Colors::LIGHT_BLUE.modifyA(0.1F));

    if (g_pSeatManager->m_state.pointerFocus) {
        if (g_pSeatManager->m_state.pointerFocus->current.input.intersect(CBox{{}, g_pSeatManager->m_state.pointerFocus->current.size}).getExtents().size() !=
            g_pSeatManager->m_state.pointerFocus->current.size) {
            auto hlSurface = CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
            if (hlSurface) {
                auto BOX = hlSurface->getSurfaceBoxGlobal();
                if (BOX) {
                    auto region = g_pSeatManager->m_state.pointerFocus->current.input.copy()
                                      .scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale)
                                      .translate(BOX->pos() - g_pHyprOpenGL->m_RenderData.pMonitor->m_position);
                    g_pHyprOpenGL->renderRectWithDamage(box, CHyprColor{0.8F, 0.8F, 0.2F, 0.4F}, region);
                }
            }
        }
    }

    const auto DISCARDED_ELEMENTS = std::count_if(m_vPassElements.begin(), m_vPassElements.end(), [](const auto& e) { return e->discard; });
    auto tex = g_pHyprOpenGL->renderText(std::format("occlusion layers: {}\npass elements: {} ({} discarded)\nviewport: {:X0}", occludedRegions.size(), m_vPassElements.size(),
                                                     DISCARDED_ELEMENTS, g_pHyprOpenGL->m_RenderData.pMonitor->m_pixelSize),
                                         Colors::WHITE, 12);

    if (tex) {
        box = CBox{{0.F, g_pHyprOpenGL->m_RenderData.pMonitor->m_size.y - tex->m_vSize.y}, tex->m_vSize}.scale(g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);
        g_pHyprOpenGL->renderTexture(tex, box, 1.F);
    }

    std::string passStructure;
    auto        yn   = [](const bool val) -> const char* { return val ? "yes" : "no"; };
    auto        tick = [](const bool val) -> const char* { return val ? "✔" : "✖"; };
    for (const auto& el : m_vPassElements | std::views::reverse) {
        passStructure += std::format("{} {} (bb: {} op: {})\n", tick(!el->discard), el->element->passName(), yn(el->element->boundingBox().has_value()),
                                     yn(!el->element->opaqueRegion().empty()));
    }

    if (!passStructure.empty())
        passStructure.pop_back();

    tex = g_pHyprOpenGL->renderText(passStructure, Colors::WHITE, 12);
    if (tex) {
        box = CBox{{g_pHyprOpenGL->m_RenderData.pMonitor->m_size.x - tex->m_vSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->m_size.y - tex->m_vSize.y}, tex->m_vSize}.scale(
            g_pHyprOpenGL->m_RenderData.pMonitor->m_scale);
        g_pHyprOpenGL->renderTexture(tex, box, 1.F);
    }
}

float CRenderPass::oneBlurRadius() {
    // TODO: is this exact range correct?
    static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    return *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES); // is this 2^pass? I don't know but it works... I think.
}

void CRenderPass::removeAllOfType(const std::string& type) {
    std::erase_if(m_vPassElements, [&type](const auto& e) { return e->element->passName() == type; });
}
