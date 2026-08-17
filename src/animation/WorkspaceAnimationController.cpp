#include "WorkspaceAnimationController.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../layout/target/Target.hpp"
#include "../output/Monitor.hpp"
#include "../output/WorkspaceTransition.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"

#include <hyprutils/string/VarList.hpp>

using namespace Hyprutils::String;
using namespace Desktop::View;

void Animation::Workspace::startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left, bool instant, std::optional<std::string> style) {
    if (!ws)
        return;

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR)
        return;

    auto&      transition = PMONITOR->m_workspaceTransition->ensure(ws);
    const bool IN         = type == ANIMATION_TYPE_IN;

    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", ws->m_isSpecialWorkspace ? "specialWorkspace" : "workspaces", IN ? "In" : "Out");

        transition.alpha->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIMNAME));
        transition.offset->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIMNAME));
    }

    static auto PWORKSPACEGAP = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = style.value_or(transition.alpha->getStyle());

    float       movePerc = 100.F;
    bool        vert     = ANIMSTYLE.starts_with("slidevert") || ANIMSTYLE.starts_with("slidefadevert");

    CVarList    args(ANIMSTYLE, 0, 's');
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
        transition.alpha->setValueAndWarp(1.F);
        transition.offset->setValueAndWarp(Vector2D(0, 0));

        if (vert) {
            if (IN) {
                transition.alpha->setValueAndWarp(0.F);
                transition.offset->setValueAndWarp(Vector2D(0.0, (left ? PMONITOR->m_size.y : -PMONITOR->m_size.y) * (movePerc / 100.F)));
                *transition.alpha  = 1.F;
                *transition.offset = Vector2D(0, 0);
            } else {
                transition.alpha->setValueAndWarp(1.F);
                *transition.alpha  = 0.F;
                *transition.offset = Vector2D(0.0, (left ? -PMONITOR->m_size.y : PMONITOR->m_size.y) * (movePerc / 100.F));
            }
        } else {
            if (IN) {
                transition.alpha->setValueAndWarp(0.F);
                transition.offset->setValueAndWarp(Vector2D((left ? PMONITOR->m_size.x : -PMONITOR->m_size.x) * (movePerc / 100.F), 0.0));
                *transition.alpha  = 1.F;
                *transition.offset = Vector2D(0, 0);
            } else {
                transition.alpha->setValueAndWarp(1.F);
                *transition.alpha  = 0.F;
                *transition.offset = Vector2D((left ? -PMONITOR->m_size.x : PMONITOR->m_size.x) * (movePerc / 100.F), 0.0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        transition.offset->setValueAndWarp(Vector2D(0, 0));

        if (IN) {
            transition.alpha->setValueAndWarp(0.F);
            *transition.alpha = 1.F;
        } else {
            transition.alpha->setValueAndWarp(1.F);
            *transition.alpha = 0.F;
        }
    } else if (vert) {
        const auto YDISTANCE = (PMONITOR->m_size.y + *PWORKSPACEGAP) * (movePerc / 100.F);
        transition.alpha->setValueAndWarp(1.F);

        if (IN) {
            transition.offset->setValueAndWarp(Vector2D(0.0, left ? YDISTANCE : -YDISTANCE));
            *transition.offset = Vector2D(0, 0);
        } else
            *transition.offset = Vector2D(0.0, left ? -YDISTANCE : YDISTANCE);
    } else {
        const auto XDISTANCE = (PMONITOR->m_size.x + *PWORKSPACEGAP) * (movePerc / 100.F);
        transition.alpha->setValueAndWarp(1.F);

        if (IN) {
            transition.offset->setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0.0));
            *transition.offset = Vector2D(0, 0);
        } else
            *transition.offset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0.0);
    }

    if (ws->m_isSpecialWorkspace) {
        if (IN) {
            transition.alpha->setValueAndWarp(0.F);
            *transition.alpha = 1.F;
        } else {
            transition.alpha->setValueAndWarp(1.F);
            *transition.alpha = 0.F;
        }
    }

    if (instant) {
        transition.offset->warp();
        transition.alpha->warp();
    }
}

void Animation::Workspace::setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type) {
    if (!ws)
        return;

    const auto FULLSCREEN                 = type == ANIMATION_TYPE_IN;
    const auto TOPMOST_COVERING_FS_WINDOW = Fullscreen::controller()->getFullscreenWindow(ws, true);
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != ws)
            continue;

        w->updateFullscreenInputState();

        if (w->m_state & WINDOW_STATE_PINNED)
            continue;

        // If there are several covering FS windows layered ontop of each other, needed to ensure FS windows are not stuck being invisible below the topmost
        if (TOPMOST_COVERING_FS_WINDOW == w) {
            *w->presentation().alpha(WINDOW_ALPHA_FULLSCREEN) = 1.F;
            continue;
        }

        if (!FULLSCREEN)
            *w->presentation().alpha(WINDOW_ALPHA_FULLSCREEN) = 1.F;
        else if (TOPMOST_COVERING_FS_WINDOW != w)
            *w->presentation().alpha(WINDOW_ALPHA_FULLSCREEN) = w->isAllowedOverFullscreen() ? 1.F : 0.F;
    }

    const auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR)
        return;

    if (ws->m_id == PMONITOR->activeWorkspaceID() || ws->m_id == PMONITOR->activeSpecialWorkspaceID()) {
        const auto FSWINDOW         = Fullscreen::controller()->getFullscreenWindow(ws, true);
        const auto FS_MODE_INTERNAL = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            // We have an FS window - LAYER_FLAG_ABOVE_FULLSCREEN must be correctly unset in this case
            if (!(ls->m_flags & LAYER_FLAG_ABOVE_FULLSCREEN))
                *ls->alpha()[LS_ALPHA_FADE] = FULLSCREEN && FS_MODE_INTERNAL != Fullscreen::FSMODE_MAXIMIZED ? 0.F : 1.F;
            else
                *ls->alpha()[LS_ALPHA_FADE] = 1.F;
        }
    }
}

void Animation::Workspace::setFullscreenFloatingFade(PHLWINDOW pWindow, float fade) {
    if (!pWindow || !pWindow->isFloating())
        return;

    *pWindow->presentation().alpha(WINDOW_ALPHA_FULLSCREEN) = fade;
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

        *w->presentation().alpha(WINDOW_ALPHA_FULLSCREEN) = fade;
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
