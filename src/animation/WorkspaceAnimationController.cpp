#include "WorkspaceAnimationController.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../workspace/HLWorkspace.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../layout/target/Target.hpp"
#include "../output/Monitor.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"

#include <hyprutils/string/VarList.hpp>

using namespace Hyprutils::String;
using namespace Desktop::View;

void Animation::Workspace::startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left, bool instant, std::optional<std::string> style) {
    if (!ws)
        return;

    const bool IN = type == ANIMATION_TYPE_IN;

    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", ws->type() == ::Workspace::eWorkspaceType::SPECIAL ? "specialWorkspace" : "workspaces", IN ? "In" : "Out");

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
            if (!validMapped(w) || w->presentation().workspace() != weak)
                continue;

            w->presentation().onWorkspaceAnimUpdate();
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
        } catch (std::exception& e) { LOG(Log::ERR, "Error in startAnim: invalid percentage"); }
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

    if (ws->type() == ::Workspace::eWorkspaceType::SPECIAL) {
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

static void setFullscreenLayerAlpha(PHLLS layer, float alpha) {
    if (!layer)
        return;

    static auto PANIMENABLED = CConfigValue<Config::INTEGER>("animations:enabled");
    const auto& ALPHA        = layer->alpha()[LS_ALPHA_FADE];

    if (!*PANIMENABLED || !ALPHA->enabled())
        ALPHA->setValueAndWarp(alpha);
    else
        *ALPHA = alpha;
}

static void applyFullscreenFadeState(PHLWORKSPACE ws, bool fullscreen, bool updateInput) {
    const auto TOPMOST_COVERING_FS_WINDOW = Fullscreen::controller()->getPresentedFullscreenWindow(ws);
    for (auto const& w : Desktop::windowState()->windows()) {
        if (updateInput && w->m_workspace == ws)
            w->updateFullscreenInputState();

        if (ws->m_fullscreenPresentation || w->presentation().workspace() != ws)
            continue;

        if (w->m_state & WINDOW_STATE_PINNED)
            continue;

        // If there are several covering FS windows layered ontop of each other, needed to ensure FS windows are not stuck being invisible below the topmost
        if (TOPMOST_COVERING_FS_WINDOW == w) {
            w->presentation().setFullscreenAlpha(1.F);
            continue;
        }

        if (!fullscreen)
            w->presentation().setFullscreenAlpha(1.F);
        else if (TOPMOST_COVERING_FS_WINDOW != w)
            w->presentation().setFullscreenAlpha(Fullscreen::controller()->isAllowedOverPresentedFullscreen(w) ? 1.F : 0.F);
    }

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR || ws->m_fullscreenPresentation)
        return;

    if (ws == PMONITOR->m_activeWorkspace || ws == PMONITOR->m_activeSpecialWorkspace) {
        const auto FS_MODE_INTERNAL = Fullscreen::controller()->getPresentedFullscreenModes(ws).internal;
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            // We have an FS window - if LAYER_FLAG_ABOVE_FULLSCREEN is set, the layer is allowed above fullscreen window (maximised too in any case)
            if (!(ls->m_flags & LAYER_FLAG_ABOVE_FULLSCREEN))
                setFullscreenLayerAlpha(ls.lock(), (fullscreen && FS_MODE_INTERNAL != Fullscreen::FSMODE_MAXIMIZED) ? 0.F : 1.F);
            else
                setFullscreenLayerAlpha(ls.lock(), 1.F);
        }
    }
}

void Animation::Workspace::setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type) {
    if (!ws)
        return;

    applyFullscreenFadeState(ws, type == ANIMATION_TYPE_IN, true);
}

void Animation::Workspace::applyFullscreenFade(PHLWORKSPACE ws) {
    if (!ws || ws->m_fullscreenPresentation)
        return;

    applyFullscreenFadeState(ws, !!Fullscreen::controller()->getPresentedFullscreenWindow(ws), false);
}

void Animation::Workspace::setFullscreenFloatingFade(PHLWINDOW pWindow, float fade) {
    if (!pWindow || !pWindow->isFloating())
        return;

    pWindow->presentation().setFullscreenAlpha(fade);
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

        if ((w->m_state & WINDOW_STATE_PINNED) || Fullscreen::controller()->isFullscreen(w))
            continue;

        w->presentation().setFullscreenAlpha(fade);
        w->updateFullscreenInputState();
    }

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR || ws->m_fullscreenPresentation)
        return;

    if (ws == PMONITOR->m_activeWorkspace || ws == PMONITOR->m_activeSpecialWorkspace) {
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            setFullscreenLayerAlpha(ls.lock(), fade);
        }
    }
}
