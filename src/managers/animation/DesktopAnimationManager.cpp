#include "DesktopAnimationManager.hpp"

#include "../../desktop/LayerSurface.hpp"
#include "../../desktop/Window.hpp"
#include "../../desktop/Workspace.hpp"

#include "../../config/ConfigManager.hpp"
#include "../../Compositor.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"

void CDesktopAnimationManager::startAnimation(PHLWINDOW pWindow, eAnimationType type, bool force) {
    const bool CLOSE = type == ANIMATION_TYPE_OUT;

    if (CLOSE)
        *pWindow->m_alpha = 0.F;
    else {
        pWindow->m_alpha->setValueAndWarp(0.F);
        *pWindow->m_alpha = 1.F;
    }

    if (!CLOSE) {
        pWindow->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsIn"));
        pWindow->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));
    } else {
        pWindow->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsOut"));
        pWindow->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
    }

    std::string ANIMSTYLE = pWindow->m_realPosition->getStyle();
    std::ranges::transform(ANIMSTYLE, ANIMSTYLE.begin(), ::tolower);

    CVarList animList(ANIMSTYLE, 0, 's');

    // if the window is not being animated, that means the layout set a fixed size for it, don't animate.
    if (!pWindow->m_realPosition->isBeingAnimated() && !pWindow->m_realSize->isBeingAnimated() && !force)
        return;

    // if the animation is disabled and we are leaving, ignore the anim to prevent the snapshot being fucked
    if (!pWindow->m_realPosition->enabled() && !force)
        return;

    if (pWindow->m_windowData.animationStyle.hasValue()) {
        const auto STYLE = pWindow->m_windowData.animationStyle.value();
        // the window has config'd special anim
        if (STYLE.starts_with("slide")) {
            CVarList animList2(STYLE, 0, 's');
            animationSlide(pWindow, animList2[1], CLOSE);
        } else if (STYLE == "gnomed" || STYLE == "gnome")
            animationGnomed(pWindow, CLOSE);
        else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (STYLE.find("%") != std::string::npos) {
                try {
                    auto percstr = STYLE.substr(STYLE.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, CLOSE, minPerc / 100.f);
        }
    } else {
        if (animList[0] == "slide")
            animationSlide(pWindow, animList[1], CLOSE);
        else if (animList[0] == "gnomed" || animList[0] == "gnome")
            animationGnomed(pWindow, CLOSE);
        else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (!ANIMSTYLE.starts_with("%")) {
                try {
                    auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, CLOSE, minPerc / 100.f);
        }
    }
}

