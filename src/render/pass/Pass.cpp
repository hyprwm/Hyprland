#include "Pass.hpp"
#include "../OpenGL.hpp"
#include <algorithm>
#include <ranges>
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../protocols/core/Compositor.hpp"

bool CRenderPass::empty() const {
    return false;
}

bool CRenderPass::single() const {
    return m_passElements.size() == 1;
}

void CRenderPass::add(UP<IPassElement>&& el) {
    m_passElements.emplace_back(makeUnique<SPassElementData>(CRegion{}, std::move(el)));
}

void CRenderPass::simplify() {
    static auto PDEBUGPASS = CConfigValue<Hyprlang::INT>("debug:pass");

    // TODO: use precompute blur for instances where there is nothing in between

    // if there is live blur, we need to NOT occlude any area where it will be influenced
    const auto WILLBLUR = std::ranges::any_of(m_passElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    CRegion    newDamage = m_damage.copy().intersect(CBox{{}, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize});
    for (auto& el : m_passElements | std::views::reverse) {

        if (newDamage.empty() && !el->element->undiscardable()) {
            el->discard = true;
            continue;
        }

        el->elementDamage = newDamage;
        auto bb1          = el->element->boundingBox();
        if (!bb1 || newDamage.empty())
            continue;

        auto bb = bb1->scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale);

        // drop if empty
        if (CRegion copy = newDamage.copy(); copy.intersect(bb).empty()) {
            el->discard = true;
            continue;
        }

        auto opaque = el->element->opaqueRegion();

        if (!opaque.empty()) {
            opaque.scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale);

            // if this intersects the liveBlur region, allow live blur to operate correctly.
            // do not occlude a border near it.
            if (WILLBLUR) {
                CRegion liveBlurRegion;
                for (auto& el2 : m_passElements) {
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
                liveBlurRegion.scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale).expand(oneBlurRadius() * 2.F);

                if (auto infringement = opaque.copy().intersect(liveBlurRegion); !infringement.empty()) {
                    // eh, this is not the correct solution, but it will do...
                    // TODO: is this *easily* fixable?
                    opaque.subtract(infringement);
                }
            }
            newDamage.subtract(opaque);
            if (*PDEBUGPASS)
                m_occludedRegions.emplace_back(opaque);
        }
    }

    if (*PDEBUGPASS) {
        for (auto& el2 : m_passElements) {
            if (!el2->element->needsLiveBlur())
                continue;

            const auto BB = el2->element->boundingBox();
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            m_totalLiveBlurRegion.add(BB->copy().scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale));
        }
    }
}

void CRenderPass::clear() {
    m_passElements.clear();
}

CRegion CRenderPass::render(const CRegion& damage_) {
    static auto PDEBUGPASS = CConfigValue<Hyprlang::INT>("debug:pass");

    const auto  WILLBLUR = std::ranges::any_of(m_passElements, [](const auto& el) { return el->element->needsLiveBlur(); });

    m_damage = *PDEBUGPASS ? CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}} : damage_.copy();
    if (*PDEBUGPASS) {
        m_occludedRegions.clear();
        m_totalLiveBlurRegion = CRegion{};
    }

    if (m_damage.empty()) {
        g_pHyprOpenGL->m_renderData.damage      = m_damage;
        g_pHyprOpenGL->m_renderData.finalDamage = m_damage;
        return m_damage;
    }

    if (!*PDEBUGPASS && m_debugData.present)
        m_debugData = {false};
    else if (*PDEBUGPASS && !m_debugData.present) {
        m_debugData.present           = true;
        m_debugData.keyboardFocusText = g_pHyprOpenGL->renderText("keyboard", Colors::WHITE, 12);
        m_debugData.pointerFocusText  = g_pHyprOpenGL->renderText("pointer", Colors::WHITE, 12);
        m_debugData.lastWindowText    = g_pHyprOpenGL->renderText("lastWindow", Colors::WHITE, 12);
    }

    if (WILLBLUR && !*PDEBUGPASS) {
        // combine blur regions into one that will be expanded
        CRegion blurRegion;
        for (auto& el : m_passElements) {
            if (!el->element->needsLiveBlur())
                continue;

            const auto BB = el->element->boundingBox();
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            blurRegion.add(*BB);
        }

        blurRegion.scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale);

        blurRegion.intersect(m_damage).expand(oneBlurRadius());

        g_pHyprOpenGL->m_renderData.finalDamage = blurRegion.copy().add(m_damage);

        // FIXME: why does this break on * 1.F ?
        // used to work when we expand all the damage... I think? Well, before pass.
        // moving a window over blur shows the edges being wonk.
        blurRegion.expand(oneBlurRadius() * 1.5F);

        m_damage = blurRegion.copy().add(m_damage);
    } else
        g_pHyprOpenGL->m_renderData.finalDamage = m_damage;

    if (std::ranges::any_of(m_passElements, [](const auto& el) { return el->element->disableSimplification(); })) {
        for (auto& el : m_passElements) {
            el->elementDamage = m_damage;
        }
    } else
        simplify();

    g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFBShouldRender = std::ranges::any_of(m_passElements, [](const auto& el) { return el->element->needsPrecomputeBlur(); });

    if (m_passElements.empty())
        return {};

    for (auto& el : m_passElements) {
        if (el->discard) {
            el->element->discard();
            continue;
        }

        g_pHyprOpenGL->m_renderData.damage = el->elementDamage;
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

    g_pHyprOpenGL->m_renderData.damage = m_damage;
    return m_damage;
}

