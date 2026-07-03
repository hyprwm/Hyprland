#include "LayerSurfaceAnimationController.hpp"

#include "../LayerSurface.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../state/MonitorState.hpp"

#include <array>
#include <exception>
#include <limits>
#include <string>

#include <hyprutils/string/VarList.hpp>

using namespace Desktop::View;
using namespace Hyprutils::String;

static float percentageFromLayerStyle(const std::string& style) {
    float minPerc = 0.F;

    if (style.find("%") == std::string::npos)
        return minPerc;

    try {
        auto percstr = style.substr(style.find_last_of(' '));
        minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
    } catch (std::exception& e) {
        ; // oops
    }

    return minPerc * 0.01F;
}

static int forcedEdgeFromStyle(const std::string& style) {
    CVarList args(style, 0, 's');

    if (args.size() <= 1)
        return -1;

    const auto ARG2 = args[1];
    if (ARG2 == "top")
        return 0;
    if (ARG2 == "bottom")
        return 1;
    if (ARG2 == "left")
        return 2;
    if (ARG2 == "right")
        return 3;

    return -1;
}

static void applySlide(Animation::SViewAnimationContext& ctx, CLayerSurface* layer, const std::string& style, const bool in) {
    const auto MIDDLE   = layer->m_geometry.middle();
    const auto PMONITOR = State::monitorState()->query().vec(MIDDLE).run();

    if (!PMONITOR) {
        ctx.alpha = {.from = in ? 1.F : 0.F, .to = in ? 1.F : 0.F};
        return;
    }

    const std::array<Vector2D, 4> edgePoints = {
        PMONITOR->m_position + Vector2D{PMONITOR->m_size.x / 2, 0.0},
        PMONITOR->m_position + Vector2D{PMONITOR->m_size.x / 2, PMONITOR->m_size.y},
        PMONITOR->m_position + Vector2D{0.0, PMONITOR->m_size.y},
        PMONITOR->m_position + Vector2D{PMONITOR->m_size.x, PMONITOR->m_size.y / 2},
    };

    float closest = std::numeric_limits<float>::max();
    int   leader  = forcedEdgeFromStyle(style);
    if (leader == -1) {
        for (size_t i = 0; i < 4; ++i) {
            float dist = MIDDLE.distance(edgePoints[i]);
            if (dist < closest) {
                leader  = i;
                closest = dist;
            }
        }
    }

    Vector2D prePos;
    switch (leader) {
        case 0: prePos = {layer->m_geometry.x, PMONITOR->m_position.y - layer->m_geometry.h}; break;
        case 1: prePos = {layer->m_geometry.x, PMONITOR->m_position.y + PMONITOR->m_size.y}; break;
        case 2: prePos = {PMONITOR->m_position.x - layer->m_geometry.w, layer->m_geometry.y}; break;
        case 3: prePos = {PMONITOR->m_position.x + PMONITOR->m_size.x, layer->m_geometry.y}; break;
        default: return;
    }

    if (in)
        ctx.pos.from = prePos;
    else
        ctx.pos.to = prePos;
}

static void applyPopin(Animation::SViewAnimationContext& ctx, CLayerSurface* layer, const std::string& style, const bool in) {
    const auto MINPERC  = percentageFromLayerStyle(style);
    const auto GOALSIZE = (layer->m_geometry.size() * MINPERC).clamp({5, 5});
    const auto GOALPOS  = layer->m_geometry.pos() + (layer->m_geometry.size() - GOALSIZE) / 2.F;

    ctx.alpha = {.from = in ? 0.F : 1.F, .to = in ? 1.F : 0.F};

    if (in) {
        ctx.size.from = GOALSIZE;
        ctx.pos.from  = GOALPOS;
    } else {
        ctx.size.to = GOALSIZE;
        ctx.pos.to  = GOALPOS;
    }
}

CLayerSurfaceAnimationController::CLayerSurfaceAnimationController(CLayerSurface* parent) : m_parent(parent) {
    ;
}

Animation::SViewAnimationContext CLayerSurfaceAnimationController::animateIn() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->m_geometry.pos();
    ctx.pos.to    = m_parent->m_geometry.pos();
    ctx.size.from = m_parent->m_geometry.size();
    ctx.size.to   = m_parent->m_geometry.size();
    ctx.alpha     = {.from = 0.F, .to = 1.F};

    const auto ANIMSTYLE = m_parent->m_ruleApplicator->animationStyle().valueOr(m_parent->m_realPosition->getStyle());
    if (ANIMSTYLE.starts_with("slide"))
        applySlide(ctx, m_parent, ANIMSTYLE, true);
    else if (ANIMSTYLE.starts_with("popin"))
        applyPopin(ctx, m_parent, ANIMSTYLE, true);

    return ctx;
}

Animation::SViewAnimationContext CLayerSurfaceAnimationController::animateOut() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->m_geometry.pos();
    ctx.pos.to    = m_parent->m_geometry.pos();
    ctx.size.from = m_parent->m_geometry.size();
    ctx.size.to   = m_parent->m_geometry.size();
    ctx.alpha     = {.from = m_parent->alpha()[LS_ALPHA_FADE]->value(), .to = 0.F};

    const auto ANIMSTYLE = m_parent->m_ruleApplicator->animationStyle().valueOr(m_parent->m_realPosition->getStyle());
    if (ANIMSTYLE.starts_with("slide"))
        applySlide(ctx, m_parent, ANIMSTYLE, false);
    else if (ANIMSTYLE.starts_with("popin"))
        applyPopin(ctx, m_parent, ANIMSTYLE, false);

    return ctx;
}

void CLayerSurfaceAnimationController::apply(const Animation::SViewAnimationContext& ctx) const {
    m_parent->m_realPosition->setValueAndWarp(ctx.pos.from);
    m_parent->m_realSize->setValueAndWarp(ctx.size.from);
    m_parent->alpha()[LS_ALPHA_FADE]->setValueAndWarp(ctx.alpha.from);

    *m_parent->m_realPosition         = ctx.pos.to;
    *m_parent->m_realSize             = ctx.size.to;
    *m_parent->alpha()[LS_ALPHA_FADE] = ctx.alpha.to;
}
