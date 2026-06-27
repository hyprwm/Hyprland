#include "ViewHitTester.hpp"
#include "FocusState.hpp"
#include "ViewStateTracker.hpp"
#include "../view/LayerSurface.hpp"
#include "../view/WLSurface.hpp"
#include "../view/Window.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../layout/space/Space.hpp"
#include "../../macros.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../protocols/XDGShell.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../state/MonitorState.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../../xwayland/XWayland.hpp"

#include <cmath>
#include <ranges>
#include <tuple>

using namespace Desktop;
using namespace Desktop::View;

CViewHitTester::CViewHitTester(const IViewStateTracker& tracker) : m_tracker(tracker) {
    ;
}

PHLWINDOW CViewHitTester::windowAt(const Vector2D& pos, uint16_t properties, PHLWINDOW ignoreWindow) const {
    const auto PMONITOR = State::monitorState()->query().vec(pos).run();
    if (!PMONITOR)
        return nullptr;

    static auto PRESIZEONBORDER       = CConfigValue<Config::INTEGER>("general:resize_on_border");
    static auto PBORDERSIZE           = CConfigValue<Config::INTEGER>("general:border_size");
    static auto PBORDERGRABEXTEND     = CConfigValue<Config::INTEGER>("general:extend_border_grab_area");
    static auto PSPECIALFALLTHRU      = CConfigValue<Config::INTEGER>("input:special_fallthrough");
    static auto PMODALPARENTBLOCKING  = CConfigValue<Config::INTEGER>("general:modal_parent_blocking");
    static auto PFOLLOWMOUSESHRINK    = CConfigValue<Config::INTEGER>("input:follow_mouse_shrink");
    const auto  BORDER_GRAB_AREA      = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;
    const bool  ONLY_PRIORITY         = properties & FOCUS_PRIORITY;
    const bool  DO_FOLLOW_MOUSE_CHECK = properties & FOLLOW_MOUSE_CHECK;
    const auto  HITBOX_SHRINK         = DO_FOLLOW_MOUSE_CHECK ? *PFOLLOWMOUSESHRINK : 0;
    const auto  LASTFOCUSED           = focusState()->window();
    const auto& WINDOWS               = m_tracker.windows();

    const auto  isShadowedByModal = [](PHLWINDOW w) -> bool {
        return *PMODALPARENTBLOCKING && w->m_xdgSurface && w->m_xdgSurface->m_toplevel && w->m_xdgSurface->m_toplevel->anyChildModal();
    };

    // pinned windows on top of floating regardless
    if (properties & ALLOW_FLOATING) {
        for (auto const& w : WINDOWS | std::views::reverse) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (w->m_isFloating && w->m_isMapped && w->acceptsInput() && !w->m_X11ShouldntFocus && w->m_pinned && !w->m_ruleApplicator->noFocus().valueOrDefault() &&
                w != ignoreWindow && !isShadowedByModal(w)) {
                const auto BB  = w->getWindowBoxUnified(properties);
                CBox       box = BB.copy().expand(!w->isX11OverrideRedirect() ? BORDER_GRAB_AREA : 0);
                if (HITBOX_SHRINK > 0 && w != LASTFOCUSED)
                    box = box.copy().expand(-HITBOX_SHRINK);
                if (box.containsPoint(pos))
                    return w;

                if (!w->m_isX11) {
                    if (w->hasPopupAt(pos))
                        return w;
                }
            }
        }
    }

    auto windowForWorkspace = [&](bool special) -> PHLWINDOW {
        auto floating = [&](bool aboveFullscreen) -> PHLWINDOW {
            for (auto const& w : WINDOWS | std::views::reverse) {
                if (special && !w->onSpecialWorkspace()) // because special floating may creep up into regular
                    continue;

                if (!w->m_workspace)
                    continue;

                if (ONLY_PRIORITY && !w->priorityFocus())
                    continue;

                const auto PWINDOWMONITOR = w->m_monitor.lock();

                // to avoid focusing windows behind special workspaces from other monitors
                if (!*PSPECIALFALLTHRU && PWINDOWMONITOR && PWINDOWMONITOR->m_activeSpecialWorkspace && w->m_workspace != PWINDOWMONITOR->m_activeSpecialWorkspace) {
                    const auto BB = w->getWindowBoxUnified(properties);
                    if (BB.x >= PWINDOWMONITOR->m_position.x && BB.y >= PWINDOWMONITOR->m_position.y &&
                        BB.x + BB.width <= PWINDOWMONITOR->m_position.x + PWINDOWMONITOR->m_size.x && BB.y + BB.height <= PWINDOWMONITOR->m_position.y + PWINDOWMONITOR->m_size.y)
                        continue;
                }

                if (w->m_isFloating && w->m_isMapped && w->m_workspace->isVisible() && w->acceptsInput() && !w->m_pinned && !w->m_ruleApplicator->noFocus().valueOrDefault() &&
                    w != ignoreWindow && (!aboveFullscreen || w->isAllowedOverFullscreen()) && !isShadowedByModal(w)) {
                    // OR windows should add focus to parent
                    if (w->m_X11ShouldntFocus && !w->isX11OverrideRedirect())
                        continue;

                    const auto BB  = w->getWindowBoxUnified(properties);
                    CBox       box = BB.copy().expand(!w->isX11OverrideRedirect() ? BORDER_GRAB_AREA : 0);
                    if (HITBOX_SHRINK > 0 && w != LASTFOCUSED)
                        box = box.copy().expand(-HITBOX_SHRINK);
                    if (box.containsPoint(pos)) {
                        if (w->m_isX11 && w->isX11OverrideRedirect() && !w->m_xwaylandSurface->wantsFocus()) {
                            // Override Redirect
                            return focusState()->window(); // we kinda trick everything here.
                            // TODO: this is wrong, we should focus the parent, but idk how to get it considering it's nullptr in most cases.
                        }

                        return w;
                    }

                    if (!w->m_isX11) {
                        if (w->hasPopupAt(pos))
                            return w;
                    }
                }
            }

            return nullptr;
        };

        if (properties & ALLOW_FLOATING) {
            // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
            auto found = floating(true);
            if (found)
                return found;
        }

        if (properties & FLOATING_ONLY)
            return floating(false);

        const WORKSPACEID WSPID      = special ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
        const auto        PWORKSPACE = State::workspaceState()->query().id(WSPID).run();

        if (PWORKSPACE->m_hasFullscreenWindow && !(properties & SKIP_FULLSCREEN_PRIORITY) && !ONLY_PRIORITY) {
            const auto FS_WINDOW = PWORKSPACE->getFullscreenWindow();

            if (!FS_WINDOW)
                return nullptr;

            // for maximized windows, don't return a window if we are not directly on it.
            if (FS_WINDOW->m_fullscreenState.internal != FSMODE_MAXIMIZED || FS_WINDOW->getWindowBoxUnified(properties).containsPoint(pos))
                return PWORKSPACE->getFullscreenWindow();
            else
                return nullptr;
        }

        auto found = floating(false);
        if (found)
            return found;

        // for windows, we need to check their extensions too, first.
        for (auto const& w : WINDOWS) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_workspace)
                continue;

            if (!w->m_isX11 && !w->m_isFloating && w->m_isMapped && w->workspaceID() == WSPID && w->acceptsInput() && !w->m_X11ShouldntFocus &&
                !w->m_ruleApplicator->noFocus().valueOrDefault() && w != ignoreWindow && !isShadowedByModal(w)) {
                if (w->hasPopupAt(pos))
                    return w;
            }
        }

        for (auto const& w : WINDOWS) {
            if (ONLY_PRIORITY && !w->priorityFocus())
                continue;

            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_workspace)
                continue;

            if (!w->m_isFloating && w->m_isMapped && w->workspaceID() == WSPID && w->acceptsInput() && !w->m_X11ShouldntFocus && !w->m_ruleApplicator->noFocus().valueOrDefault() &&
                w != ignoreWindow && !isShadowedByModal(w)) {
                CBox box = (properties & USE_PROP_TILED) ? w->getWindowBoxUnified(properties) : w->layoutBox();
                if ((properties & INPUT_EXTENTS) && BORDER_GRAB_AREA > 0 && !w->isX11OverrideRedirect()) {
                    const auto WORKAREA                    = PWORKSPACE->m_space->workArea();
                    auto       isWindowCloseToWorkAreaEdge = [&](const Math::eDirection dir) -> bool {
                        constexpr double STICK_THRESHOLD = 2.0; // This constant is taken from isAdjacent in CWindowQuery::inDirection
                        double           aEdge           = -1;
                        double           bEdge           = -1;

                        switch (dir) {
                            case Math::DIRECTION_LEFT:
                                aEdge = WORKAREA.x;
                                bEdge = box.x;
                                break;
                            case Math::DIRECTION_RIGHT:
                                aEdge = WORKAREA.x + WORKAREA.width;
                                bEdge = box.x + box.width;
                                break;
                            case Math::DIRECTION_UP:
                                aEdge = WORKAREA.y;
                                bEdge = box.y;
                                break;
                            case Math::DIRECTION_DOWN:
                                aEdge = WORKAREA.y + WORKAREA.height;
                                bEdge = box.y + box.height;
                                break;
                            default: break;
                        }
                        const double delta = aEdge - bEdge;
                        return std::abs(delta) < STICK_THRESHOLD;
                    };

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_LEFT)) {
                        box.x -= BORDER_GRAB_AREA;
                        box.width += BORDER_GRAB_AREA;
                    }

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_RIGHT))
                        box.width += BORDER_GRAB_AREA;

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_UP)) {
                        box.y -= BORDER_GRAB_AREA;
                        box.height += BORDER_GRAB_AREA;
                    }

                    if (isWindowCloseToWorkAreaEdge(Math::eDirection::DIRECTION_DOWN))
                        box.height += BORDER_GRAB_AREA;
                }
                if (HITBOX_SHRINK > 0 && w != LASTFOCUSED)
                    box = box.copy().expand(-HITBOX_SHRINK);
                if (box.containsPoint(pos))
                    return w;
            }
        }

        return nullptr;
    };

    // special workspace
    if (PMONITOR->m_activeSpecialWorkspace && !*PSPECIALFALLTHRU)
        return windowForWorkspace(true);

    if (PMONITOR->m_activeSpecialWorkspace) {
        const auto PWINDOW = windowForWorkspace(true);

        if (PWINDOW)
            return PWINDOW;
    }

    return windowForWorkspace(false);
}