void CRenderPass::renderDebugData() {
    CBox box = {{}, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize};
    for (const auto& rg : m_occludedRegions) {
        g_pHyprOpenGL->renderRect(box, Colors::RED.modifyA(0.1F), {.damage = &rg});
    }
    g_pHyprOpenGL->renderRect(box, Colors::GREEN.modifyA(0.1F), {.damage = &m_totalLiveBlurRegion});

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

        CBox box = bb->copy().translate(-g_pHyprOpenGL->m_renderData.pMonitor->m_position).scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale);

        if (box.intersection(CBox{{}, g_pHyprOpenGL->m_renderData.pMonitor->m_size}).empty())
            return;

        static const auto FULL_REGION = CRegion{0, 0, INT32_MAX, INT32_MAX};

        g_pHyprOpenGL->renderRect(box, color, {.damage = &FULL_REGION});

        if (offsets.contains(surface.get()))
            box.translate(Vector2D{0.F, offsets[surface.get()]});
        else
            offsets[surface.get()] = 0;

        box = {box.pos(), texture->m_size};
        g_pHyprOpenGL->renderRect(box, CHyprColor{0.F, 0.F, 0.F, 0.2F}, {.damage = &FULL_REGION, .round = std::min(5.0, box.size().y)});
        g_pHyprOpenGL->renderTexture(texture, box, {});

        offsets[surface.get()] += texture->m_size.y;
    };

    renderHLSurface(m_debugData.keyboardFocusText, g_pSeatManager->m_state.keyboardFocus.lock(), Colors::PURPLE.modifyA(0.1F));
    renderHLSurface(m_debugData.pointerFocusText, g_pSeatManager->m_state.pointerFocus.lock(), Colors::ORANGE.modifyA(0.1F));
    if (Desktop::focusState()->window())
        renderHLSurface(m_debugData.lastWindowText, Desktop::focusState()->window()->m_wlSurface->resource(), Colors::LIGHT_BLUE.modifyA(0.1F));

    if (g_pSeatManager->m_state.pointerFocus) {
        if (g_pSeatManager->m_state.pointerFocus->m_current.input.intersect(CBox{{}, g_pSeatManager->m_state.pointerFocus->m_current.size}).getExtents().size() !=
            g_pSeatManager->m_state.pointerFocus->m_current.size) {
            auto hlSurface = CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
            if (hlSurface) {
                auto BOX = hlSurface->getSurfaceBoxGlobal();
                if (BOX) {
                    auto region = g_pSeatManager->m_state.pointerFocus->m_current.input.copy()
                                      .scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale)
                                      .translate(BOX->pos() - g_pHyprOpenGL->m_renderData.pMonitor->m_position);
                    g_pHyprOpenGL->renderRect(box, CHyprColor{0.8F, 0.8F, 0.2F, 0.4F}, {.damage = &region});
                }
            }
        }
    }

    const auto DISCARDED_ELEMENTS = std::ranges::count_if(m_passElements, [](const auto& e) { return e->discard; });
    auto tex = g_pHyprOpenGL->renderText(std::format("occlusion layers: {}\npass elements: {} ({} discarded)\nviewport: {:X0}", m_occludedRegions.size(), m_passElements.size(),
                                                     DISCARDED_ELEMENTS, g_pHyprOpenGL->m_renderData.pMonitor->m_pixelSize),
                                         Colors::WHITE, 12);

    if (tex) {
        box = CBox{{0.F, g_pHyprOpenGL->m_renderData.pMonitor->m_size.y - tex->m_size.y}, tex->m_size}.scale(g_pHyprOpenGL->m_renderData.pMonitor->m_scale);
        g_pHyprOpenGL->renderTexture(tex, box, {});
    }

    std::string passStructure;
    auto        yn   = [](const bool val) -> const char* { return val ? "yes" : "no"; };
    auto        tick = [](const bool val) -> const char* { return val ? "✔" : "✖"; };
    for (const auto& el : m_passElements | std::views::reverse) {
        passStructure += std::format("{} {} (bb: {} op: {}, pb: {}, lb: {})\n", tick(!el->discard), el->element->passName(), yn(el->element->boundingBox().has_value()),
                                     yn(!el->element->opaqueRegion().empty()), yn(el->element->needsPrecomputeBlur()), yn(el->element->needsLiveBlur()));
    }

    if (!passStructure.empty())
        passStructure.pop_back();

    tex = g_pHyprOpenGL->renderText(passStructure, Colors::WHITE, 12);
    if (tex) {
        box = CBox{{g_pHyprOpenGL->m_renderData.pMonitor->m_size.x - tex->m_size.x, g_pHyprOpenGL->m_renderData.pMonitor->m_size.y - tex->m_size.y}, tex->m_size}.scale(
            g_pHyprOpenGL->m_renderData.pMonitor->m_scale);
        g_pHyprOpenGL->renderTexture(tex, box, {});
    }
}

float CRenderPass::oneBlurRadius() {
    // TODO: is this exact range correct?
    static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");

    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    return std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES); // is this 2^pass? I don't know but it works... I think.
}

void CRenderPass::removeAllOfType(const std::string& type) {
    std::erase_if(m_passElements, [&type](const auto& e) { return e->element->passName() == type; });
}
