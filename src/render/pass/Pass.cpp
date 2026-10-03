#include "Pass.hpp"
#include "../SceneResources.hpp"
#include "../OpenGL.hpp"
#include <algorithm>
#include <ranges>
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/WLSurface.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../state/MonitorState.hpp"
#include "RectPassElement.hpp"
#include "BackdropScopePassElement.hpp"
#include "macros.hpp"

using namespace Render;

bool CRenderPass::empty() const {
    return false;
}

bool CRenderPass::single() const {
    return m_passElements.size() == 1;
}

bool CRenderPass::needsLiveBlur(CRenderContext& ctx) {
    return std::ranges::any_of(m_passElements, [&ctx](const auto& el) { return el.element->needsLiveBlur(ctx); });
}

bool CRenderPass::needsPrecomputeBlur(CRenderContext& ctx) {
    return std::ranges::any_of(m_passElements, [&ctx](const auto& el) { return el.element->needsPrecomputeBlur(ctx); });
}

void CRenderPass::add(UP<IPassElement>&& el) {
    m_passElements.emplace_back(SPassElementData{.element = std::move(el)});
}

void CRenderPass::simplify(CRenderContext& ctx, bool willBlur, const CRegion& liveBlurRegion) {
    const auto  pMonitor   = ctx.m_data.pMonitor;
    static auto PDEBUGPASS = CConfigValue<Config::INTEGER>("debug:pass");

    // TODO: use precompute blur for instances where there is nothing in between

    CRegion newDamage = m_damage.copy().intersect(CBox{{}, pMonitor->m_transformedSize});
    for (auto& el : m_passElements | std::views::reverse) {

        if (newDamage.empty() && !el.element->undiscardable(ctx)) {
            el.discard = true;
            continue;
        }

        auto bb1 = el.element->boundingBox(ctx);
        if (!bb1 || newDamage.empty()) {
            el.elementDamage = newDamage;
            continue;
        }

        auto bb = bb1->scale(pMonitor->m_scale);

        // drop if empty
        if (CRegion copy = newDamage.copy(); copy.intersect(bb).empty()) {
            el.discard = true;
            continue;
        }

        el.elementDamage = newDamage;

        auto opaque = el.element->opaqueRegion(ctx);

        if (!opaque.empty()) {
            // scale and rounding is very particular so we have to use CBoxes scale and round functions
            if (pixman_region32_n_rects(opaque.pixman()) == 1)
                opaque = opaque.getExtents().scale(pMonitor->m_scale).round();
            else {
                CRegion scaledRegion;
                opaque.forEachRect([&scaledRegion, pMonitor](const auto& RECT) {
                    scaledRegion.add(CBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1).scale(pMonitor->m_scale).round());
                });
                opaque = scaledRegion;
            }

            // if this intersects the liveBlur region, allow live blur to operate correctly.
            // do not occlude a border near it.
            if (willBlur) {
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
            if (!el2.element->needsLiveBlurCached)
                continue;

            const auto BB = el2.element->boundingBox(ctx);
            RASSERT(BB, "No bounding box for an element with live blur is illegal");

            m_totalLiveBlurRegion.add(BB->copy().scale(pMonitor->m_scale));
        }
    }
}

void CRenderPass::clear() {
    m_passElements.clear();
    m_damage.clear();
    m_occludedRegions.clear();
    m_totalLiveBlurRegion.clear();
}

void CRenderPass::planBackdropScopes(CRenderContext& ctx) {
    CBackdropScopePlanner planner;
    const CBox            bounds = {{}, ctx.m_data.pMonitor->m_transformedSize};

    for (auto& el : m_passElements) {
        if (el.element->type() != EK_BACKDROP_SCOPE) {
            if (!el.discard && el.element->needsLiveBlurCached)
                planner.addLiveBlur(el.elementDamage);
            continue;
        }

        const auto marker = sc<CBackdropScopePassElement*>(el.element.get());
        const auto scope  = marker->scope();
        RASSERT(scope, "Backdrop scope marker has no scope");

        if (marker->action() == CBackdropScopePassElement::eAction::BEGIN)
            planner.begin(scope);
        else
            planner.end(scope, bounds);
    }

    RASSERT(planner.empty(), "Unclosed backdrop scope marker");
}