SP<CWLSurfaceResource> CViewHitTester::windowSurfaceAt(const Vector2D& pos, PHLWINDOW window, Vector2D& surfaceLocal) const {
    if (!validMapped(window))
        return nullptr;

    RASSERT(!window->m_isX11, "Cannot call windowSurfaceAt on an X11 window!");

    // try popups first
    const auto PPOPUP = window->m_popupHead->at(pos);

    if (PPOPUP) {
        const auto OFF = PPOPUP->coordsRelativeToParent();
        surfaceLocal   = pos - window->m_realPosition->goal() - OFF;
        return PPOPUP->wlSurface()->resource();
    }

    auto [surf, local] = window->wlSurface()->resource()->at(pos - window->m_realPosition->goal(), true);
    if (surf) {
        surfaceLocal = local;
        return surf;
    }

    return nullptr;
}

Vector2D CViewHitTester::surfaceLocalAt(const Vector2D& pos, PHLWINDOW window, SP<CWLSurfaceResource> surface) const {
    if (!validMapped(window))
        return {};

    if (window->m_isX11)
        return pos - window->m_realPosition->goal();

    const auto PPOPUP = window->m_popupHead->at(pos);
    if (PPOPUP)
        return pos - PPOPUP->coordsGlobal();

    std::tuple<SP<CWLSurfaceResource>, Vector2D> iterData = {surface, {-1337, -1337}};

    window->wlSurface()->resource()->breadthfirst(
        [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
            const auto PDATA = sc<std::tuple<SP<CWLSurfaceResource>, Vector2D>*>(data);
            if (surf == std::get<0>(*PDATA))
                std::get<1>(*PDATA) = offset;
        },
        &iterData);

    CBox geom = window->m_xdgSurface->m_current.geometry;

    if (std::get<1>(iterData) == Vector2D{-1337, -1337})
        return pos - window->m_realPosition->goal();

    return pos - window->m_realPosition->goal() - std::get<1>(iterData) + Vector2D{geom.x, geom.y};
}

