#include "WorkspaceAnimationController.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/Window.hpp"
#include "../layout/target/Target.hpp"
#include "../output/Monitor.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"

#include <hyprutils/string/VarList.hpp>

using namespace Hyprutils::String;
using namespace Desktop::View;

void Animation::Workspace::startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left, bool instant, std::optional<std::string> style) {
    if (!ws)
        return;

    const bool IN = type == ANIMATION_TYPE_IN;

    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", ws->m_isSpecialWorkspace ? "specialWorkspace" : "workspaces", IN ? "In" : "Out");

        ws->m_alpha->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIMNAME));
        ws->m_renderOffset->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIMNAME));
    }

    static auto PWORKSPACEGAP = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const auto  PMONITOR      = ws->m_monitor.lock();
    const auto  ANIMSTYLE     = style.value_or(ws->m_alpha->getStyle());

    if (!PMONITOR)
        return;

    float movePerc = 100.F;
    bool  vert     = ANIMSTYLE.starts_with("slidevert") || ANIMSTYLE.starts_with("slidefadevert");

    ws->m_renderOffset->setUpdateCallback([weak = PHLWORKSPACEREF{ws}](auto) {
        if (!weak)
            return;

        for (auto const& w : Desktop::windowState()->windows()) {
            if (!validMapped(w) || w->workspaceID() != weak->m_id)
                continue;

            w->onWorkspaceAnimUpdate();
        };
    });

    CVarList args(ANIMSTYLE, 0, 's');
    if (args.size() > 1) {
        const auto ARG2 = args[1];
        if (ARG2 == "top") {
            left = false;
            vert = true;
        } else if (ARG2 == "bottom") {
            left = true;
            vert = true;
        } else if (ARG2 == "left") {
            left = false;
            vert = false;
        } else if (ARG2 == "right") {
            left = true;
            vert = false;
        }
    }

    const auto percstr = args[args.size() - 1];
    if (percstr.ends_with('%')) {
        try {
            movePerc = std::stoi(percstr.substr(0, percstr.length() - 1));
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Error in startAnim: invalid percentage"); }
    }

    if (ANIMSTYLE.starts_with("slidefade")) {
        ws->m_alpha->setValueAndWarp(1.F);
        ws->m_renderOffset->setValueAndWarp(Vector2D(0, 0));

        if (vert) {
            if (IN) {
                ws->m_alpha->setValueAndWarp(0.F);
                ws->m_renderOffset->setValueAndWarp(Vector2D(0.0, (left ? PMONITOR->m_size.y : -PMONITOR->m_size.y) * (movePerc / 100.F)));
                *ws->m_alpha        = 1.F;
                *ws->m_renderOffset = Vector2D(0, 0);
            } else {
                ws->m_alpha->setValueAndWarp(1.F);
                *ws->m_alpha        = 0.F;
                *ws->m_renderOffset = Vector2D(0.0, (left ? -PMONITOR->m_size.y : PMONITOR->m_size.y) * (movePerc / 100.F));
            }
        } else {
            if (IN) {
                ws->m_alpha->setValueAndWarp(0.F);
                ws->m_renderOffset->setValueAndWarp(Vector2D((left ? PMONITOR->m_size.x : -PMONITOR->m_size.x) * (movePerc / 100.F), 0.0));
                *ws->m_alpha        = 1.F;
                *ws->m_renderOffset = Vector2D(0, 0);
            } else {
                ws->m_alpha->setValueAndWarp(1.F);
                *ws->m_alpha        = 0.F;
                *ws->m_renderOffset = Vector2D((left ? -PMONITOR->m_size.x : PMONITOR->m_size.x) * (movePerc / 100.F), 0.0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        ws->m_renderOffset->setValueAndWarp(Vector2D(0, 0));

        if (IN) {
            ws->m_alpha->setValueAndWarp(0.F);
            *ws->m_alpha = 1.F;
        } else {
            ws->m_alpha->setValueAndWarp(1.F);
            *ws->m_alpha = 0.F;
        }
    } else if (vert) {
        const auto YDISTANCE = (PMONITOR->m_size.y + *PWORKSPACEGAP) * (movePerc / 100.F);
        ws->m_alpha->setValueAndWarp(1.F);

        if (IN) {
            ws->m_renderOffset->setValueAndWarp(Vector2D(0.0, left ? YDISTANCE : -YDISTANCE));
            *ws->m_renderOffset = Vector2D(0, 0);
        } else
            *ws->m_renderOffset = Vector2D(0.0, left ? -YDISTANCE : YDISTANCE);
    } else {
        const auto XDISTANCE = (PMONITOR->m_size.x + *PWORKSPACEGAP) * (movePerc / 100.F);
        ws->m_alpha->setValueAndWarp(1.F);

        if (IN) {
            ws->m_renderOffset->setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0.0));
            *ws->m_renderOffset = Vector2D(0, 0);
        } else
            *ws->m_renderOffset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0.0);
    }

    if (ws->m_isSpecialWorkspace) {
        if (IN) {
            ws->m_alpha->setValueAndWarp(0.F);
            *ws->m_alpha = 1.F;
        } else {
            ws->m_alpha->setValueAndWarp(1.F);
            *ws->m_alpha = 0.F;
        }
    }

    if (instant) {
        ws->m_renderOffset->warp();
        ws->m_alpha->warp();
    }
}

void Animation::Workspace::setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type) {
    if (!ws)
        return;

    const auto FULLSCREEN = type == ANIMATION_TYPE_IN;

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != ws)
            continue;

        w->updateFullscreenInputState();

        if (w->m_pinned || w->isFullscreen())
            continue;

        if (!FULLSCREEN)
            *w->alpha(WINDOW_ALPHA_FULLSCREEN) = 1.F;
        else if (!w->isFullscreen())
            *w->alpha(WINDOW_ALPHA_FULLSCREEN) = w->isAllowedOverFullscreen() ? 1.F : 0.F;
    }

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR)
        return;

    if (ws->m_id == PMONITOR->activeWorkspaceID() || ws->m_id == PMONITOR->activeSpecialWorkspaceID()) {
        const auto FSWINDOW = ws->getFullscreenWindow(true);
        const auto FSMODE   = FSWINDOW ? FSWINDOW->m_target->fullscreenMode() : ws->m_fullscreenMode;
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->m_aboveFullscreen)
                *ls->alpha()[LS_ALPHA_FADE] = FULLSCREEN && FSMODE != FSMODE_MAXIMIZED ? 0.F : 1.F;
        }
    }
}

void Animation::Workspace::setFullscreenFloatingFade(PHLWINDOW pWindow, float fade) {
    if (!pWindow || !pWindow->m_isFloating)
        return;

    *pWindow->alpha(WINDOW_ALPHA_FULLSCREEN) = fade;
    pWindow->updateFullscreenInputState();
}

void Animation::Workspace::overrideFullscreenFadeAmount(PHLWORKSPACE ws, float fade, PHLWINDOW exclude) {
    if (!ws)
        return;

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w == exclude)
            continue;

        if (w->m_workspace != ws)
            continue;

        if (w->m_pinned || w->isFullscreen())
            continue;

        *w->alpha(WINDOW_ALPHA_FULLSCREEN) = fade;
        w->updateFullscreenInputState();
    }

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR)
        return;

    if (ws->m_id == PMONITOR->activeWorkspaceID() || ws->m_id == PMONITOR->activeSpecialWorkspaceID()) {
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            *ls->alpha()[LS_ALPHA_FADE] = fade;
        }
    }
}