CRegion CRenderPass::render(CRenderContext& ctx, const CRegion& damage_) {
    const auto  pMonitor   = ctx.m_data.pMonitor;
    static auto PDEBUGPASS = CConfigValue<Config::INTEGER>("debug:pass");

    // single pass: cache blur results and gather aggregate info
    bool    willBlur = false, willDisableSimplification = false, willPrecomputeBlur = false, requiresFullDamage = false, conditionalPreBlur = false;
    CRegion blurRegion;
    for (auto& el : m_passElements) {
        el.element->needsLiveBlurCached       = el.element->needsLiveBlur(ctx);
        el.element->needsPrecomputeBlurCached = el.element->needsPrecomputeBlur(ctx);

        if (el.element->needsLiveBlurCached) {
            willBlur      = true;
            const auto BB = el.element->boundingBox(ctx);
            RASSERT(BB, "No bounding box for an element with live blur is illegal");
            blurRegion.add(*BB);
        }

        if (el.element->needsPrecomputeBlurCached)
            willPrecomputeBlur = true;

        if (ctx.readOnlyEffects() && el.element->type() == EK_PRE_BLUR) {
            conditionalPreBlur = true;
            continue;
        }

        if (el.element->disableSimplification(ctx))
            willDisableSimplification = true;

        if (el.element->requiresFullDamage(ctx))
            requiresFullDamage = true;
    }

    if (conditionalPreBlur && willPrecomputeBlur) {
        willDisableSimplification = true;
        requiresFullDamage        = true;
    }

    m_damage = *PDEBUGPASS ? CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}} : requiresFullDamage ? CRegion{CBox{{}, pMonitor->m_transformedSize}} : damage_.copy();
    if (*PDEBUGPASS) {
        m_occludedRegions.clear();
        m_totalLiveBlurRegion = CRegion{};
    }

    if (m_damage.empty()) {
        ctx.m_data.damage      = m_damage;
        ctx.m_data.finalDamage = m_damage;
        return m_damage;
    }

    if (!*PDEBUGPASS && m_debugData.present)
        m_debugData = {false};
    else if (*PDEBUGPASS && !m_debugData.present) {
        m_debugData.present           = true;
        m_debugData.keyboardFocusText = g_pHyprRenderer->renderText("keyboard", Colors::WHITE, 12);
        m_debugData.pointerFocusText  = g_pHyprRenderer->renderText("pointer", Colors::WHITE, 12);
        m_debugData.lastWindowText    = g_pHyprRenderer->renderText("lastWindow", Colors::WHITE, 12);
    }

    // precompute the expanded live blur region for simplify() to use
    CRegion liveBlurRegion;
    if (willBlur && !*PDEBUGPASS) {
        blurRegion.scale(pMonitor->m_scale);

        // save a copy for simplify's occlusion test before we mutate for damage expansion
        liveBlurRegion = blurRegion.copy();
        g_pHyprRenderer->expandBlurDamage(liveBlurRegion, 2.F);

        blurRegion.intersect(m_damage);
        g_pHyprRenderer->expandBlurDamage(blurRegion);

        ctx.m_data.finalDamage = blurRegion.copy().add(m_damage);

        // FIXME: why does this break on * 1.F ?
        // used to work when we expand all the damage... I think? Well, before pass.
        // moving a window over blur shows the edges being wonk.
        g_pHyprRenderer->expandBlurDamage(blurRegion, 1.5F);

        m_damage = blurRegion.copy().add(m_damage);
    } else
        ctx.m_data.finalDamage = m_damage;

    if (ctx.m_data.noSimplify || willDisableSimplification) {
        for (auto& el : m_passElements) {
            el.elementDamage = m_damage;
        }
    } else
        simplify(ctx, willBlur, liveBlurRegion);

    planBackdropScopes(ctx);

    if (ctx.sceneResources())
        ctx.sceneResources()->setBlurQueued(willPrecomputeBlur);
    else if (ctx.m_data.pMonitor)
        ctx.m_data.pMonitor->m_blurFBShouldRender = willPrecomputeBlur;

    if (m_passElements.empty())
        return {};

    const bool providerIsAnimated = g_pHyprRenderer->blurProviderIsAnimated(ctx);
    CRegion    animatedBlurDamage;
    bool       usesPrecomputedBlur = false;

    for (auto& el : m_passElements) {
        if (el.discard) {
            el.element->discard(ctx);
            continue;
        }

        ctx.m_data.damage = el.elementDamage;
        g_pHyprRenderer->draw(ctx, el.element, el.elementDamage);

        if (!providerIsAnimated || (!el.element->needsLiveBlurCached && !el.element->needsPrecomputeBlurCached))
            continue;

        const auto BB = el.element->boundingBox(ctx);
        if (!BB)
            animatedBlurDamage.add(CBox{{}, pMonitor->m_transformedSize});
        else {
            auto box = BB->copy().scale(pMonitor->m_scale);
            ctx.m_data.renderModif.applyToBox(box);
            animatedBlurDamage.add(box);
        }

        usesPrecomputedBlur = usesPrecomputedBlur || el.element->needsPrecomputeBlurCached;
    }

    animatedBlurDamage.intersect(CBox{{}, pMonitor->m_transformedSize});
    g_pHyprRenderer->scheduleFrameForAnimatedBlur(ctx, animatedBlurDamage, usesPrecomputedBlur);

    if (*PDEBUGPASS) {
        renderDebugData(ctx);
        if (!ctx.readOnlyEffects())
            g_pEventLoopManager->doLater([] {
                for (auto& m : State::monitorState()->monitors()) {
                    g_pHyprRenderer->damageMonitor(m);
                }
            });
    }

    ctx.m_data.damage = m_damage;
    return m_damage;
}