SP<CWLSurfaceResource> CViewHitTester::layerPopupSurfaceAt(const Vector2D& pos, PHLMONITOR monitor, Vector2D* surfaceCoords, PHLLS* layerFound) const {
    for (auto const& lsl : monitor->m_layerSurfaceLayers | std::views::reverse) {
        for (auto const& ls : lsl | std::views::reverse) {
            if (!ls->aliveAndVisible())
                continue;

            auto SURFACEAT = ls->m_popupHead->at(pos, true);

            if (SURFACEAT) {
                *layerFound    = ls.lock();
                *surfaceCoords = pos - SURFACEAT->coordsGlobal();
                return SURFACEAT->wlSurface()->resource();
            }
        }
    }

    return nullptr;
}

SP<CWLSurfaceResource> CViewHitTester::layerSurfaceAt(const Vector2D& pos, std::vector<PHLLSREF>* layerSurfaces, Vector2D* surfaceCoords, PHLLS* layerFound,
                                                      bool aboveLockscreen) const {
    for (auto const& ls : *layerSurfaces | std::views::reverse) {
        if (!ls->aliveAndVisible() || (aboveLockscreen && ls->m_ruleApplicator->aboveLock().valueOrDefault() != 2))
            continue;

        auto [surf, local] = ls->m_layerSurface->m_surface->at(pos - ls->m_geometry.pos(), true);

        if (surf) {
            if (!surf->m_current.inputIsInfinite && surf->m_current.input.empty())
                continue;

            *layerFound = ls.lock();

            *surfaceCoords = local;

            return surf;
        }
    }

    return nullptr;
}
