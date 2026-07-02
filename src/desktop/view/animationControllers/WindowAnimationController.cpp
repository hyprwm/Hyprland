#include "WindowAnimationController.hpp"

#include "../Window.hpp"
#include "../../../output/Monitor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <string>

#include <hyprutils/string/VarList.hpp>

using namespace Desktop::View;
using namespace Hyprutils::String;

static float percentageFromStyle(const std::string& style, bool ignoreLeadingPercentage = false) {
    float minPerc = 0.F;

    if (ignoreLeadingPercentage && style.starts_with("%"))
        return minPerc;

    if (style.find("%") == std::string::npos)
        return minPerc;

    try {
        auto percstr = style.substr(style.find_last_of(' '));
        minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
    } catch (std::exception& e) {
        ; // oops
    }

    return minPerc / 100.F;
}

static void applyPopin(Animation::SViewAnimationContext& ctx, const bool close, const float minPerc) {
    const auto GOALPOS  = ctx.pos.to;
    const auto GOALSIZE = ctx.size.to;
    const auto ENDSIZE  = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
    const auto ENDPOS   = GOALPOS + GOALSIZE / 2.F - ENDSIZE / 2.F;

    if (!close) {
        ctx.size.from = ENDSIZE;
        ctx.pos.from  = ENDPOS;
    } else {
        ctx.size.to = ENDSIZE;
        ctx.pos.to  = ENDPOS;
    }
}

static void applyGnomed(Animation::SViewAnimationContext& ctx, const bool close) {
    const auto GOALPOS  = ctx.pos.to;
    const auto GOALSIZE = ctx.size.to;
    const auto GNOMEPOS = GOALPOS + Vector2D{0.F, GOALSIZE.y / 2.F};
    const auto GNOMESZ  = Vector2D{GOALSIZE.x, 0.F};

    if (close) {
        ctx.pos.to  = GNOMEPOS;
        ctx.size.to = GNOMESZ;
    } else {
        ctx.pos.from  = GNOMEPOS;
        ctx.size.from = GNOMESZ;
    }
}

static void applySlide(Animation::SViewAnimationContext& ctx, CWindow* window, const std::string& force, const bool close) {
    const auto GOALPOS  = ctx.pos.to;
    const auto GOALSIZE = ctx.size.to;
    const auto PMONITOR = window->m_monitor.lock();

    ctx.size.from = GOALSIZE;
    ctx.size.to   = GOALSIZE;

    if (!PMONITOR)
        return;

    Vector2D posOffset;

    if (!force.empty()) {
        if (force == "bottom")
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y + PMONITOR->m_size.y);
        else if (force == "left")
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
        else if (force == "right")
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y - GOALSIZE.y);

        if (!close)
            ctx.pos.from = posOffset;
        else
            ctx.pos.to = posOffset;

        return;
    }

    const auto                 MIDPOINT = GOALPOS + GOALSIZE / 2.F;
    const auto                 MONBOX   = PMONITOR->logicalBox();

    const std::array<float, 4> distances = {
        MIDPOINT.y - MONBOX.y,            //
        MONBOX.x + MONBOX.w - MIDPOINT.x, //
        MONBOX.y + MONBOX.h - MIDPOINT.y, //
        MIDPOINT.x - MONBOX.x,            //
    };

    const auto MIN_DIST = std::min({distances[0], distances[1], distances[2], distances[3]});
    if (MIN_DIST == distances[2])
        posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y + PMONITOR->m_size.y);
    else if (MIN_DIST == distances[3])
        posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
    else if (MIN_DIST == distances[1])
        posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
    else
        posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y - GOALSIZE.y);

    if (!close)
        ctx.pos.from = posOffset;
    else
        ctx.pos.to = posOffset;
}

static void applyWindowStyle(Animation::SViewAnimationContext& ctx, CWindow* window, const bool close) {
    std::string animStyle = window->m_realPosition->getStyle();
    std::ranges::transform(animStyle, animStyle.begin(), ::tolower);

    CVarList animList(animStyle, 0, 's');

    if (window->m_ruleApplicator->animationStyle().hasValue()) {
        const auto STYLE = window->m_ruleApplicator->animationStyle().value();

        if (STYLE.starts_with("slide")) {
            CVarList animList2(STYLE, 0, 's');
            applySlide(ctx, window, animList2[1], close);
        } else if (STYLE == "gnomed" || STYLE == "gnome")
            applyGnomed(ctx, close);
        else
            applyPopin(ctx, close, percentageFromStyle(STYLE));

        return;
    }

    if (animList[0] == "slide")
        applySlide(ctx, window, animList[1], close);
    else if (animList[0] == "gnomed" || animList[0] == "gnome")
        applyGnomed(ctx, close);
    else
        applyPopin(ctx, close, percentageFromStyle(animStyle, true));
}

CWindowAnimationController::CWindowAnimationController(CWindow* parent) : m_parent(parent) {
    ;
}

Animation::SViewAnimationContext CWindowAnimationController::animateIn() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->m_realPosition->goal();
    ctx.pos.to    = m_parent->m_realPosition->goal();
    ctx.size.from = m_parent->m_realSize->goal();
    ctx.size.to   = m_parent->m_realSize->goal();
    ctx.alpha     = {.from = 0.F, .to = 1.F};

    // Do not apply movement anims to X11 ORs
    if (!m_parent->m_X11DoesntWantBorders)
        applyWindowStyle(ctx, m_parent, false);

    return ctx;
}

Animation::SViewAnimationContext CWindowAnimationController::animateOut() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->m_realPosition->value();
    ctx.pos.to    = m_parent->m_realPosition->goal();
    ctx.size.from = m_parent->m_realSize->value();
    ctx.size.to   = m_parent->m_realSize->goal();

    ctx.alpha = {.from = m_parent->alpha(WINDOW_ALPHA_FADE)->value(), .to = 0.F};

    // Do not apply movement anims to X11 ORs
    if (!m_parent->m_X11DoesntWantBorders)
        applyWindowStyle(ctx, m_parent, true);

    return ctx;
}

void CWindowAnimationController::apply(const Animation::SViewAnimationContext& ctx) const {
    m_parent->m_realPosition->setValueAndWarp(ctx.pos.from);
    m_parent->m_realSize->setValueAndWarp(ctx.size.from);
    m_parent->alpha(WINDOW_ALPHA_FADE)->setValueAndWarp(ctx.alpha.from);

    *m_parent->m_realPosition           = ctx.pos.to;
    *m_parent->m_realSize               = ctx.size.to;
    *m_parent->alpha(WINDOW_ALPHA_FADE) = ctx.alpha.to;
}