void CRenderPass::renderDebugData(CRenderContext& ctx) {
    const auto pMonitor = ctx.m_data.pMonitor;
    CBox       box      = {{}, pMonitor->m_transformedSize};
    for (const auto& rg : m_occludedRegions) {
        g_pHyprRenderer->draw(ctx, CRectPassElement::SRectData{.box = box, .color = Colors::RED.modifyA(0.1F)}, rg);
    }

    g_pHyprRenderer->draw(ctx, CRectPassElement::SRectData{.box = box, .color = Colors::GREEN.modifyA(0.1F)}, m_totalLiveBlurRegion);

    std::unordered_map<CWLSurfaceResource*, float> offsets;

    // render focus stuff
    auto renderHLSurface = [&ctx, &offsets, pMonitor, this](SP<ITexture> texture, SP<CWLSurfaceResource> surface, const CHyprColor& color) {
        if (!surface || !texture)
            return;

        auto hlSurface = Desktop::View::CWLSurface::fromResource(surface);
        if (!hlSurface)
            return;

        auto bb = hlSurface->getSurfaceBoxGlobal();

        if (!bb.has_value())
            return;

        CBox box = bb->copy().translate(-pMonitor->m_position).scale(pMonitor->m_scale);

        if (box.intersection(CBox{{}, pMonitor->m_size}).empty())
            return;

        g_pHyprRenderer->draw(ctx, CRectPassElement::SRectData{.box = box, .color = color}, m_damage);

        if (offsets.contains(surface.get()))
            box.translate(Vector2D{0.F, offsets[surface.get()]});
        else
            offsets[surface.get()] = 0;

        box = {box.pos(), texture->m_size};

        g_pHyprRenderer->draw(ctx,
                              CRectPassElement::SRectData{
                                  .box   = box,
                                  .color = color,
                                  .round = std::min(5.0, box.size().y),
                              },
                              m_damage);

        g_pHyprRenderer->draw(ctx, CTexPassElement::SRenderData{.tex = texture, .box = box}, m_damage);

        offsets[surface.get()] += texture->m_size.y;
    };

    renderHLSurface(m_debugData.keyboardFocusText, g_pSeatManager->m_state.keyboardFocus.lock(), Colors::PURPLE.modifyA(0.1F));
    renderHLSurface(m_debugData.pointerFocusText, g_pSeatManager->m_state.pointerFocus.lock(), Colors::ORANGE.modifyA(0.1F));
    if (Desktop::focusState()->window())
        renderHLSurface(m_debugData.lastWindowText, Desktop::focusState()->window()->wlSurface()->resource(), Colors::LIGHT_BLUE.modifyA(0.1F));

    if (g_pSeatManager->m_state.pointerFocus) {
        if (g_pSeatManager->m_state.pointerFocus->m_current.effectiveInputRegion().getExtents().size() != g_pSeatManager->m_state.pointerFocus->m_current.size) {
            auto hlSurface = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
            if (hlSurface) {
                auto BOX = hlSurface->getSurfaceBoxGlobal();
                if (BOX) {
                    g_pHyprRenderer->draw(ctx, CRectPassElement::SRectData{.box = box, .color = CHyprColor{0.8F, 0.8F, 0.2F, 0.4F}}, m_damage);
                }
            }
        }
    }

    const auto DISCARDED_ELEMENTS = std::ranges::count_if(m_passElements, [](const auto& e) { return e.discard; });
    auto tex = g_pHyprRenderer->renderText(std::format("occlusion layers: {}\npass elements: {} ({} discarded)\nviewport: {:X0}", m_occludedRegions.size(), m_passElements.size(),
                                                       DISCARDED_ELEMENTS, pMonitor->m_transformedSize),
                                           Colors::WHITE, 12);

    if (tex)
        g_pHyprRenderer->draw(ctx,
                              CTexPassElement::SRenderData{
                                  .tex = tex,
                                  .box = CBox{{0.F, pMonitor->m_size.y - tex->m_size.y}, tex->m_size}.scale(pMonitor->m_scale),
                              },
                              m_damage);

    std::string passStructure;
    auto        yn   = [](const bool val) -> const char* { return val ? "yes" : "no"; };
    auto        tick = [](const bool val) -> const char* { return val ? "✔" : "✖"; };
    for (const auto& el : m_passElements | std::views::reverse) {
        passStructure += std::format("{} {} (bb: {} op: {}, pb: {}, lb: {})\n", tick(!el.discard), el.element->passName(), yn(el.element->boundingBox(ctx).has_value()),
                                     yn(!el.element->opaqueRegion(ctx).empty()), yn(el.element->needsPrecomputeBlurCached), yn(el.element->needsLiveBlurCached));
    }

    if (!passStructure.empty())
        passStructure.pop_back();

    tex = g_pHyprRenderer->renderText(passStructure, Colors::WHITE, 12);
    if (tex)
        g_pHyprRenderer->draw(ctx,
                              CTexPassElement::SRenderData{
                                  .tex = tex,
                                  .box = CBox{{pMonitor->m_size.x - tex->m_size.x, pMonitor->m_size.y - tex->m_size.y}, tex->m_size}.scale(pMonitor->m_scale),
                              },
                              m_damage);
}

void CRenderPass::removeAllOfType(const std::string& type) {
    std::erase_if(m_passElements, [&type](const auto& e) { return e.element->passName() == type; });
}