void CDesktopAnimationManager::startAnimation(PHLLS ls, eAnimationType type, bool instant) {
    const bool IN = type == ANIMATION_TYPE_IN;

    if (IN) {
        ls->m_alpha->setValueAndWarp(0.F);
        *ls->m_alpha = 1.F;
    } else
        *ls->m_alpha = 0.F;

    if (IN) {
        ls->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersIn"));
        ls->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersIn"));
        ls->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeLayersIn"));
    } else {
        ls->m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersOut"));
        ls->m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersOut"));
        ls->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeLayersOut"));
    }

    const auto ANIMSTYLE = ls->m_animationStyle.value_or(ls->m_realPosition->getStyle());
    if (ANIMSTYLE.starts_with("slide")) {
        // get closest edge
        const auto MIDDLE = ls->m_geometry.middle();

        const auto PMONITOR = g_pCompositor->getMonitorFromVector(MIDDLE);

        if (!PMONITOR) { // can rarely happen on exit
            ls->m_alpha->setValueAndWarp(IN ? 1.F : 0.F);
            return;
        }

        int      force = -1;

        CVarList args(ANIMSTYLE, 0, 's');
        if (args.size() > 1) {
            const auto ARG2 = args[1];
            if (ARG2 == "top")
                force = 0;
            else if (ARG2 == "bottom")
                force = 1;
            else if (ARG2 == "left")
                force = 2;
            else if (ARG2 == "right")
                force = 3;
        }

        const std::array<Vector2D, 4> edgePoints = {
            PMONITOR->m_position + Vector2D{PMONITOR->m_size.x / 2, 0.0},
            PMONITOR->m_position + Vector2D{PMONITOR->m_size.x / 2, PMONITOR->m_size.y},
            PMONITOR->m_position + Vector2D{0.0, PMONITOR->m_size.y},
            PMONITOR->m_position + Vector2D{PMONITOR->m_size.x, PMONITOR->m_size.y / 2},
        };

        float closest = std::numeric_limits<float>::max();
        int   leader  = force;
        if (leader == -1) {
            for (size_t i = 0; i < 4; ++i) {
                float dist = MIDDLE.distance(edgePoints[i]);
                if (dist < closest) {
                    leader  = i;
                    closest = dist;
                }
            }
        }

        ls->m_realSize->setValueAndWarp(ls->m_geometry.size());

        Vector2D prePos;

        switch (leader) {
            case 0:
                // TOP
                prePos = {ls->m_geometry.x, PMONITOR->m_position.y - ls->m_geometry.h};
                break;
            case 1:
                // BOTTOM
                prePos = {ls->m_geometry.x, PMONITOR->m_position.y + PMONITOR->m_size.y};
                break;
            case 2:
                // LEFT
                prePos = {PMONITOR->m_position.x - ls->m_geometry.w, ls->m_geometry.y};
                break;
            case 3:
                // RIGHT
                prePos = {PMONITOR->m_position.x + PMONITOR->m_size.x, ls->m_geometry.y};
                break;
            default: UNREACHABLE();
        }

        if (IN) {
            ls->m_realPosition->setValueAndWarp(prePos);
            *ls->m_realPosition = ls->m_geometry.pos();
        } else {
            ls->m_realPosition->setValueAndWarp(ls->m_geometry.pos());
            *ls->m_realPosition = prePos;
        }

    } else if (ANIMSTYLE.starts_with("popin")) {
        float minPerc = 0.f;
        if (ANIMSTYLE.find("%") != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) {
                ; // oops
            }
        }

        minPerc *= 0.01;

        const auto GOALSIZE = (ls->m_geometry.size() * minPerc).clamp({5, 5});
        const auto GOALPOS  = ls->m_geometry.pos() + (ls->m_geometry.size() - GOALSIZE) / 2.f;

        ls->m_alpha->setValueAndWarp(IN ? 0.f : 1.f);
        *ls->m_alpha = IN ? 1.f : 0.f;

        if (IN) {
            ls->m_realSize->setValueAndWarp(GOALSIZE);
            ls->m_realPosition->setValueAndWarp(GOALPOS);
            *ls->m_realSize     = ls->m_geometry.size();
            *ls->m_realPosition = ls->m_geometry.pos();
        } else {
            ls->m_realSize->setValueAndWarp(ls->m_geometry.size());
            ls->m_realPosition->setValueAndWarp(ls->m_geometry.pos());
            *ls->m_realSize     = GOALSIZE;
            *ls->m_realPosition = GOALPOS;
        }
    } else {
        // fade
        ls->m_realPosition->setValueAndWarp(ls->m_geometry.pos());
        ls->m_realSize->setValueAndWarp(ls->m_geometry.size());
        *ls->m_alpha = IN ? 1.f : 0.f;
    }

    if (instant) {
        ls->m_realPosition->warp();
        ls->m_realSize->warp();
        ls->m_alpha->warp();
    }
}

void CDesktopAnimationManager::startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left, bool instant) {
    const bool IN = type == ANIMATION_TYPE_IN;

    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", ws->m_isSpecialWorkspace ? "specialWorkspace" : "workspaces", IN ? "In" : "Out");

        ws->m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
        ws->m_renderOffset->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
    }

    const auto  ANIMSTYLE     = ws->m_alpha->getStyle();
    static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");

    // set floating windows offset callbacks
    ws->m_renderOffset->setUpdateCallback([&](auto) {
        for (auto const& w : g_pCompositor->m_windows) {
            if (!validMapped(w) || w->workspaceID() != ws->m_id)
                continue;

            w->onWorkspaceAnimUpdate();
        };
    });

    if (ANIMSTYLE.starts_with("slidefade")) {
        const auto PMONITOR = ws->m_monitor.lock();
        float      movePerc = 100.f;

        if (ANIMSTYLE.find('%') != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' ') + 1);
                movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) { Debug::log(ERR, "Error in startAnim: invalid percentage"); }
        }

        ws->m_alpha->setValueAndWarp(1.f);
        ws->m_renderOffset->setValueAndWarp(Vector2D(0, 0));

        if (ANIMSTYLE.starts_with("slidefadevert")) {
            if (IN) {
                ws->m_alpha->setValueAndWarp(0.f);
                ws->m_renderOffset->setValueAndWarp(Vector2D(0.0, (left ? PMONITOR->m_size.y : -PMONITOR->m_size.y) * (movePerc / 100.f)));
                *ws->m_alpha        = 1.f;
                *ws->m_renderOffset = Vector2D(0, 0);
            } else {
                ws->m_alpha->setValueAndWarp(1.f);
                *ws->m_alpha        = 0.f;
                *ws->m_renderOffset = Vector2D(0.0, (left ? -PMONITOR->m_size.y : PMONITOR->m_size.y) * (movePerc / 100.f));
            }
        } else {
            if (IN) {
                ws->m_alpha->setValueAndWarp(0.f);
                ws->m_renderOffset->setValueAndWarp(Vector2D((left ? PMONITOR->m_size.x : -PMONITOR->m_size.x) * (movePerc / 100.f), 0.0));
                *ws->m_alpha        = 1.f;
                *ws->m_renderOffset = Vector2D(0, 0);
            } else {
                ws->m_alpha->setValueAndWarp(1.f);
                *ws->m_alpha        = 0.f;
                *ws->m_renderOffset = Vector2D((left ? -PMONITOR->m_size.x : PMONITOR->m_size.x) * (movePerc / 100.f), 0.0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        ws->m_renderOffset->setValueAndWarp(Vector2D(0, 0)); // fix a bug, if switching from slide -> fade.

        if (IN) {
            ws->m_alpha->setValueAndWarp(0.f);
            *ws->m_alpha = 1.f;
        } else {
            ws->m_alpha->setValueAndWarp(1.f);
            *ws->m_alpha = 0.f;
        }
    } else if (ANIMSTYLE == "slidevert") {
        // fallback is slide
        const auto PMONITOR  = ws->m_monitor.lock();
        const auto YDISTANCE = PMONITOR->m_size.y + *PWORKSPACEGAP;

        ws->m_alpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (IN) {
            ws->m_renderOffset->setValueAndWarp(Vector2D(0.0, left ? YDISTANCE : -YDISTANCE));
            *ws->m_renderOffset = Vector2D(0, 0);
        } else {
            *ws->m_renderOffset = Vector2D(0.0, left ? -YDISTANCE : YDISTANCE);
        }
    } else {
        // fallback is slide
        const auto PMONITOR  = ws->m_monitor.lock();
        const auto XDISTANCE = PMONITOR->m_size.x + *PWORKSPACEGAP;

        ws->m_alpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (IN) {
            ws->m_renderOffset->setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0.0));
            *ws->m_renderOffset = Vector2D(0, 0);
        } else {
            *ws->m_renderOffset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0.0);
        }
    }

    if (ws->m_isSpecialWorkspace) {
        // required for open/close animations
        if (IN) {
            ws->m_alpha->setValueAndWarp(0.f);
            *ws->m_alpha = 1.f;
        } else {
            ws->m_alpha->setValueAndWarp(1.f);
            *ws->m_alpha = 0.f;
        }
    }

    if (instant) {
        ws->m_renderOffset->warp();
        ws->m_alpha->warp();
    }
}

void CDesktopAnimationManager::animationPopin(PHLWINDOW pWindow, bool close, float minPerc) {
    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    if (!close) {
        pWindow->m_realSize->setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_realPosition->setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_realSize->value() / 2.f);
    } else {
        *pWindow->m_realSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        *pWindow->m_realPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_realSize->goal() / 2.f;
    }
}

void CDesktopAnimationManager::animationSlide(PHLWINDOW pWindow, std::string force, bool close) {
    pWindow->m_realSize->warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR)
        return; // unsafe state most likely

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
            pWindow->m_realPosition->setValue(posOffset);
        else
            *pWindow->m_realPosition = posOffset;

        return;
    }

    const auto MIDPOINT = GOALPOS + GOALSIZE / 2.f;

    // check sides it touches
    const bool DISPLAYLEFT   = STICKS(pWindow->m_position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pWindow->m_position.x + pWindow->m_size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pWindow->m_position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pWindow->m_position.y + pWindow->m_size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    if (DISPLAYBOTTOM && DISPLAYTOP) {
        if (DISPLAYLEFT && DISPLAYRIGHT) {
            posOffset = GOALPOS + Vector2D(0.0, GOALSIZE.y);
        } else if (DISPLAYLEFT) {
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0.0);
        } else {
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0.0);
        }
    } else if (DISPLAYTOP) {
        posOffset = GOALPOS - Vector2D(0.0, GOALSIZE.y);
    } else if (DISPLAYBOTTOM) {
        posOffset = GOALPOS + Vector2D(0.0, GOALSIZE.y);
    } else {
        if (MIDPOINT.y > PMONITOR->m_position.y + PMONITOR->m_size.y / 2.f)
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y + PMONITOR->m_size.y);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->m_position.y - GOALSIZE.y);
    }

    if (!close)
        pWindow->m_realPosition->setValue(posOffset);
    else
        *pWindow->m_realPosition = posOffset;
}

void CDesktopAnimationManager::animationGnomed(PHLWINDOW pWindow, bool close) {
    const auto GOALPOS  = pWindow->m_realPosition->goal();
    const auto GOALSIZE = pWindow->m_realSize->goal();

    if (close) {
        *pWindow->m_realPosition = GOALPOS + Vector2D{0.F, GOALSIZE.y / 2.F};
        *pWindow->m_realSize     = Vector2D{GOALSIZE.x, 0.F};
    } else {
        pWindow->m_realPosition->setValueAndWarp(GOALPOS + Vector2D{0.F, GOALSIZE.y / 2.F});
        pWindow->m_realSize->setValueAndWarp(Vector2D{GOALSIZE.x, 0.F});
        *pWindow->m_realPosition = GOALPOS;
        *pWindow->m_realSize     = GOALSIZE;
    }
}

void CDesktopAnimationManager::setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type) {
    if (!ws)
        return;

    const auto FULLSCREEN = type == ANIMATION_TYPE_IN;

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == ws) {

            if (w->m_fadingOut || w->m_pinned || w->isFullscreen())
                continue;

            if (!FULLSCREEN)
                *w->m_alpha = 1.F;
            else if (!w->isFullscreen())
                *w->m_alpha = !w->m_createdOverFullscreen ? 0.f : 1.f;
        }
    }

    const auto PMONITOR = ws->m_monitor.lock();

    if (ws->m_id == PMONITOR->activeWorkspaceID() || ws->m_id == PMONITOR->activeSpecialWorkspaceID()) {
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->m_fadingOut)
                *ls->m_alpha = FULLSCREEN && ws->m_fullscreenMode == FSMODE_FULLSCREEN ? 0.f : 1.f;
        }
    }
}

void CDesktopAnimationManager::overrideFullscreenFadeAmount(PHLWORKSPACE ws, float fade, PHLWINDOW exclude) {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w == exclude)
            continue;

        if (w->m_workspace == ws) {
            if (w->m_fadingOut || w->m_pinned || w->isFullscreen())
                continue;

            *w->m_alpha = fade;
        }
    }

    const auto PMONITOR = ws->m_monitor.lock();

    if (ws->m_id == PMONITOR->activeWorkspaceID() || ws->m_id == PMONITOR->activeSpecialWorkspaceID()) {
        for (auto const& ls : PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->m_fadingOut)
                *ls->m_alpha = fade;
        }
    }
}
