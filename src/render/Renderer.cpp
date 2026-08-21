#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <cmath>
#include <cstring>
#include <drm_mode.h>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../pointer/cursor/CursorManager.hpp"
#include "../pointer/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../animation/AnimationManager.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowEffectsController.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/GlobalViewMethods.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/state/FadingOutState.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/LinuxDMABUF.hpp"
#include "../protocols/InputCapture.hpp"
#include "../errorOverlay/Overlay.hpp"
#include "../debug/Overlay.hpp"
#include "../notification/NotificationOverlay.hpp"
#include "../overview/Overview.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../i18n/Engine.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../event/EventBus.hpp"
#include "../helpers/CursorShapes.hpp"
#include "../helpers/MainLoopExecutor.hpp"
#include "../output/Monitor.hpp"
#include "../output/OutputCommitCoordinator.hpp"
#include "../output/WorkspaceTransition.hpp"
#include "../state/MonitorState.hpp"
#include "../state/WorkspaceState.hpp"
#include "macros.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/RendererHintsPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "pass/BackdropScopePassElement.hpp"
#include "scene/Scene.hpp"
#include "../debug/log/Logger.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/types/ContentType.hpp"
#include "AsyncResourceGatherer.hpp"
#include "ElementRenderer.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "Texture.hpp"
#include "./pass/PreBlurElement.hpp"
#include "../protocols/types/SurfaceState.hpp"
#include "types.hpp"
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <optional>
#include <pango/pangocairo.h>
#include <type_traits>
#include <unordered_map>

#include <hyprutils/utils/ScopeGuard.hpp>
#include <random>
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;
using namespace NColorManagement;
using namespace Desktop::View;
using namespace Render;

extern "C" {
#include <xf86drm.h>
}

static int cursorTicker(void* data) {
    g_pHyprRenderer->ensureCursorRenderingMode();
    wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 500);
    return 0;
}

IHyprRenderer::IHyprRenderer() {
    m_globalTimer.reset();

    if (g_pCompositor->m_aqBackend->hasSession()) {
        size_t drmDevices = 0;
        for (auto const& dev : g_pCompositor->m_aqBackend->session->sessionDevices) {
            const auto DRMV = drmGetVersion(dev->fd);
            if (!DRMV)
                continue;
            drmDevices++;
            std::string name = std::string{DRMV->name, DRMV->name_len};
            std::ranges::transform(name, name.begin(), tolower);

            if (name.contains("nvidia"))
                m_nvidia = true;
            else if (name.contains("i915"))
                m_intel = true;
            else if (name.contains("softpipe") || name.contains("Software Rasterizer") || name.contains("llvmpipe"))
                m_software = true;

            Log::logger->log(Log::DEBUG, "DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor, DRMV->version_patchlevel,
                             std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});

            drmFreeVersion(DRMV);
        }
        m_mgpu = drmDevices > 1;
    } else {
        Log::logger->log(Log::DEBUG, "Aq backend has no session, omitting full DRM node checks");

        const auto DRMV = drmGetVersion(g_pCompositor->m_drm.fd);

        if (DRMV) {
            std::string name = std::string{DRMV->name, DRMV->name_len};
            std::ranges::transform(name, name.begin(), tolower);

            if (name.contains("nvidia"))
                m_nvidia = true;
            else if (name.contains("i915"))
                m_intel = true;
            else if (name.contains("softpipe") || name.contains("Software Rasterizer") || name.contains("llvmpipe"))
                m_software = true;

            Log::logger->log(Log::DEBUG, "Primary DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor,
                             DRMV->version_patchlevel, std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});
        } else {
            Log::logger->log(Log::DEBUG, "No primary DRM driver information found");
        }

        drmFreeVersion(DRMV);
    }

    if (m_nvidia)
        Log::logger->log(Log::WARN, "NVIDIA detected, please remember to follow nvidia instructions on the wiki");

    // cursor hiding stuff

    static auto P = Event::bus()->m_events.input.keyboard.key.listen([&](IKeyboard::SKeyEvent e, SP<IKeyboard>, Event::SCallbackInfo&) {
        if (m_cursorHiddenConditions.hiddenOnKeyboard)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = true;
        ensureCursorRenderingMode();
    });

    static auto P2 = Event::bus()->m_events.input.mouse.move.listen([&](Vector2D pos, Event::SCallbackInfo&) {
        if (!m_cursorHiddenConditions.hiddenOnKeyboard && m_cursorHiddenConditions.hiddenOnTouch == g_pInputManager->m_lastInputTouch &&
            m_cursorHiddenConditions.hiddenOnTablet == g_pInputManager->m_lastInputTablet && !m_cursorHiddenConditions.hiddenOnTimeout)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = false;
        m_cursorHiddenConditions.hiddenOnTimeout  = false;
        m_cursorHiddenConditions.hiddenOnTouch    = g_pInputManager->m_lastInputTouch;
        m_cursorHiddenConditions.hiddenOnTablet   = g_pInputManager->m_lastInputTablet;
        ensureCursorRenderingMode();
    });

    static auto P3 = Event::bus()->m_events.monitor.focused.listen([&](PHLMONITOR mon) {
        g_pEventLoopManager->doLater([this]() {
            if (!ErrorOverlay::overlay()->active())
                return;
            for (auto& m : State::monitorState()->monitors()) {
                arrangeLayersForMonitor(m->m_id);
            }
        });
    });

    static auto P4 = Event::bus()->m_events.window.updateRules.listen([&](PHLWINDOW window) {
        if (window->m_ruleApplicator->renderUnfocused().valueOrDefault())
            addWindowToRenderUnfocused(window);
    });

    m_cursorTicker = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, cursorTicker, nullptr);
    wl_event_source_timer_update(m_cursorTicker, 500);

    m_renderUnfocusedTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            static auto PFPS = CConfigValue<Config::INTEGER>("misc:render_unfocused_fps");

            if (m_renderUnfocused.empty())
                return;

            bool dirty = false;
            for (auto& w : m_renderUnfocused) {
                if (!w) {
                    dirty = true;
                    continue;
                }

                if (!w->wlSurface() || !w->wlSurface()->resource() || shouldRenderWindow(w.lock()))
                    continue;

                w->wlSurface()->resource()->breadthfirst(
                    [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                        surf->m_stateQueue.unlockFirst(LOCK_REASON_FENCE | LOCK_REASON_FIFO | LOCK_REASON_TIMER);
                        surf->presentFeedback(Time::steadyNow(), Desktop::focusState()->monitor(), true);
                    },
                    nullptr);
            }

            if (dirty)
                std::erase_if(m_renderUnfocused, [](const auto& e) { return !e || !e->m_ruleApplicator->renderUnfocused().valueOr(false); });

            if (!m_renderUnfocused.empty())
                m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
        },
        nullptr);

    g_pEventLoopManager->addTimer(m_renderUnfocusedTimer);
}

IHyprRenderer::~IHyprRenderer() {
    if (m_cursorTicker)
        wl_event_source_remove(m_cursorTicker);
}

WP<Render::GL::CHyprOpenGLImpl> IHyprRenderer::glBackend() {
    return Render::GL::g_pHyprOpenGL;
}

static bool shouldRenderWindowOnMonitor(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    if (!pWindow->presentation().visibleOnMonitor(pMonitor) || !pWindow->m_workspace)
        return false;

    if (pWindow->m_state & WINDOW_STATE_PINNED)
        return true;

    if (pWindow->presentation().movingFromMonitor() && pWindow->presentation().alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->isBeingAnimated() &&
        pWindow->presentation().alphaValue(WINDOW_ALPHA_MOVE_TO_WORKSPACE) > 0.F && !pWindow->m_workspace->isVisible())
        return true;

    const auto PWINDOWWORKSPACE = pWindow->m_workspace;
    if (PWINDOWWORKSPACE->m_monitor == pMonitor) {
        if (pMonitor->m_workspaceTransition->participates(PWINDOWWORKSPACE))
            return true;

        if (Fullscreen::controller()->hasFullscreen(PWINDOWWORKSPACE) && !pWindow->isAllowedOverFullscreen() &&
            pWindow->presentation().alphaValue(WINDOW_ALPHA_FADE) * pWindow->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN) == 0)
            return false;

        if (!pMonitor->m_workspaceTransition->isAnimating(PWINDOWWORKSPACE) && !PWINDOWWORKSPACE->isVisible())
            return false;
    }

    if (pWindow->m_monitor == pMonitor)
        return true;

    if (!pWindow->m_workspace->isVisible() && pWindow->m_monitor != pMonitor)
        return false;

    if (pWindow->m_workspace->isVisible() && pWindow->isFloating())
        return !Fullscreen::controller()->isFullscreen(pWindow);

    if (pMonitor->m_activeSpecialWorkspace == pWindow->m_workspace)
        return true;

    if (!pWindow->isFloating() && pWindow->positionAnimation()->isBeingAnimated() && pWindow->presentation().animatingIn() && pWindow->m_monitor != pMonitor)
        return false;

    if (pWindow->positionAnimation()->isBeingAnimated()) {
        const auto PWORKSPACEMONITOR = PWINDOWWORKSPACE->m_monitor.lock();
        if (!PWINDOWWORKSPACE->m_isSpecialWorkspace && PWORKSPACEMONITOR && PWORKSPACEMONITOR->m_workspaceTransition->isAnimating(PWINDOWWORKSPACE))
            return false;

        CBox windowBox = pWindow->getFullWindowBoundingBox();
        if (PWORKSPACEMONITOR && PWORKSPACEMONITOR->m_workspaceTransition->isAnimating(PWINDOWWORKSPACE))
            windowBox.translate(PWORKSPACEMONITOR->m_workspaceTransition->offsetValue(PWINDOWWORKSPACE));
        windowBox.translate(pWindow->presentation().floatingOffset());

        const CBox monitorBox = {pMonitor->m_position, pMonitor->m_size};
        if (!windowBox.intersection(monitorBox).empty() && (pWindow->workspaceID() == pMonitor->activeWorkspaceID() || pWindow->presentation().movingFromMonitor()))
            return true;
    }

    return false;
}

bool IHyprRenderer::shouldRenderWindow(const CRenderingContext& context, PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    if (!pWindow->presentation().visibleOnMonitor(pMonitor))
        return false;

    if (!pWindow->m_workspace)
        return false;

    if (context.isolatedWorkspace) {
        if (pWindow->m_state & WINDOW_STATE_PINNED)
            return context.isolatedWorkspaceFullScene;

        if (pWindow->m_workspace == context.isolatedWorkspace)
            return true;

        const auto WORKSPACE_MONITOR = pWindow->m_workspace->m_monitor.lock();
        return context.isolatedWorkspaceFullScene && pWindow->onSpecialWorkspace() && WORKSPACE_MONITOR &&
            (WORKSPACE_MONITOR->m_activeSpecialWorkspace == pWindow->m_workspace || WORKSPACE_MONITOR->m_workspaceTransition->participates(pWindow->m_workspace));
    }

    return shouldRenderWindowOnMonitor(pWindow, pMonitor);
}

bool IHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    return shouldRenderWindowOnMonitor(pWindow, pMonitor);
}

bool IHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow) {

    if (!validMapped(pWindow))
        return false;

    const auto PWORKSPACE = pWindow->m_workspace;

    if (!pWindow->m_workspace)
        return false;

    const auto PWORKSPACEMONITOR = PWORKSPACE->m_monitor.lock();

    if ((pWindow->m_state & WINDOW_STATE_PINNED) || (PWORKSPACEMONITOR && PWORKSPACEMONITOR->m_workspaceTransition->forceRendering(PWORKSPACE)))
        return true;

    if (PWORKSPACE && PWORKSPACE->isVisible())
        return true;

    for (auto const& m : State::monitorState()->monitors()) {
        if (PWORKSPACE && PWORKSPACE->m_monitor == m && m->m_workspaceTransition->isAnimating(PWORKSPACE))
            return true;

        if (m->m_activeSpecialWorkspace && pWindow->onSpecialWorkspace())
            return true;
    }

    return false;
}

float IHyprRenderer::workspaceRenderAlpha(const CRenderingContext& context, PHLWORKSPACE workspace, PHLMONITOR) const {
    if (!workspace || (context.isolatedWorkspace && workspace == context.isolatedWorkspace))
        return 1.F;

    const auto WORKSPACEMONITOR = workspace->m_monitor.lock();
    return WORKSPACEMONITOR ? WORKSPACEMONITOR->m_workspaceTransition->alphaValue(workspace) : 1.F;
}

Vector2D IHyprRenderer::workspaceRenderOffset(const CRenderingContext& context, PHLWORKSPACE workspace, PHLMONITOR) const {
    if (!workspace || (context.isolatedWorkspace && workspace == context.isolatedWorkspace))
        return {};

    const auto WORKSPACEMONITOR = workspace->m_monitor.lock();
    return WORKSPACEMONITOR ? WORKSPACEMONITOR->m_workspaceTransition->offsetValue(workspace) : Vector2D{};
}

bool IHyprRenderer::workspaceRenderIsAnimating(const CRenderingContext& context, PHLWORKSPACE workspace, PHLMONITOR) const {
    if (!workspace || (context.isolatedWorkspace && workspace == context.isolatedWorkspace))
        return false;

    const auto WORKSPACEMONITOR = workspace->m_monitor.lock();
    return WORKSPACEMONITOR && WORKSPACEMONITOR->m_workspaceTransition->isAnimating(workspace);
}

Vector2D IHyprRenderer::windowRenderFloatingOffset(const CRenderingContext& context, PHLWINDOW window) const {
    return window && !(context.isolatedWorkspace && window->m_workspace == context.isolatedWorkspace) ? window->presentation().floatingOffset() : Vector2D{};
}

bool IHyprRenderer::renderingWorkspaceToBuffer(const CRenderingContext& context) const {
    return !!context.isolatedWorkspace;
}

bool IHyprRenderer::shouldRenderMonitor(PHLMONITOR monitor) {
    static auto PDAMAGETRACKINGMODE = CConfigValue<Config::INTEGER>("debug:damage_tracking");
    bool        hasChanged          = monitor->m_output->needsFrame || monitor->m_damage.hasChanged();

    if (!hasChanged && *PDAMAGETRACKINGMODE != DAMAGE_TRACKING_NONE && monitor->m_forceFullFrames == 0)
        return false;

    return true;
}

void IHyprRenderer::renderWorkspaceWindowsFullscreen(CRenderingContext& context, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    PHLWINDOW pWorkspaceWindow = nullptr;

    if (!context.isolatedWorkspace)
        Event::bus()->m_events.render.stage.emit(RENDER_PRE_WINDOWS);

    // pre-filter renderable windows once for the tiled + floating passes
    std::vector<PHLWINDOW> windows;
    windows.reserve(Desktop::windowState()->windows().size());
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!shouldRenderWindow(context, w, pMonitor))
            continue;

        if (w->presentation().alphaValue(WINDOW_ALPHA_FADE) * w->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN) == 0.f)
            continue;

        if (Fullscreen::controller()->isFullscreen(w))
            continue;

        windows.emplace_back(w);
    }

    // tiled windows that are fading out
    for (auto const& w : windows) {
        if (w->isFloating())
            continue;

        if (pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        renderWindow(context, w, pMonitor, time, true, RENDER_PASS_ALL);
    }
    if (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene)
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_WINDOW_TILED, pWorkspace);

    // and floating ones too
    for (auto const& w : windows) {
        if (!w->isFloating())
            continue;

        if (w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        if (w->isFadingOutUnderFullscreen())
            continue; // render these over fullscreen so the fade-out is visible

        if (context.isolatedWorkspace && w->shouldRenderOverFullscreen())
            continue;

        renderWindow(context, w, pMonitor, time, true, RENDER_PASS_ALL);
    }
    if (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene)
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_WINDOW_FLOATING, pWorkspace);

    // TODO: this pass sucks
    for (auto const& w : Desktop::windowState()->windows()) {
        const auto PWORKSPACE = w->m_workspace;

        if (w->m_workspace != pWorkspace || !Fullscreen::controller()->isFullscreen(w)) {
            const auto PWORKSPACEMONITOR = PWORKSPACE ? PWORKSPACE->m_monitor.lock() : nullptr;
            if (!(PWORKSPACE && PWORKSPACEMONITOR && PWORKSPACEMONITOR->m_workspaceTransition->participates(PWORKSPACE)))
                continue;

            if (w->m_monitor != pMonitor)
                continue;
        }

        if (!Fullscreen::controller()->isFullscreen(w))
            continue;

        if (w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (shouldRenderWindow(context, w, pMonitor))
            renderWindow(context, w, pMonitor, time, Fullscreen::controller()->getFullscreenModes(pWorkspace).internal != Fullscreen::FSMODE_FULLSCREEN, RENDER_PASS_ALL);

        if (w->m_workspace != pWorkspace)
            continue;

        pWorkspaceWindow = w;
    }

    if (!pWorkspaceWindow)
        return; // this will produce one blank frame. Oh well.

    // then render windows over fullscreen.
    for (auto const& w : Desktop::windowState()->windows()) {
        const bool shouldSkipWindow = w->workspaceID() != pWorkspaceWindow->workspaceID() || !w->isFloating() || !w->shouldRenderOverFullscreen() || !w->mapped() ||
            Fullscreen::controller()->isFullscreen(w);

        if (shouldSkipWindow)
            continue;

        if (!shouldRenderWindow(context, w, pMonitor))
            continue;

        const bool mismatchedSpecialWorkspace = w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace();

        if (mismatchedSpecialWorkspace)
            continue;

        const bool specialWorkspaceOnDifferentMonitor = pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor;

        if (specialWorkspaceOnDifferentMonitor)
            continue; // special on another are rendered as a part of the base pass

        renderWindow(context, w, pMonitor, time, true, RENDER_PASS_ALL);
    }
    if (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene)
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN, pWorkspace);
}

void IHyprRenderer::renderWorkspaceWindows(CRenderingContext& context, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    PHLWINDOW lastWindow;

    if (!context.isolatedWorkspace)
        Event::bus()->m_events.render.stage.emit(RENDER_PRE_WINDOWS);

    std::vector<PHLWINDOWREF> windows;
    windows.reserve(Desktop::windowState()->windows().size());

    for (auto const& w : Desktop::windowState()->windows()) {
        const bool isNotRenderable = w->isHidden() || !w->mapped();

        if (isNotRenderable)
            continue;

        if (!shouldRenderWindow(context, w, pMonitor))
            continue;

        windows.emplace_back(w);
    }

    // Non-floating main
    for (auto& w : windows) {
        if (w->isFloating())
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->presentation().movingFromMonitor() && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render active window after all others of this pass
        if (w == Desktop::focusState()->window()) {
            lastWindow = w.lock();
            continue;
        }

        // render the bad boy
        renderWindow(context, w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
        w.reset();
    }

    if (lastWindow)
        renderWindow(context, lastWindow, pMonitor, time, true, RENDER_PASS_MAIN);

    lastWindow.reset();

    if (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene)
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_WINDOW_TILED, pWorkspace);

    // Non-floating popup
    for (auto& w : windows) {
        if (!w)
            continue;

        if (w->isFloating())
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->presentation().movingFromMonitor() && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render the bad boy
        renderWindow(context, w.lock(), pMonitor, time, true, RENDER_PASS_POPUP);
        w.reset();
    }

    // floating on top
    for (auto& w : windows) {
        if (!w)
            continue;

        if (!w->isFloating() || (w->m_state & WINDOW_STATE_PINNED))
            continue;

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->presentation().movingFromMonitor() && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        // render the bad boy
        renderWindow(context, w.lock(), pMonitor, time, true, RENDER_PASS_ALL);
    }
    if (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene)
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_WINDOW_FLOATING, pWorkspace);
}

void IHyprRenderer::bindOffMain(CRenderingContext& context) {
    bindFB(context, context.sceneMonitor->resources()->getUnusedWorkBuffer());
    draw(context, CClearPassElement::SClearData{{0, 0, 0, 0}});
}

void IHyprRenderer::bindBackOnMain(CRenderingContext& context) {
    bindFB(context, context.mainFB);
}

void IHyprRenderer::addPassElement(CRenderingContext& context, UP<IPassElement>&& element) {
    context.renderPass().add(std::move(element));
}

void IHyprRenderer::renderWindow(CRenderingContext& context, PHLWINDOW pWindow, PHLMONITOR pMonitor, const Time::steady_tp& time, bool decorate, eRenderPassMode mode,
                                 bool ignorePosition, bool standalone) {
    if (pWindow->isHidden() && !standalone)
        return;

    if (!standalone && pWindow->presentation().alphaTotal() == 0.F && !pWindow->presentation().alpha().isBeingAnimated())
        return;

    if (!pWindow->mapped())
        return;

    TRACY_GPU_ZONE("RenderWindow");

    const auto  PWORKSPACE      = pWindow->m_workspace;
    const auto  WORKSPACEOFFSET = workspaceRenderOffset(context, PWORKSPACE, pMonitor);
    const auto  WINDOWOFFSET    = windowRenderFloatingOffset(context, pWindow);
    const auto  REALPOS         = pWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) + ((pWindow->m_state & WINDOW_STATE_PINNED) ? Vector2D{} : WORKSPACEOFFSET);
    static auto PDIMAROUND      = CConfigValue<Config::FLOAT>("decoration:dim_around");

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time};
    const auto                       REALSIZE   = pWindow->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    CBox                             textureBox = {REALPOS.x, REALPOS.y, std::max(REALSIZE.x, 5.0), std::max(REALSIZE.y, 5.0)};

    renderdata.pos.x = textureBox.x;
    renderdata.pos.y = textureBox.y;
    renderdata.w     = textureBox.w;
    renderdata.h     = textureBox.h;

    if (ignorePosition) {
        renderdata.pos.x = pMonitor->m_position.x;
        renderdata.pos.y = pMonitor->m_position.y;
    } else {
        pWindow->presentation().setNotResponding(pWindow->isNotResponding());
    }

    if (standalone)
        decorate = false;

    // whether to use m_fMovingToWorkspaceAlpha, only if fading out into an invisible ws
    const bool USE_WORKSPACE_FADE_ALPHA = pWindow->presentation().movingFromMonitor() && (!PWORKSPACE || !PWORKSPACE->isVisible());

    renderdata.surface   = pWindow->wlSurface()->resource();
    renderdata.dontRound = Fullscreen::controller()->getFullscreenModes(pWindow).internal == Fullscreen::FSMODE_FULLSCREEN;
    renderdata.fadeAlpha = pWindow->presentation().alphaValue(WINDOW_ALPHA_FADE) * pWindow->presentation().alphaValue(WINDOW_ALPHA_FULLSCREEN) *
        pWindow->presentation().alphaValue(WINDOW_ALPHA_LAYOUT) *
        ((pWindow->m_state & WINDOW_STATE_PINNED) || USE_WORKSPACE_FADE_ALPHA ? 1.f : workspaceRenderAlpha(context, PWORKSPACE, pMonitor)) *
        (USE_WORKSPACE_FADE_ALPHA ? pWindow->presentation().alphaValue(WINDOW_ALPHA_MOVE_TO_WORKSPACE) : 1.F) *
        pWindow->presentation().alphaValue(WINDOW_ALPHA_MOVE_FROM_WORKSPACE);
    renderdata.alpha = pWindow->presentation().alphaValue(WINDOW_ALPHA_ACTIVE);
    renderdata.decorate =
        decorate && !pWindow->backend().traits().suggestsNoBorder && Fullscreen::controller()->getFullscreenModes(pWindow).internal != Fullscreen::FSMODE_FULLSCREEN;
    renderdata.rounding      = standalone || renderdata.dontRound ? 0 : pWindow->presentation().rounding() * pMonitor->m_scale;
    renderdata.roundingPower = standalone || renderdata.dontRound ? 2.0f : pWindow->presentation().roundingPower();
    renderdata.blur          = !standalone && shouldBlur(context, pWindow);
    renderdata.pWindow       = pWindow;

    if (standalone) {
        renderdata.alpha     = 1.f;
        renderdata.fadeAlpha = 1.f;
    }

    // apply opaque
    if (pWindow->m_ruleApplicator->opaque().valueOrDefault())
        renderdata.alpha = 1.f;

    renderdata.pWindow = pWindow;

    // for plugins
    context.currentWindow = pWindow;

    if (!context.isolatedWorkspace)
        Event::bus()->m_events.render.stage.emit(RENDER_PRE_WINDOW);

    const auto fullAlpha = renderdata.alpha * renderdata.fadeAlpha;

    if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault() && !context.renderingSnapshot && mode != RENDER_PASS_POPUP) {
        CBox                        monbox = {0, 0, context.sceneMonitor->m_transformedSize.x, context.sceneMonitor->m_transformedSize.y};
        CRectPassElement::SRectData data;
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * fullAlpha);
        data.box   = monbox;
        addPassElement(context, makeUnique<CRectPassElement>(data));
    }

    renderdata.pos += WINDOWOFFSET;

    // if window is floating and we have a slide animation, clip it to its full bb
    if (!ignorePosition && pWindow->isFloating() && !Fullscreen::controller()->isFullscreen(pWindow) && workspaceRenderIsAnimating(context, PWORKSPACE, pMonitor) &&
        !(pWindow->m_state & WINDOW_STATE_PINNED)) {
        CRegion rg         = pWindow->getFullWindowBoundingBox().translate(-pMonitor->m_position + WORKSPACEOFFSET + WINDOWOFFSET).scale(pMonitor->m_scale).round();
        renderdata.clipBox = rg.getExtents();
    }

    // render window decorations first, if not fullscreen full
    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_MAIN) {

        const bool                       TRANSFORMEDWINDOW = pWindow->effects().hasActiveTransformers();
        UP<CRenderPass>                  transformedPass;
        std::optional<CRenderingContext> transformedContext;
        CRenderingContext*               elementContext     = &context;
        const bool                       windowBlur         = renderdata.blur;
        const bool                       windowBlurUsesLive = windowBlur && !shouldUseNewBlurOptimizations(context, nullptr, pWindow);
        const auto                       backdropScope      = makeShared<SBackdropScope>();

        addPassElement(context, makeUnique<CBackdropScopePassElement>(CBackdropScopePassElement::eAction::BEGIN, backdropScope));

        if (TRANSFORMEDWINDOW) {
            transformedPass = makeUnique<CRenderPass>();
            transformedContext.emplace(context, *transformedPass);
            elementContext  = &*transformedContext;
            renderdata.blur = false;

            pWindow->effects().preWindowRender(*elementContext, &renderdata);
        }

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->presentation().decorations()) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_BOTTOM)
                    continue;

                wd->draw(*elementContext, pMonitor, fullAlpha);
            }

            for (auto const& wd : pWindow->presentation().decorations()) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_UNDER)
                    continue;

                wd->draw(*elementContext, pMonitor, fullAlpha);
            }
        }

        static auto PXWLUSENN = CConfigValue<Config::INTEGER>("xwayland:use_nearest_neighbor");
        if ((pWindow->backend().isX11() && *PXWLUSENN) || pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())
            renderdata.useNearestNeighbor = true;

        if (!TRANSFORMEDWINDOW && pWindow->wlSurface()->small() && !pWindow->wlSurface()->m_fillIgnoreSmall && renderdata.blur) {
            CBox wb = {renderdata.pos.x - pMonitor->m_position.x, renderdata.pos.y - pMonitor->m_position.y, renderdata.w, renderdata.h};
            wb.scale(pMonitor->m_scale).round();
            CRectPassElement::SRectData data;
            data.color          = CHyprColor(0, 0, 0, 0);
            data.box            = wb;
            data.round          = renderdata.dontRound ? 0 : renderdata.rounding - 1;
            data.blur           = true;
            data.blurA          = renderdata.fadeAlpha;
            data.xray           = shouldUseNewBlurOptimizations(context, nullptr, pWindow);
            data.blurPatternBox = wb;
            data.blurOwner      = pWindow;
            addPassElement(*elementContext, makeUnique<CRectPassElement>(data));
            renderdata.blur = false;
        }

        renderdata.surfaceCounter = 0;
        pWindow->wlSurface()->resource()->breadthfirst(
            [this, &renderdata, &pWindow, elementContext](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                if (!s->m_current.texture)
                    return;

                if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                    return;

                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pWindow->wlSurface()->resource();
                addPassElement(*elementContext, makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            nullptr);

        renderdata.useNearestNeighbor = false;

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->presentation().decorations()) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVER)
                    continue;

                wd->draw(*elementContext, pMonitor, fullAlpha);
            }
        }

        if (TRANSFORMEDWINDOW) {
            CBox currentBox = pWindow->getFullWindowBoundingBox();
            currentBox.translate(((pWindow->m_state & WINDOW_STATE_PINNED) ? Vector2D{} : WORKSPACEOFFSET) + WINDOWOFFSET - pMonitor->m_position);
            CBox            transformedBox = pWindow->effects().transformedExtents(currentBox);

            SMotionBlurData windowMotionBlur;
            if (!standalone && !context.renderingSnapshot) {
                pWindow->effects().amendTransformedRenderData(context, transformedBox, &windowMotionBlur);
            }

            CBox blurBox = {renderdata.pos.x - pMonitor->m_position.x, renderdata.pos.y - pMonitor->m_position.y, renderdata.w, renderdata.h};
            blurBox.scale(pMonitor->m_scale).round();

            addPassElement(context,
                           makeUnique<CTransformedWindowPassElement>(CTransformedWindowPassElement::SData{
                               .pass              = std::move(transformedPass),
                               .window            = pWindow,
                               .currentBox        = currentBox,
                               .blurBox           = blurBox,
                               .blur              = windowBlur,
                               .blurUsesLive      = windowBlurUsesLive,
                               .blurA             = renderdata.fadeAlpha,
                               .blurRound         = renderdata.dontRound ? 0 : std::max(renderdata.rounding - 1, 0),
                               .blurRoundingPower = renderdata.roundingPower,
                               .transformedBox    = transformedBox,
                               .motionBlur        = windowMotionBlur,
                               .standalone        = standalone,
                               .renderingSnapshot = context.renderingSnapshot,
                           }));

            renderdata.blur = windowBlur;
        }

        addPassElement(context, makeUnique<CBackdropScopePassElement>(CBackdropScopePassElement::eAction::END, backdropScope));
    }

    context.clipBox = CBox();

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_POPUP) {
        if (!pWindow->backend().isX11()) {
            const auto GEOM = pWindow->backend().geometry().box;

            renderdata.pos -= GEOM.pos();
            renderdata.dontRound       = true; // don't round popups
            renderdata.pMonitor        = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            renderdata.popup           = true;

            static CConfigValue PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:popups_ignorealpha");

            renderdata.blur = (!context.isolatedWorkspace || context.isolatedWorkspaceFullScene) && shouldBlur(context, pWindow->popupHead());

            if (renderdata.blur) {
                renderdata.discardMode |= DISCARD_ALPHA;
                renderdata.discardOpacity = *PBLURIGNOREA;
            }

            if (pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())
                renderdata.useNearestNeighbor = true;

            renderdata.surfaceCounter  = 0;
            const auto PARENTFADEALPHA = renderdata.fadeAlpha;

            pWindow->popupHead()->breadthfirst(
                [this, &context, &renderdata, PARENTFADEALPHA](WP<Desktop::View::CPopup> popup, void* data) {
                    if (!popup->mapped() || !popup->resource() ||
                        ((!context.isolatedWorkspace || context.isolatedWorkspaceFullScene) && (!popup->acceptsInput() || !popup->alphaNonZero())))
                        return;

                    const auto     pos    = popup->coordsRelativeToParent();
                    const Vector2D oldPos = renderdata.pos;
                    renderdata.pos += pos;
                    renderdata.fadeAlpha = PARENTFADEALPHA * popup->alpha()[POPUP_ALPHA_FADE]->value();

                    popup->wlSurface()->resource()->breadthfirst(
                        [this, &context, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                            if (!s->m_current.texture)
                                return;

                            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                                return;

                            renderdata.localPos    = offset;
                            renderdata.texture     = s->m_current.texture;
                            renderdata.surface     = s;
                            renderdata.mainSurface = false;
                            addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
                            renderdata.surfaceCounter++;
                        },
                        data);

                    renderdata.pos = oldPos;
                },
                &renderdata);

            renderdata.fadeAlpha = PARENTFADEALPHA;
            renderdata.alpha     = 1.F;
        }

        if (decorate) {
            for (auto const& wd : pWindow->presentation().decorations()) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVERLAY)
                    continue;

                wd->draw(context, pMonitor, fullAlpha);
            }
        }
    }

    if (!context.isolatedWorkspace)
        Event::bus()->m_events.render.stage.emit(RENDER_POST_WINDOW);

    context.currentWindow.reset();
}

void IHyprRenderer::draw(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage) {
    ASSERT(element);
    if (!element)
        return;

    elementRenderer()->drawElement(context, element, damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CBorderPassElement::SBorderData& data, const CRegion& damage) {
    draw(context, makeUnique<CBorderPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CClearPassElement::SClearData& data, const CRegion& damage) {
    draw(context, makeUnique<CClearPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CFramebufferElement::SFramebufferElementData& data, const CRegion& damage) {
    draw(context, makeUnique<CFramebufferElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CRectPassElement::SRectData& data, const CRegion& damage) {
    draw(context, makeUnique<CRectPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CRendererHintsPassElement::SData& data, const CRegion& damage) {
    draw(context, makeUnique<CRendererHintsPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CShadowPassElement::SShadowData& data, const CRegion& damage) {
    draw(context, makeUnique<CShadowPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CSurfacePassElement::SRenderData& data, const CRegion& damage) {
    draw(context, makeUnique<CSurfacePassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CTexPassElement::SRenderData& data, const CRegion& damage) {
    draw(context, makeUnique<CTexPassElement>(data), damage);
}

void IHyprRenderer::draw(CRenderingContext& context, const CTextureMatteElement::STextureMatteData& data, const CRegion& damage) {
    draw(context, makeUnique<CTextureMatteElement>(data), damage);
}

void IHyprRenderer::bindFB(CRenderingContext& context, SP<IFramebuffer> fb) {
    fb->bind();
    context.currentFB = fb;
}

UP<CScopeGuard> IHyprRenderer::bindTempFB(CRenderingContext& context, SP<IFramebuffer> fb) {
    const auto oldFB = context.currentFB;
    bindFB(context, fb);
    return makeUnique<CScopeGuard>([this, &context, oldFB] { bindFB(context, oldFB); });
}

bool IHyprRenderer::preBlurQueued(const CRenderingContext& context) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");
    static auto PBLUR            = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    if (!context.sceneMonitor)
        return false;
    return context.sceneMonitor->m_blurFBDirty && *PBLURNEWOPTIMIZE && *PBLUR && context.precomputeBlur;
}

SP<ITexture> IHyprRenderer::createTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy) {
    if (!buffer)
        return createTexture();

    auto attrs = buffer->dmabuf();

    if (!attrs.success) {
        // attempt shm
        auto shm = buffer->shm();

        if (!shm.success) {
            Log::logger->log(Log::ERR, "Cannot create a texture: buffer has no dmabuf or shm");
            return createTexture(buffer->opaque);
        }

        auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0);

        return createTexture(fmt, pixelData, bufLen, shm.size, keepDataCopy, buffer->opaque);
    }

    auto tex = createTexture(attrs, buffer->opaque);

    if (!tex) {
        Log::logger->log(Log::ERR, "Cannot create a texture: failed to create an Image");
        return createTexture(buffer->opaque);
    }

    return tex;
}

void IHyprRenderer::renderLayer(CRenderingContext& context, PHLLS pLayer, PHLMONITOR pMonitor, const Time::steady_tp& time, bool popups, bool lockscreen) {
    if (!pLayer)
        return;

    if (!pLayer->mapped() || !pLayer->acceptsInput() || !pLayer->alphaNonZero())
        return;

    // skip rendering based on abovelock rule and make sure to not render abovelock layers twice
    if ((pLayer->m_ruleApplicator->aboveLock().valueOrDefault() && !lockscreen && g_pSessionLockManager->isSessionLocked()) ||
        (lockscreen && !pLayer->m_ruleApplicator->aboveLock().valueOrDefault()))
        return;

    static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");

    if (*PDIMAROUND && pLayer->m_ruleApplicator->dimAround().valueOrDefault() && !context.renderingSnapshot && !popups) {
        CRectPassElement::SRectData data;
        data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pLayer->alpha()[LS_ALPHA_FADE]->value());
        addPassElement(context, makeUnique<CRectPassElement>(data));
    }

    TRACY_GPU_ZONE("RenderLayer");

    const auto                       REALPOS = pLayer->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const auto                       REALSIZ = pLayer->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, REALPOS};
    renderdata.fadeAlpha                        = pLayer->alpha()[LS_ALPHA_FADE]->value();
    renderdata.blur                             = shouldBlur(context, pLayer);
    renderdata.surface                          = pLayer->wlSurface()->resource();
    renderdata.decorate                         = false;
    renderdata.w                                = REALSIZ.x;
    renderdata.h                                = REALSIZ.y;
    renderdata.pLS                              = pLayer;
    renderdata.blockBlurOptimization            = pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    renderdata.clipBox = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y}.scale(pMonitor->m_scale).round();
    if (renderdata.blur && pLayer->m_ruleApplicator->ignoreAlpha().hasValue()) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = pLayer->m_ruleApplicator->ignoreAlpha().valueOrDefault();
    }

    if (!popups)
        pLayer->wlSurface()->resource()->breadthfirst(
            [this, &context, &renderdata, &pLayer](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                if (!s->m_current.texture)
                    return;

                if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                    return;

                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pLayer->wlSurface()->resource();
                addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    renderdata.popup           = true;
    renderdata.blur            = pLayer->m_ruleApplicator->blurPopups().valueOrDefault();
    renderdata.discardMode &= ~DISCARD_ALPHA;
    renderdata.discardOpacity = 0.F;
    if (renderdata.blur && pLayer->m_ruleApplicator->ignoreAlpha().hasValue()) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = pLayer->m_ruleApplicator->ignoreAlpha().valueOrDefault();
    }
    renderdata.surfaceCounter = 0;
    if (popups) {
        pLayer->popupHead()->breadthfirst(
            [this, &context, &renderdata](WP<Desktop::View::CPopup> popup, void* data) {
                if (!popup->mapped() || !popup->acceptsInput() || !popup->alphaNonZero())
                    return;

                const auto SURF = popup->wlSurface()->resource();

                if (!SURF->m_current.texture)
                    return;

                if (SURF->m_current.size.x < 1 || SURF->m_current.size.y < 1)
                    return;

                Vector2D pos           = popup->coordsRelativeToParent();
                renderdata.localPos    = pos;
                renderdata.texture     = SURF->m_current.texture;
                renderdata.surface     = SURF;
                renderdata.mainSurface = false;
                addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);
    }
}

void IHyprRenderer::renderIMEPopup(CRenderingContext& context, CInputPopup* pPopup, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    const auto                       POS = pPopup->globalBox().pos();

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, POS};

    const auto                       SURF = pPopup->getSurface();

    renderdata.surface  = SURF;
    renderdata.decorate = false;
    renderdata.w        = SURF->m_current.size.x;
    renderdata.h        = SURF->m_current.size.y;

    static auto PBLUR        = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PBLURIMES    = CConfigValue<Config::INTEGER>("decoration:blur:input_methods");
    static auto PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:input_methods_ignorealpha");

    renderdata.blur = *PBLURIMES && *PBLUR;
    if (renderdata.blur) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = *PBLURIGNOREA;
    }

    SURF->breadthfirst(
        [this, &context, &renderdata, &SURF](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == SURF;
            addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void IHyprRenderer::renderSessionLockSurface(CRenderingContext& context, WP<SSessionLockSurface> pSurface, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    static auto                      PSESSIONLOCKXRAY = CConfigValue<Config::BOOL>("misc:session_lock_xray");
    static auto                      PSESSIONLOCKBLUR = CConfigValue<Config::BOOL>("misc:session_lock_blur");

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->m_position, pMonitor->m_position};

    renderdata.blur     = *PSESSIONLOCKBLUR && *PSESSIONLOCKXRAY;
    renderdata.surface  = pSurface->surface->surface();
    renderdata.decorate = false;
    renderdata.w        = pMonitor->m_size.x;
    renderdata.h        = pMonitor->m_size.y;

    renderdata.surface->breadthfirst(
        [this, &context, &renderdata, &pSurface](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pSurface->surface->surface();
            addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void IHyprRenderer::renderAllClientsForWorkspace(CRenderingContext& context, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate,
                                                 const float& scale) {
    static auto PXPMODE          = CConfigValue<Config::INTEGER>("render:xp_mode");
    static auto PSESSIONLOCKXRAY = CConfigValue<Config::INTEGER>("misc:session_lock_xray");

    if UNLIKELY (!pMonitor)
        return;

    if UNLIKELY (g_pSessionLockManager->isSessionLocked() && !*PSESSIONLOCKXRAY) {
        // We stop to render workspaces as soon as the lockscreen was sent the "locked" or "finished" (aka denied) event.
        // In addition we make sure to stop rendering workspaces after misc:lockdead_screen_delay has passed.
        if (g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied())
            return;
    }

    SRenderModifData RENDERMODIFDATA;
    if (translate != Vector2D{0, 0})
        RENDERMODIFDATA.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate);
    if UNLIKELY (scale != 1.f)
        RENDERMODIFDATA.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale);

    if UNLIKELY (!RENDERMODIFDATA.modifs.empty())
        addPassElement(context, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RENDERMODIFDATA}));

    CScopeGuard x([&context, &RENDERMODIFDATA] {
        if (!RENDERMODIFDATA.modifs.empty()) {
            g_pHyprRenderer->addPassElement(context, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        }
    });

    if UNLIKELY (!pWorkspace) {
        // allow rendering without a workspace. In this case, just render layers.

        renderMonitorBackground(context, pMonitor, time);

        if (!context.isolatedWorkspace)
            Event::bus()->m_events.render.stage.emit(RENDER_POST_WALLPAPER);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(context, ls.lock(), pMonitor, time);
        }
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_BOTTOM);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            renderLayer(context, ls.lock(), pMonitor, time);
        }
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_TOP);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
            renderLayer(context, ls.lock(), pMonitor, time);
        }
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_OVERLAY);

        return;
    }

    if LIKELY (!*PXPMODE) {
        renderMonitorBackground(context, pMonitor, time);

        if (!context.isolatedWorkspace)
            Event::bus()->m_events.render.stage.emit(RENDER_POST_WALLPAPER);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(context, ls.lock(), pMonitor, time);
        }
        renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_BOTTOM);
    }

    // pre window pass
    if (preBlurQueued(context))
        addPassElement(context, makeUnique<CPreBlurElement>());

    if UNLIKELY /* subjective? */ (Fullscreen::controller()->hasFullscreen(pWorkspace))
        renderWorkspaceWindowsFullscreen(context, pMonitor, pWorkspace, time);
    else
        renderWorkspaceWindows(context, pMonitor, pWorkspace, time);

    // and then special
    if UNLIKELY (pMonitor->m_specialDim->value() != 0.F) {
        CRectPassElement::SRectData data;
        data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
        data.color = CHyprColor(0, 0, 0, pMonitor->m_specialDim->value());

        addPassElement(context, makeUnique<CRectPassElement>(data));
    }

    if UNLIKELY (pMonitor->m_specialBlur->value() != 0.F) {
        CRectPassElement::SRectData data;
        data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
        data.color = CHyprColor(0, 0, 0, 0);
        data.blur  = true;
        data.blurA = pMonitor->m_specialBlur->value();

        addPassElement(context, makeUnique<CRectPassElement>(data));
    }

    std::vector<PHLWORKSPACE> specialWorkspaces;
    for (const auto& monitor : State::monitorState()->monitors()) {
        for (const auto& workspace : monitor->m_workspaceTransition->participants(true)) {
            if (!std::ranges::contains(specialWorkspaces, workspace))
                specialWorkspaces.emplace_back(workspace);
        }

        if (monitor->m_activeSpecialWorkspace && !std::ranges::contains(specialWorkspaces, monitor->m_activeSpecialWorkspace))
            specialWorkspaces.emplace_back(monitor->m_activeSpecialWorkspace);
    }

    for (const auto& workspace : specialWorkspaces) {
        if (workspaceRenderAlpha(context, workspace, pMonitor) <= 0.F)
            continue;

        if (Fullscreen::controller()->hasFullscreen(workspace))
            renderWorkspaceWindowsFullscreen(context, pMonitor, workspace, time);
        else
            renderWorkspaceWindows(context, pMonitor, workspace, time);
    }

    // pinned always above
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->isHidden() && !w->mapped())
            continue;

        if (!(w->m_state & WINDOW_STATE_PINNED) || !w->isFloating())
            continue;

        if (!shouldRenderWindow(context, w, pMonitor))
            continue;

        // render the bad boy
        renderWindow(context, w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    if (!context.isolatedWorkspace)
        Event::bus()->m_events.render.stage.emit(RENDER_POST_WINDOWS);

    // Render surfaces above windows for monitor
    for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        renderLayer(context, ls.lock(), pMonitor, time);
    }
    renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_TOP);

    for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(context, ls.lock(), pMonitor, time);
    }
    renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_LAYER_OVERLAY);

    for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
        for (auto const& ls : lsl) {
            renderLayer(context, ls.lock(), pMonitor, time, true);
        }
    }
    renderFadeouts(context, pMonitor, Desktop::FADEOUT_PLANE_POPUP);

    renderDragIcon(context, pMonitor, time);
}

void IHyprRenderer::renderMonitorBackground(CRenderingContext& context, PHLMONITOR monitor, const Time::steady_tp& time) {
    if (!monitor)
        return;

    renderBackground(context, monitor);

    for (const auto& layer : monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        renderLayer(context, layer.lock(), monitor, time);
    }
    renderFadeouts(context, monitor, Desktop::FADEOUT_PLANE_LAYER_BACKGROUND);
}

void IHyprRenderer::renderIME(CRenderingContext& context, PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry) {
    Vector2D translate = {geometry.x, geometry.y};
    float    scale     = sc<float>(geometry.width) / pMonitor->m_transformedSize.x;

    TRACY_GPU_ZONE("RenderIME");

    if (!DELTALESSTHAN(sc<double>(geometry.width) / sc<double>(geometry.height), pMonitor->m_transformedSize.x / pMonitor->m_transformedSize.y, 0.01)) {
        Log::logger->log(Log::ERR, "Ignoring geometry in renderIME: aspect ratio mismatch");
        scale     = 1.f;
        translate = Vector2D{};
    }

    SRenderModifData RENDERMODIFDATA;
    if (translate != Vector2D{0, 0})
        RENDERMODIFDATA.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate);
    if UNLIKELY (scale != 1.f)
        RENDERMODIFDATA.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale);

    if UNLIKELY (!RENDERMODIFDATA.modifs.empty())
        addPassElement(context, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RENDERMODIFDATA}));

    CScopeGuard x([&context, &RENDERMODIFDATA] {
        if (!RENDERMODIFDATA.modifs.empty()) {
            g_pHyprRenderer->addPassElement(context, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        }
    });

    // Render IME popups above everything
    for (auto const& imep : g_pInputManager->m_relay.m_inputMethodPopups) {
        if (imep->shouldBeRendered())
            renderIMEPopup(context, imep.get(), pMonitor, now);
    }
}

SP<ITexture> IHyprRenderer::getBackground(CRenderingContext& context, PHLMONITOR pMonitor) {

    if (m_backgroundResourceFailed)
        return nullptr;

    if (!m_backgroundResource) {
        // queue the asset to be created
        requestBackgroundResource();
        return nullptr;
    }

    if (!m_backgroundResource->m_ready)
        return nullptr;

    Log::logger->log(Log::DEBUG, "Creating a texture for BGTex");
    SP<ITexture> backgroundTexture = createTexture(m_backgroundResource->m_asset.cairoSurface->cairo());

    if (!backgroundTexture || !backgroundTexture->ok())
        return nullptr;

    Log::logger->log(Log::DEBUG, "BGTex created for monitor {}", pMonitor->m_name);

    const int monW  = (int)std::round(pMonitor->m_transformedSize.x);
    const int monH  = (int)std::round(pMonitor->m_transformedSize.y);
    const int origW = backgroundTexture->m_size.x;
    const int origH = backgroundTexture->m_size.y;

    if (monW > 0 && monH > 0) {
        const double scaleX = (double)monW / origW;
        const double scaleY = (double)monH / origH;
        const double scale  = std::max(scaleX, scaleY);

        // scale the background if it's larger than the monitor
        if (scale < 1.0) {
            auto fb = createFB("BGTex scale");
            fb->alloc(monW, monH);

            CRenderingContext child{context, context.renderPass()};
            auto              guard = bindTempFB(child, fb);
            child.fbSize            = Vector2D{monW, monH};
            setProjectionType(child, RPT_EXPORT);
            child.transformDamage = false;
            setViewport(0, 0, monW, monH);

            draw(child, CClearPassElement::SClearData{{0.F, 0.F, 0.F, 0.F}});

            const double texW = origW * scale;
            const double texH = origH * scale;
            const double offX = (monW - texW) / 2.0;
            const double offY = (monH - texH) / 2.0;

            CRegion      fullDamage = {0, 0, monW, monH};
            draw(child, CTexPassElement::SRenderData{.tex = backgroundTexture, .box = CBox{offX, offY, texW, texH}, .damage = fullDamage}, fullDamage);

            backgroundTexture = fb->getTexture();

            Log::logger->log(Log::INFO, "BGTex scaled from {}x{} to {}x{} for monitor {}", origW, origH, monW, monH, pMonitor->m_name);
        }
    }

    // clear the resource after we're done using it
    g_pEventLoopManager->doLater([this] { m_backgroundResource.reset(); });

    // set the animation to start for fading this background in nicely
    pMonitor->m_backgroundOpacity->setValueAndWarp(0.F);
    *pMonitor->m_backgroundOpacity = 1.F;

    return backgroundTexture;
}

void IHyprRenderer::renderBackground(CRenderingContext& context, PHLMONITOR pMonitor) {
    static auto PRENDERTEX       = CConfigValue<Config::INTEGER>("misc:disable_hyprland_logo");
    static auto PBACKGROUNDCOLOR = CConfigValue<Config::INTEGER>("misc:background_color");
    static auto PNOSPLASH        = CConfigValue<Config::INTEGER>("misc:disable_splash_rendering");

    if (*PRENDERTEX /* inverted cfg flag */ || pMonitor->m_backgroundOpacity->isBeingAnimated())
        addPassElement(context, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));

    if (!*PRENDERTEX) {
        static auto PBACKGROUNDCOLOR = CConfigValue<Config::INTEGER>("misc:background_color");

        if (!pMonitor->m_background)
            pMonitor->m_background = getBackground(context, pMonitor);

        if (!pMonitor->m_background)
            addPassElement(context, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));
        else {
            CTexPassElement::SRenderData data;
            const double                 MONRATIO = context.sceneMonitor->m_transformedSize.x / context.sceneMonitor->m_transformedSize.y;
            const double                 WPRATIO  = pMonitor->m_background->m_size.x / pMonitor->m_background->m_size.y;
            Vector2D                     origin;
            double                       scale = 1.0;

            if (MONRATIO > WPRATIO) {
                scale    = context.sceneMonitor->m_transformedSize.x / pMonitor->m_background->m_size.x;
                origin.y = (context.sceneMonitor->m_transformedSize.y - pMonitor->m_background->m_size.y * scale) / 2.0;
            } else {
                scale    = context.sceneMonitor->m_transformedSize.y / pMonitor->m_background->m_size.y;
                origin.x = (context.sceneMonitor->m_transformedSize.x - pMonitor->m_background->m_size.x * scale) / 2.0;
            }

            if (MONRATIO != WPRATIO)
                addPassElement(context, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));

            data.box = {origin, pMonitor->m_background->m_size * scale};
            data.a   = context.sceneMonitor->m_backgroundOpacity->value();
            data.tex = pMonitor->m_background;
            addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
        }
    }

    if (!*PNOSPLASH) {
        auto monitorSize = pMonitor->m_transformedSize;
        if (!pMonitor->m_splash)
            pMonitor->m_splash = renderSplash([this, pMonitor](auto width, auto height, const auto DATA) { return createTexture(width, height, DATA); }, monitorSize.y / 76,
                                              monitorSize.x, monitorSize.y);

        if (pMonitor->m_splash) {
            CTexPassElement::SRenderData data;
            data.box = {{(monitorSize.x - pMonitor->m_splash->m_size.x) / 2.0, monitorSize.y * 0.98 - pMonitor->m_splash->m_size.y}, pMonitor->m_splash->m_size};
            data.tex = pMonitor->m_splash;
            addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
        }
    }
}

void IHyprRenderer::requestBackgroundResource() {
    if (m_backgroundResource)
        return;

    static auto PNOWALLPAPER    = CConfigValue<Config::INTEGER>("misc:disable_hyprland_logo");
    static auto PFORCEWALLPAPER = CConfigValue<Config::INTEGER>("misc:force_default_wallpaper");

    const auto  FORCEWALLPAPER = std::clamp(*PFORCEWALLPAPER, sc<int64_t>(-1), sc<int64_t>(2));

    if (*PNOWALLPAPER)
        return;

    static bool        once    = true;
    static std::string texPath = "wall";

    if (once) {
        // get the adequate tex
        if (FORCEWALLPAPER == -1) {
            std::mt19937_64                 engine(time(nullptr));
            std::uniform_int_distribution<> distribution(0, 2);

            texPath += std::to_string(distribution(engine));
        } else
            texPath += std::to_string(std::clamp(*PFORCEWALLPAPER, sc<int64_t>(0), sc<int64_t>(2)));

        texPath += ".png";

        texPath = resolveAssetPath(texPath);

        once = false;
    }

    if (texPath.empty()) {
        m_backgroundResourceFailed = true;
        return;
    }

    m_backgroundResource = makeAtomicShared<Hyprgraphics::CImageResource>(texPath);

    // doesn't have to be ASP as it's passed
    SP<CMainLoopExecutor> executor = makeShared<CMainLoopExecutor>([this] {
        for (const auto& m : State::monitorState()->monitors()) {
            damageMonitor(m);
        }
    });

    m_backgroundResource->m_events.finished.listenStatic([executor] {
        // this is in the worker thread.
        executor->signal();
    });

    g_pAsyncResourceGatherer->enqueue(m_backgroundResource);
}

std::string IHyprRenderer::resolveAssetPath(const std::string& filename) {
    std::string fullPath;
    for (auto& e : ASSET_PATHS) {
        std::string     p = std::format("{}/hypr/{}", e, filename);
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            fullPath = p;
            break;
        } else
            Log::logger->log(Log::DEBUG, "resolveAssetPath: looking at {} unsuccessful: ec {}", filename, ec.message());
    }

    if (fullPath.empty()) {
        m_failedAssetsNo++;
        Log::logger->log(Log::ERR, "resolveAssetPath: looking for {} failed (no provider found)", filename);
        return "";
    }

    return fullPath;
}

SP<ITexture> IHyprRenderer::loadAsset(const std::string& filename) {

    const std::string fullPath = resolveAssetPath(filename);

    if (fullPath.empty())
        return m_missingAssetTexture;

    const auto CAIROSURFACE = cairo_image_surface_create_from_png(fullPath.c_str());

    if (!CAIROSURFACE) {
        m_failedAssetsNo++;
        Log::logger->log(Log::ERR, "loadAsset: failed to load {} (corrupt / inaccessible / not png)", fullPath);
        return m_missingAssetTexture;
    }

    auto tex = createTexture(CAIROSURFACE);

    cairo_surface_destroy(CAIROSURFACE);

    return tex;
}

SP<ITexture> IHyprRenderer::getBlurTexture(const CRenderingContext&, PHLMONITORREF pMonitor) {
    return pMonitor->resources()->m_blurFB->getTexture();
}

bool IHyprRenderer::shouldUseNewBlurOptimizations(const CRenderingContext& context, PHLLS pLayer, PHLWINDOW pWindow) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Config::INTEGER>("decoration:blur:xray");

    if (context.isolatedWorkspaceFullScene || !getBlurTexture(context, context.sceneMonitor))
        return false;

    if (blurProviderRequiresLiveBlur())
        return false;

    if (pWindow && pWindow->m_ruleApplicator->xray().hasValue() && !pWindow->m_ruleApplicator->xray().valueOrDefault())
        return false;

    if (pLayer && pLayer->m_ruleApplicator->xray().valueOrDefault() == 0)
        return false;

    if ((*PBLURNEWOPTIMIZE && pWindow && !pWindow->isFloating() && !pWindow->onSpecialWorkspace()) || *PBLURXRAY)
        return true;

    if ((pLayer && pLayer->m_ruleApplicator->xray().valueOrDefault() == 1) || (pWindow && pWindow->m_ruleApplicator->xray().valueOrDefault()))
        return true;

    return false;
}

void IHyprRenderer::initMissingAssetTexture() {

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 512, 512);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_antialias(CAIRO, CAIRO_ANTIALIAS_NONE);
    cairo_save(CAIRO);
    cairo_set_source_rgba(CAIRO, 0, 0, 0, 1);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_SOURCE);
    cairo_paint(CAIRO);
    cairo_set_source_rgba(CAIRO, 1, 0, 1, 1);
    cairo_rectangle(CAIRO, 256, 0, 256, 256);
    cairo_fill(CAIRO);
    cairo_rectangle(CAIRO, 0, 256, 256, 256);
    cairo_fill(CAIRO);
    cairo_restore(CAIRO);

    cairo_surface_flush(CAIROSURFACE);

    auto tex = createTexture(CAIROSURFACE);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);

    m_missingAssetTexture = tex;
}

void IHyprRenderer::initAssets() {
    initMissingAssetTexture();

    m_screencopyDeniedTexture = renderText("Permission denied to share screen", Colors::WHITE, 20);
}

SP<ITexture> IHyprRenderer::renderText(const std::string& text, CHyprColor col, int pt, bool italic, const std::string& fontFamily, int maxWidth, int weight) {
    static auto           FONT = CConfigValue<std::string>("misc:font_family");

    const auto            FONTFAMILY = fontFamily.empty() ? *FONT : fontFamily;
    const auto            FONTSIZE   = pt;
    const auto            COLOR      = col;

    PangoFontMap*         fontMap    = pango_cairo_font_map_get_default();
    PangoContext*         context    = pango_font_map_create_context(fontMap);
    PangoLayout*          layoutText = pango_layout_new(context);
    PangoFontDescription* pangoFD    = pango_font_description_new();
    g_object_unref(context);

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, sc<PangoWeight>(weight));
    pango_layout_set_font_description(layoutText, pangoFD);
    pango_layout_set_text(layoutText, text.c_str(), -1);

    if (maxWidth > 0) {
        pango_layout_set_width(layoutText, maxWidth * PANGO_SCALE);
        pango_layout_set_ellipsize(layoutText, PANGO_ELLIPSIZE_END);
    }

    PangoRectangle rectInk = {}, rectLog = {};
    pango_layout_get_pixel_extents(layoutText, &rectInk, &rectLog);
    int  textW = std::max(rectLog.width, rectInk.x + rectInk.width);
    int  textH = std::max(rectLog.height, rectInk.y + rectInk.height);

    auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textW, textH);
    auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);
    cairo_move_to(CAIRO, 0, 0);
    pango_cairo_show_layout(CAIRO, layoutText);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);
    auto tex = createTexture(CAIROSURFACE);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    return tex;
}

SP<ITexture> IHyprRenderer::renderText(Hyprgraphics::CTextResource::STextResourceData&& data) {
    auto res = makeAtomicShared<Hyprgraphics::CTextResource>(std::move(data));
    g_pAsyncResourceGatherer->enqueue(res);
    g_pAsyncResourceGatherer->await(res);

    if (!res->m_asset.cairoSurface)
        return nullptr;

    return createTexture(res->m_asset.pixelSize.x, res->m_asset.pixelSize.y, res->m_asset.cairoSurface->data());
}

void IHyprRenderer::ensureLockTexturesRendered(bool load) {
    static bool loaded = false;

    if (loaded == load)
        return;

    loaded = load;

    if (load) {
        // this will cause a small hitch. I don't think we can do much, other than wasting VRAM and having this loaded all the time.
        m_lockDeadTexture  = loadAsset("lockdead.png");
        m_lockDead2Texture = loadAsset("lockdead2.png");
        m_lockDead3Texture = loadAsset("lockdead.png");

        const auto VT = g_pCompositor->getVTNr();

        m_lockTtyTextTexture = renderText(std::format("Running on tty {}", VT.has_value() ? std::to_string(*VT) : "unknown"), CHyprColor{0.9F, 0.9F, 0.9F, 0.7F}, 20, true);
    } else {
        m_lockDeadTexture.reset();
        m_lockDead2Texture.reset();
        m_lockDead3Texture.reset();
        m_lockTtyTextTexture.reset();
    }
}

void IHyprRenderer::renderLockscreen(CRenderingContext& context, PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry) {
    TRACY_GPU_ZONE("RenderLockscreen");

    const bool LOCKED = g_pSessionLockManager->isSessionLocked();
    if (!LOCKED) {
        ensureLockTexturesRendered(false);
        return;
    }

    const bool RENDERPRIMER = g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied();
    if (RENDERPRIMER)
        renderSessionLockPrimer(context, pMonitor);

    const auto PSLS              = g_pSessionLockManager->getSessionLockSurfaceForMonitor(pMonitor->m_id);
    const bool RENDERLOCKMISSING = (PSLS.expired() || g_pSessionLockManager->clientDenied()) && g_pSessionLockManager->shallConsiderLockMissing();

    ensureLockTexturesRendered(RENDERLOCKMISSING);

    if (RENDERLOCKMISSING)
        renderSessionLockMissing(context, pMonitor);
    else if (PSLS) {
        renderSessionLockSurface(context, PSLS, pMonitor, now);
        g_pSessionLockManager->onLockscreenRenderedOnMonitor(pMonitor->m_id);

        // render layers and then their popups for abovelock rule
        for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                renderLayer(context, ls.lock(), pMonitor, now, false, true);
            }
        }
        for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                renderLayer(context, ls.lock(), pMonitor, now, true, true);
            }
        }
    }
}

void IHyprRenderer::renderSessionLockPrimer(CRenderingContext& context, PHLMONITOR pMonitor) {
    static auto PSESSIONLOCKXRAY = CConfigValue<Config::INTEGER>("misc:session_lock_xray");
    if (*PSESSIONLOCKXRAY)
        return;

    CRectPassElement::SRectData data;
    data.color = CHyprColor(0, 0, 0, 1.f);
    data.box   = CBox{{}, pMonitor->m_transformedSize};

    addPassElement(context, makeUnique<CRectPassElement>(data));
}

void IHyprRenderer::renderSessionLockMissing(CRenderingContext& context, PHLMONITOR pMonitor) {
    if (g_pCompositor->m_startLocked && !g_pCompositor->m_startLockedCommand.empty())
        return;

    const bool ANY_PRESENT = g_pSessionLockManager->anySessionLockSurfacesPresent();

    // ANY_PRESENT: render image2, without instructions. Lock still "alive", unless texture dead
    // else: render image, with instructions. Lock is gone.
    CBox                         monbox = {{}, pMonitor->m_transformedSize};
    CTexPassElement::SRenderData data;
    if (g_pCompositor->m_startLocked && g_pCompositor->m_startLockedCommand.empty())
        data.tex = m_lockDead3Texture;
    else
        data.tex = (ANY_PRESENT) ? m_lockDead2Texture : m_lockDeadTexture;
    data.box = monbox;
    data.a   = 1;

    addPassElement(context, makeUnique<CTexPassElement>(data));

    if (!ANY_PRESENT && m_lockTtyTextTexture) {
        // also render text for the tty number
        CBox texbox = {{}, m_lockTtyTextTexture->m_size};
        data.tex    = m_lockTtyTextTexture;
        data.box    = texbox;

        addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
    }
}

bool IHyprRenderer::beginRender(CRenderingContext& context, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, SP<IFramebuffer> fb, bool simple,
                                std::optional<Monitor::CDamageRing::CTransaction>* damageTransaction) {
    context.renderPass().clear();
    context.backdropCaptures.clear();
    context.cmSettingsCache->entries.clear();
    context.renderMode  = mode;
    const auto pMonitor = context.outputMonitor.lock();

    if (simple) {
        context.fbSize = fb ? fb->m_size : buffer->m_texture->m_size;
        setProjectionType(context, RPT_EXPORT);
    } else
        setProjectionType(context, RPT_MONITOR);

    const auto RESOURCES     = context.sceneMonitor->resources();
    const bool HAS_MIRROR_FB = RESOURCES->hasMirrorFB();

    if (HAS_MIRROR_FB && !RESOURCES->shouldKeepMirrorFB())
        RESOURCES->releaseMirrorFB();

    if (context.renderMode == RENDER_MODE_FULL_FAKE)
        return beginFullFakeRenderInternal(context, damage, fb, simple);

    int bufferAge = 0;

    if (!buffer) {
        context.buffer = pMonitor->m_output->swapchain->next(&bufferAge);
        if (!context.buffer) {
            Log::logger->log(Log::ERR, "Failed to acquire swapchain buffer for {}", pMonitor->m_name);
            return false;
        }
    } else
        context.buffer = buffer;

    initRender();

    if (!initRenderBuffer(context, context.buffer, pMonitor->m_output->state->state().drmFormat)) {
        Log::logger->log(Log::ERR, "failed to start a render pass for output {}, no RBO could be obtained", pMonitor->m_name);
        return false;
    }

    std::optional<Monitor::CDamageRing::CTransaction> transaction;
    if (context.renderMode == RENDER_MODE_NORMAL) {
        transaction.emplace(pMonitor->m_damage.beginTransaction());
        damage = transaction->getBufferDamage(bufferAge);

        if (pMonitor->needsACopyFB())
            damage.add(pMonitor->resources()->pendingMirrorFBDamage());
    }

    const auto  res     = beginRenderInternal(context, damage, simple);
    static bool initial = true;
    if (initial) {
        initAssets();
        initial = false;
    }

    if (!res) {
        if (context.renderMode == RENDER_MODE_NORMAL && !buffer)
            pMonitor->m_output->swapchain->rollback();
        return false;
    }

    if (transaction) {
        if (damageTransaction)
            *damageTransaction = std::move(transaction);
        else
            transaction->commit();
    }

    return true;
}

void IHyprRenderer::setDamage(CRenderingContext& context, const CRegion& damage_, std::optional<CRegion> finalDamage) {
    context.damage.set(damage_);
    context.finalDamage.set(finalDamage.value_or(damage_));
}

static Mat3x3 getFBProjection(PHLMONITORREF pMonitor, const Vector2D& size) {
    if (pMonitor->m_transform == WL_OUTPUT_TRANSFORM_NORMAL)
        return Mat3x3::identity();

    const Vector2D tfmd = pMonitor->m_transform % 2 == 1 ? Vector2D{size.y, size.x} : size;
    return Mat3x3::identity().translate(size / 2.0).transform(Math::wlTransformToHyprutils(pMonitor->m_transform)).translate(-tfmd / 2.0);
}

void IHyprRenderer::setProjectionType(CRenderingContext& context, const Vector2D& fbSize) {
    context.fbSize = fbSize;
    setProjectionType(context, RPT_FB);
}

void IHyprRenderer::setProjectionType(CRenderingContext& context, eRenderProjectionType projectionType) {
    context.projectionType = projectionType;
    switch (projectionType) {
        case RPT_MONITOR:
        case RPT_EXPORT: context.targetProjection = Mat3x3::identity(); break;
        case RPT_OUTPUT: context.targetProjection = context.sceneMonitor->getTransformMatrix(); break;
        case RPT_FB: context.targetProjection = getFBProjection(context.sceneMonitor, context.fbSize); break;
        default: UNREACHABLE();
    }
}

Mat3x3 IHyprRenderer::getBoxProjection(const CRenderingContext& context, const CBox& box, std::optional<eTransform> transform) {
    return context.targetProjection.projectBox(box, transform.value_or(HYPRUTILS_TRANSFORM_NORMAL), box.rot);
}

Mat3x3 IHyprRenderer::projectBoxToTarget(const CRenderingContext& context, const CBox& box, std::optional<eTransform> transform) {
    const auto TARGET_SIZE = context.projectionType == RPT_MONITOR ? context.sceneMonitor->m_transformedSize : context.fbSize;
    const auto OUTPUT_PROJECTION =
        context.projectionType == RPT_OUTPUT ? context.sceneMonitor->getScaleMatrix() : Mat3x3::outputProjection(TARGET_SIZE, HYPRUTILS_TRANSFORM_NORMAL);

    return OUTPUT_PROJECTION.copy().multiply(getBoxProjection(context, box, transform));
}

SP<IFramebuffer> IHyprRenderer::blurMainFramebuffer(CRenderingContext& context, float strength, const CRegion& originalDamage, const SBlurContext& blurContext) {
    const auto renderTarget = context.currentFB;
    const auto blurSource   = !context.backdropCaptures.empty() && context.backdropCaptures.back().framebuffer ? context.backdropCaptures.back().framebuffer : renderTarget;

    if (!blurSource || !blurSource->getTexture()) {
        Log::logger->log(Log::ERR, "BUG THIS: null fb texture while attempting to blur main fb?! (introspection off?!)");
        return context.sceneMonitor->resources()->m_blurFB;
    }

    auto guard = bindTempFB(context, renderTarget); // blurFramebuffer messes with FB bindings
    return blurFramebuffer(context, blurSource, strength, originalDamage, blurContext);
}

void IHyprRenderer::beginBackdropScope(CRenderingContext& context, SP<SBackdropScope> scope) {
    RASSERT(scope, "Cannot begin a null backdrop scope");

    SP<IFramebuffer> backdrop;
    if (scope->required && !scope->damage.empty() && context.currentFB && context.currentFB->getTexture()) {
        backdrop = context.sceneMonitor->resources()->getUnusedWorkBuffer();
        if (backdrop) {
            const auto renderTarget = context.currentFB;
            const auto backend      = glBackend();
            const auto savedBlend   = backend && backend->blendEnabled();

            {
                CRenderingContext child{context, context.renderPass()};
                auto              guard  = bindTempFB(child, backdrop);
                child.damage             = scope->damage;
                child.renderModif        = {};
                child.useNearestNeighbor = true;
                blend(false);
                renderOffToMain(child, renderTarget);
                blend(savedBlend);
            }
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                Log::logger->log(Log::WARN, "Failed to allocate a clean backdrop buffer; live blur will include the current window's rendered content");
            }
        }
    }

    context.backdropCaptures.emplace_back(SBackdropCapture{.scope = std::move(scope), .framebuffer = std::move(backdrop)});
}

void IHyprRenderer::endBackdropScope(CRenderingContext& context, SP<SBackdropScope> scope) {
    RASSERT(!context.backdropCaptures.empty() && context.backdropCaptures.back().scope == scope, "Unbalanced runtime backdrop scope");
    context.backdropCaptures.pop_back();
}

void IHyprRenderer::scheduleFrameForAnimatedBlur(const CRenderingContext& context, const CRegion& damage, bool usesPrecomputedBlur) {
    const auto monitor = context.sceneMonitor;
    if (context.renderMode != RENDER_MODE_NORMAL || !monitor || monitor->isMirror() || damage.empty())
        return;

    if (usesPrecomputedBlur)
        monitor->m_blurFBDirty = true;

    monitor->addDamage(damage);
}

void IHyprRenderer::preBlurForCurrentMonitor(CRenderingContext& context, const CRegion& fakeDamage) {

    const auto blurredFB  = blurMainFramebuffer(context, 1, fakeDamage);
    const auto blurredTex = blurredFB->getTexture();

    // render onto blurFB
    auto guard = bindTempFB(context, context.sceneMonitor->resources()->m_blurFB);

    draw(context, CClearPassElement::SClearData{{0, 0, 0, 0}});

    draw(context,
         CTexPassElement::SRenderData{
             .tex    = blurredTex,
             .box    = CBox{0, 0, context.sceneMonitor->m_transformedSize.x, context.sceneMonitor->m_transformedSize.y},
             .damage = fakeDamage,
         },
         fakeDamage); // .noAA = true
}

static bool isSDR2HDR(const CRenderingContext& context, const NColorManagement::SImageDescription& imageDescription,
                      const NColorManagement::SImageDescription& targetImageDescription) {
    // might be too strict
    return (imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_SRGB ||
            imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22) &&
        (targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ||
         targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_HLG ||
         (targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR &&
          context.sceneMonitor->m_imageDescription->value().transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ));
}

static bool isHDR2SDR(const NColorManagement::SImageDescription& imageDescription, const NColorManagement::SImageDescription& targetImageDescription) {
    // might be too strict
    return (imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ ||
            imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_HLG) &&
        (targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_SRGB ||
         targetImageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22);
}

SCMSettings IHyprRenderer::getCMSettings(const CRenderingContext& context, const NColorManagement::PImageDescription imageDescription,
                                         const NColorManagement::PImageDescription targetImageDescription, SP<CWLSurfaceResource> surface, bool modifySDR, float sdrMinLuminance,
                                         int sdrMaxLuminance, bool shouldUseSurface) {
    const auto srcId = imageDescription->id();
    const auto dstId = targetImageDescription->id();
    void*      sPtr  = shouldUseSurface ? context.surface.get() : nullptr;

    for (auto const& entry : context.cmSettingsCache->entries) {
        if (entry.srcDescId == srcId && entry.dstDescId == dstId && entry.surfacePtr == sPtr && entry.modifySDR == modifySDR && entry.sdrMinLuminance == sdrMinLuminance &&
            entry.sdrMaxLuminance == sdrMaxLuminance)
            return entry.settings;
    }

    const auto                          sdrEOTF = NTransferFunction::fromConfig();
    NColorManagement::eTransferFunction srcTF;

    const int                           tonemapMode = shouldUseSurface && context.currentWindow ? context.currentWindow->m_ruleApplicator->tonemap().valueOr(1) : 1;

    if (shouldUseSurface && context.surface.valid() &&
        (imageDescription->value().transferFunction == CM_TRANSFER_FUNCTION_GAMMA22 || imageDescription->value().transferFunction == CM_TRANSFER_FUNCTION_SRGB)) {
        if (context.surface->m_colorManagement.valid()) {
            if (sdrEOTF == NTransferFunction::TF_FORCED_GAMMA22 && imageDescription->value().transferFunction == NColorManagement::eTransferFunction::CM_TRANSFER_FUNCTION_SRGB)
                srcTF = NColorManagement::eTransferFunction::CM_TRANSFER_FUNCTION_GAMMA22;
            else
                srcTF = imageDescription->value().transferFunction;
        } else if (sdrEOTF == NTransferFunction::TF_SRGB)
            srcTF = NColorManagement::eTransferFunction::CM_TRANSFER_FUNCTION_SRGB;
        else if (sdrEOTF == NTransferFunction::TF_GAMMA22 || sdrEOTF == NTransferFunction::TF_FORCED_GAMMA22)
            srcTF = NColorManagement::eTransferFunction::CM_TRANSFER_FUNCTION_GAMMA22;
        else
            srcTF = imageDescription->value().transferFunction;
    } else
        srcTF = imageDescription->value().transferFunction;

    const bool  needsSDRmod     = modifySDR && isSDR2HDR(context, imageDescription->value(), targetImageDescription->value());
    const bool  needsHDRmod     = !needsSDRmod && isHDR2SDR(imageDescription->value(), targetImageDescription->value());
    const float maxLuminance    = needsHDRmod ?
        imageDescription->value().getTFMaxLuminance(-1) :
        (imageDescription->value().luminances.max > 0 ? imageDescription->value().luminances.max : imageDescription->value().luminances.reference);
    const auto  dstMaxLuminance = targetImageDescription->value().luminances.max > 0 ? targetImageDescription->value().luminances.max : 10000;

    auto        matrix = imageDescription->getPrimaries()->convertMatrix(targetImageDescription->getPrimaries());
    auto        toXYZ  = targetImageDescription->getPrimaries()->value().toXYZ();

    const bool  needsMod = needsSDRmod &&
        ((context.sceneMonitor->m_sdrSaturation > 0 && context.sceneMonitor->m_sdrSaturation != 1.0f) ||
         (context.sceneMonitor->m_sdrBrightness > 0 && context.sceneMonitor->m_sdrBrightness != 1.0f));

    const bool needsTonemap = maxLuminance >= dstMaxLuminance * 1.01;

    auto       result = SCMSettings{
        .sourceTF        = srcTF,
        .targetTF        = targetImageDescription->value().transferFunction,
        .srcTFRange      = {.min = imageDescription->value().getTFMinLuminance(needsSDRmod ? sdrMinLuminance : -1),
                            .max = imageDescription->value().getTFMaxLuminance(needsSDRmod ? sdrMaxLuminance : -1)},
        .dstTFRange      = {.min = targetImageDescription->value().getTFMinLuminance(needsSDRmod ? sdrMinLuminance : -1),
                            .max = targetImageDescription->value().getTFMaxLuminance(needsSDRmod ? sdrMaxLuminance : -1)},
        .srcRefLuminance = imageDescription->value().luminances.reference,
        .dstRefLuminance = targetImageDescription->value().luminances.reference,
        .convertMatrix   = matrix.mat(),

        .needsTonemap            = tonemapMode != 0 && needsTonemap,
        .tonemapMode             = tonemapMode,
        .maxLuminance            = needsTonemap && tonemapMode == 2 ? dstMaxLuminance :
                                                                      maxLuminance * targetImageDescription->value().luminances.reference / imageDescription->value().luminances.reference,
        .dstMaxLuminance         = dstMaxLuminance,
        .dstPrimaries2XYZ        = toXYZ.mat(),
        .needsSDRmod             = needsMod,
        .sdrSaturation           = needsSDRmod && context.sceneMonitor->m_sdrSaturation > 0 ? context.sceneMonitor->m_sdrSaturation : 1.0f,
        .sdrBrightnessMultiplier = needsSDRmod && context.sceneMonitor->m_sdrBrightness > 0 ? context.sceneMonitor->m_sdrBrightness : 1.0f,
    };

    context.cmSettingsCache->entries.push_back({
        .srcDescId       = srcId,
        .dstDescId       = dstId,
        .surfacePtr      = sPtr,
        .modifySDR       = modifySDR,
        .sdrMinLuminance = sdrMinLuminance,
        .sdrMaxLuminance = sdrMaxLuminance,
        .settings        = result,
    });

    return result;
}

void IHyprRenderer::renderMirrored(CRenderingContext& context) {
    auto monitor  = context.sceneMonitor;
    auto mirrored = monitor->m_mirrorOf;

    // saveBufferForMirror should create it
    if (!mirrored->resources()->hasMirrorFB())
        return;

    const double scale  = std::min(monitor->m_transformedSize.x / mirrored->m_transformedSize.x, monitor->m_transformedSize.y / mirrored->m_transformedSize.y);
    CBox         monbox = {0, 0, mirrored->m_transformedSize.x * scale, mirrored->m_transformedSize.y * scale};

    monbox.x = (monitor->m_transformedSize.x - monbox.w) / 2;
    monbox.y = (monitor->m_transformedSize.y - monbox.h) / 2;

    const auto MIRROR_TEX = mirrored->resources()->getMirrorTexture();

    addPassElement(context, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)}));

    CTexPassElement::SRenderData data;
    data.tex = MIRROR_TEX;
    data.box = monbox;

    addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
}

void IHyprRenderer::renderMonitor(PHLMONITOR pMonitor, bool commit) {
    if (!pMonitor)
        return;
    static std::chrono::high_resolution_clock::time_point renderStart        = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point renderStartOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay   = std::chrono::high_resolution_clock::now();

    static auto                                           PDEBUGOVERLAY       = CConfigValue<Config::INTEGER>("debug:overlay");
    static auto                                           PDAMAGETRACKINGMODE = CConfigValue<Config::INTEGER>("debug:damage_tracking");
    static auto                                           PDAMAGEBLINK        = CConfigValue<Config::INTEGER>("debug:damage_blink");
    static auto                                           PSOLDAMAGE          = CConfigValue<Config::INTEGER>("debug:render_solitary_wo_damage");
    static auto                                           PVFR                = CConfigValue<Config::INTEGER>("debug:vfr");

    static int                                            damageBlinkCleanup = 0; // because double-buffered

    const float                                           ZOOMFACTOR = pMonitor->m_cursorZoom->value();

    if (pMonitor->m_pixelSize.x < 1 || pMonitor->m_pixelSize.y < 1) {
        Log::logger->log(Log::ERR, "Refusing to render a monitor because of an invalid pixel size: {}", pMonitor->m_pixelSize);
        return;
    }

    if (!*PDAMAGEBLINK)
        damageBlinkCleanup = 0;

    if (*PDEBUGOVERLAY == 1) {
        renderStart = std::chrono::high_resolution_clock::now();
        Debug::overlay()->frameData(pMonitor);
    }

    if (!g_pCompositor->m_sessionActive)
        return;

    Event::bus()->m_events.render.preChecks.emit(pMonitor);

    if (Animation::mgr())
        Animation::mgr()->frameTick();

    {
        static bool once = true;
        if (once) {
            Event::bus()->m_events.start.emit();
            once = false;
        }
    }

    if (pMonitor->m_scheduledRecalc) {
        pMonitor->m_scheduledRecalc = false;
        if (pMonitor->m_activeWorkspace) // might be missing (mirror)
            pMonitor->m_activeWorkspace->m_space->recalculate(Layout::RECALCULATE_REASON_RENDER_MONITOR);
    }

    // needsFrame can be cleared by commits that didnt consume our damage like a
    // commit while a pageflip was in flight, so pending damage must keep the frame alive.
    if (!pMonitor->m_output->needsFrame && pMonitor->m_forceFullFrames == 0 && !pMonitor->m_damage.hasChanged())
        return;

    if (!pMonitor->m_commitCoordinator->canBeginFrame()) {
        pMonitor->m_pendingFrame = true;
        return;
    }

    // tearing and DS first
    bool       shouldTear              = pMonitor->updateTearing();
    const bool canAttemptDirectScanout = pMonitor->canAttemptDirectScanoutFast();
    const auto presentationMode =
        shouldTear ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE : Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC;
    if (pMonitor->m_output->state->state().presentationMode != presentationMode)
        pMonitor->m_output->state->setPresentationMode(presentationMode);

    if (canAttemptDirectScanout) {
        handleFullscreenSettings(pMonitor);
        if (pMonitor->attemptDirectScanout()) {
            return;
        } else if (!pMonitor->m_lastScanout.expired() || pMonitor->m_directScanoutIsActive)
            pMonitor->handleDSleave();
    }

    Event::bus()->m_events.render.pre.emit(pMonitor);

    const auto NOW = Time::steadyNow();

    if (!shouldRenderMonitor(pMonitor) && damageBlinkCleanup == 0)
        return;

    if (*PDAMAGETRACKINGMODE == -1) {
        Log::logger->log(Log::CRIT, "Damage tracking mode -1 ????");
        return;
    }

    Event::bus()->m_events.render.stage.emit(RENDER_PRE);

    pMonitor->m_renderingActive = true;
    CScopeGuard renderingGuard([pMonitor] { pMonitor->m_renderingActive = false; });

    CRenderPass       pass;
    CRenderingContext context{pMonitor, pass};

    // Most frames have no fading-out windows or layers for this monitor.
    if (!Desktop::fadingOutState()->fadeouts().empty())
        Desktop::fadingOutState()->cleanupForMonitor(pMonitor);

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    TRACY_GPU_ZONE("Render");

    static bool zoomLock = false;
    if (zoomLock && ZOOMFACTOR == 1.f) {
        Pointer::mgr()->unlockSoftwareAll();
        zoomLock = false;
    } else if (!zoomLock && ZOOMFACTOR != 1.f) {
        Pointer::mgr()->lockSoftwareAll();
        zoomLock = true;
    }

    context.mouseZoomFactor = 1.f;
    if (ZOOMFACTOR != 1.f && pMonitor == State::monitorState()->query().vec(Pointer::mgr()->untransformedPosition()).run())
        context.mouseZoomFactor = std::clamp(ZOOMFACTOR, 1.f, INFINITY);

    if (pMonitor->m_zoomAnimProgress->value() != 1) {
        context.mouseZoomFactor    = 2.0 - pMonitor->m_zoomAnimProgress->value(); // 2x zoom -> 1x zoom
        context.mouseZoomUseMouse  = false;
        context.useNearestNeighbor = false;
    }

    const bool ZOOM_DAMAGE_ENTIRE = pMonitor->m_zoomController.shouldDamageEntire(context.mouseZoomFactor);

    CRegion                                           damage, finalDamage;
    std::optional<Monitor::CDamageRing::CTransaction> damageTransaction;
    if (!beginRender(context, damage, RENDER_MODE_NORMAL, {}, nullptr, false, &damageTransaction)) {
        Log::logger->log(Log::ERR, "renderer: couldn't beginRender()!");
        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->m_forceFullFrames > 0 || damageBlinkCleanup > 0 ||
        ZOOM_DAMAGE_ENTIRE || pMonitor->resources()->m_sceneStack.hasOverride())
        damage = {0, 0, sc<int>(pMonitor->m_transformedSize.x) * 10, sc<int>(pMonitor->m_transformedSize.y) * 10};

    finalDamage = damage;

    // update damage in renderdata as we modified it
    setDamage(context, damage, finalDamage);

    if (pMonitor->m_forceFullFrames > 0) {
        pMonitor->m_forceFullFrames -= 1;
        if (pMonitor->m_forceFullFrames > 10)
            pMonitor->m_forceFullFrames = 0;
    }

    Event::bus()->m_events.render.stage.emit(RENDER_BEGIN);

    bool renderCursor = !pMonitor->isMirror();

    if (pMonitor->m_solitaryClient && !pMonitor->resources()->m_sceneStack.hasOverride() && (!finalDamage.empty() || *PSOLDAMAGE))
        renderWindow(context, pMonitor->m_solitaryClient.lock(), pMonitor, NOW, false, RENDER_PASS_MAIN /* solitary = no popups */);
    else if (!finalDamage.empty()) {
        pMonitor->resources()->m_sceneStack.current()->draw(context, NOW);

        if (pMonitor == Desktop::focusState()->monitor()) {
            Notification::overlay()->draw(context, pMonitor);
            ErrorOverlay::overlay()->draw(context);
        }

        // for drawing the debug overlay
        if (!State::monitorState()->monitors().empty() && pMonitor == State::monitorState()->monitors().front() && *PDEBUGOVERLAY == 1) {
            renderStartOverlay = std::chrono::high_resolution_clock::now();
            Debug::overlay()->draw(context);
            endRenderOverlay = std::chrono::high_resolution_clock::now();
        }

        if (*PDAMAGEBLINK && damageBlinkCleanup == 0) {
            CRectPassElement::SRectData data;
            data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
            data.color = CHyprColor(1.0, 0.0, 1.0, 100.0 / 255.0);
            addPassElement(context, makeUnique<CRectPassElement>(data));
            damageBlinkCleanup = 1;
        } else if (*PDAMAGEBLINK) {
            damageBlinkCleanup++;
            if (damageBlinkCleanup > 3)
                damageBlinkCleanup = 0;
        }
    } else if (!pMonitor->isMirror()) {
        if (pMonitor->m_activeWorkspace)
            sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW);
        if (pMonitor->m_activeSpecialWorkspace)
            sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeSpecialWorkspace, NOW);
    }

    renderCursor = renderCursor && shouldRenderCursor();

    if (renderCursor) {
        TRACY_GPU_ZONE("RenderCursor");
        Pointer::mgr()->renderSoftwareCursorsFor(context, pMonitor->m_self.lock(), NOW, context.damage);
    }

    if (pMonitor->m_dpmsBlackOpacity->value() != 0.F) {
        // render the DPMS black if we are animating
        CRectPassElement::SRectData data;
        data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
        data.color = Colors::BLACK.modifyA(pMonitor->m_dpmsBlackOpacity->value());
        addPassElement(context, makeUnique<CRectPassElement>(data));
    }

    Event::bus()->m_events.render.stage.emit(RENDER_LAST_MOMENT);

    endRender(context);

    TRACY_GPU_COLLECT;

    CRegion frameDamage{context.damage};

    const auto TRANSFORM = Math::invertTransform(pMonitor->m_transform);
    frameDamage.transform(Math::wlTransformToHyprutils(TRANSFORM), pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y);

    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
        frameDamage.add(0, 0, sc<int>(pMonitor->m_transformedSize.x), sc<int>(pMonitor->m_transformedSize.y));

    if (*PDAMAGEBLINK)
        frameDamage.add(damage);

    Event::bus()->m_events.render.stage.emit(RENDER_POST);

    pMonitor->m_output->state->addDamage(frameDamage);
    bool submitted = true;
    if (commit)
        submitted = commitPendingAndDoExplicitSync(pMonitor, std::move(damageTransaction), context.damage);
    else {
        if (damageTransaction)
            damageTransaction->commit();
        pMonitor->m_commitCoordinator->stageRenderedDamage(context.damage, pMonitor->needsACopyFB());
    }

    if (shouldTear && submitted)
        pMonitor->m_tearingState.busy = true;

    if (*PDAMAGEBLINK || *PVFR == 0 || pMonitor->m_pendingFrame)
        pMonitor->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_RENDER_MONITOR);

    pMonitor->m_pendingFrame = false;

    if (*PDEBUGOVERLAY == 1) {
        const float durationUs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000.f;
        Debug::overlay()->renderData(pMonitor, durationUs);

        if (pMonitor == State::monitorState()->monitors().front()) {
            const float noOverlayUs = durationUs - std::chrono::duration_cast<std::chrono::nanoseconds>(endRenderOverlay - renderStartOverlay).count() / 1000.f;
            Debug::overlay()->renderDataNoOverlay(pMonitor, noOverlayUs);
        } else
            Debug::overlay()->renderDataNoOverlay(pMonitor, durationUs);
    }
}

static const hdr_output_metadata NO_HDR_METADATA = {.hdmi_metadata_type1 = hdr_metadata_infoframe{.eotf = 0}};

static hdr_output_metadata       createHDRMetadata(SImageDescription settings, PHLMONITOR monitor) {
    uint8_t eotf = 0;
    switch (settings.transferFunction) {
        case CM_TRANSFER_FUNCTION_GAMMA22:
        case CM_TRANSFER_FUNCTION_SRGB: eotf = 0; break; // used to send primaries and luminances to AQ. ignored for now
        case CM_TRANSFER_FUNCTION_ST2084_PQ: eotf = 2; break;
        case CM_TRANSFER_FUNCTION_EXT_LINEAR:
            eotf = 2;
            break; // should be Windows scRGB
        // case CM_TRANSFER_FUNCTION_HLG: eotf = 3; break; TODO check display capabilities first
        default: return NO_HDR_METADATA; // empty metadata for SDR
    }

    const auto toNits  = [](uint32_t value) { return sc<uint16_t>(std::round(value)); };
    const auto to16Bit = [](float value) { return sc<uint16_t>(std::round(value * 50000)); };

    auto       colorimetry = settings.getPrimaries();
    auto       luminances  = settings.masteringLuminances.max > 0 ? settings.masteringLuminances :
                                                                    (settings.luminances != SImageDescription::SPCLuminances{} ?
                                                                         SImageDescription::SPCMasteringLuminances{.min = settings.luminances.min, .max = settings.luminances.max} :
                                                                         SImageDescription::SPCMasteringLuminances{.min = monitor->minLuminance(), .max = monitor->maxLuminance(10000)});

    Log::logger->log(Log::TRACE, "ColorManagement primaries {},{} {},{} {},{} {},{}", colorimetry.red.x, colorimetry.red.y, colorimetry.green.x, colorimetry.green.y,
                     colorimetry.blue.x, colorimetry.blue.y, colorimetry.white.x, colorimetry.white.y);
    Log::logger->log(Log::TRACE, "ColorManagement min {}, max {}, cll {}, fall {}", luminances.min, luminances.max, settings.maxCLL, settings.maxFALL);
    return hdr_output_metadata{
        .metadata_type = 0,
        .hdmi_metadata_type1 =
            hdr_metadata_infoframe{
                .eotf          = eotf,
                .metadata_type = 0,
                .display_primaries =
                    {
                        {.x = to16Bit(colorimetry.red.x), .y = to16Bit(colorimetry.red.y)},
                        {.x = to16Bit(colorimetry.green.x), .y = to16Bit(colorimetry.green.y)},
                        {.x = to16Bit(colorimetry.blue.x), .y = to16Bit(colorimetry.blue.y)},
                    },
                .white_point                     = {.x = to16Bit(colorimetry.white.x), .y = to16Bit(colorimetry.white.y)},
                .max_display_mastering_luminance = toNits(luminances.max),
                .min_display_mastering_luminance = toNits(luminances.min * 10000),
                .max_cll                         = toNits(settings.maxCLL > 0 ? settings.maxCLL : monitor->maxCLL()),
                .max_fall                        = toNits(settings.maxFALL > 0 ? settings.maxFALL : monitor->maxFALL()),
            },
    };
}

static bool hdrMetadataEqual(const hdr_output_metadata& a, const hdr_output_metadata& b) {
    if (a.metadata_type != b.metadata_type)
        return false;

    static_assert(std::has_unique_object_representations_v<hdr_metadata_infoframe>);
    return std::memcmp(&a.hdmi_metadata_type1, &b.hdmi_metadata_type1, sizeof(a.hdmi_metadata_type1)) == 0;
}

void IHyprRenderer::handleFullscreenSettings(PHLMONITOR pMonitor) {
    static auto PCT        = CConfigValue<Config::INTEGER>("render:send_content_type");
    static auto PAUTOHDR   = CConfigValue<Config::INTEGER>("render:cm_auto_hdr");
    static auto PNONSHADER = CConfigValue<Config::INTEGER>("render:non_shader_cm");
    static auto PNSINTEROP = CConfigValue<Config::INTEGER>("render:non_shader_cm_interop");

    const bool  configuredHDR = (pMonitor->m_cmType == NCMType::CM_HDR_EDID || pMonitor->m_cmType == NCMType::CM_HDR);
    bool        wantHDR       = configuredHDR;

    const auto  FULLSCREEN_WINDOW = Fullscreen::controller()->getFullscreenWindow(pMonitor);

    if (pMonitor->supportsHDR()) {
        // HDR metadata determined by
        // HDR scRGB - monitor settings
        // HDR PQ surface & DS is active - surface settings

        bool hdrIsHandled = false;
        if (FULLSCREEN_WINDOW) {
            const auto ROOT_SURF = FULLSCREEN_WINDOW->wlSurface()->resource();
            const auto SURF      = ROOT_SURF->findWithCM();

            // we have a surface with image description
            if (SURF && SURF->m_colorManagement.valid() && SURF->m_colorManagement->hasImageDescription()) {
                const bool surfaceIsHDR = SURF->m_colorManagement->isHDR();
                wantHDR                 = *PAUTOHDR && surfaceIsHDR;
                if (FULLSCREEN_WINDOW && FULLSCREEN_WINDOW->m_ruleApplicator->noAutoHDR().valueOrDefault())
                    wantHDR = configuredHDR;
                if (surfaceIsHDR && !SURF->m_colorManagement->isWindowsScRGB() && !pMonitor->m_lastScanout.expired()) {
                    // DS HDR
                    bool needsHdrMetadataUpdate =
                        SURF->m_colorManagement->needsHdrMetadataUpdate() || pMonitor->m_previousFSWindow != FULLSCREEN_WINDOW || pMonitor->m_needsHDRupdate;
                    if (SURF->m_colorManagement->needsHdrMetadataUpdate()) {
                        Log::logger->log(Log::INFO, "[CM] Recreating HDR metadata for surface");
                        SURF->m_colorManagement->setHDRMetadata(createHDRMetadata(SURF->m_colorManagement->imageDescription(), pMonitor));
                    }
                    if (needsHdrMetadataUpdate) {
                        Log::logger->log(Log::INFO, "[CM] Updating HDR metadata from surface");
                        pMonitor->m_output->state->setHDRMetadata(SURF->m_colorManagement->hdrMetadata());
                        pMonitor->m_hdrMetadataFromSurface = true;
                    }
                    hdrIsHandled               = true;
                    pMonitor->m_needsHDRupdate = false;
                }
            }
        }

        // Do it here instead of disabling the block above to allow hdr -> hdr metadata changes in fullscreen
        if (!*PAUTOHDR && !pMonitor->m_lastScanout)
            wantHDR = configuredHDR;

        if (!hdrIsHandled) {
            const bool HDR_CHANGED = pMonitor->inHDR() != wantHDR;

            if (HDR_CHANGED && *PAUTOHDR && !(pMonitor->inHDR() && configuredHDR)) {
                const auto targetCM      = wantHDR ? (*PAUTOHDR == 2 ? NCMType::CM_HDR_EDID : NCMType::CM_HDR) : pMonitor->m_cmType;
                const auto targetSDREOTF = pMonitor->m_sdrEotf;
                Log::logger->log(Log::INFO, "[CM] Auto HDR: changing monitor cm to {}", sc<uint8_t>(targetCM));
                pMonitor->applyCMType(targetCM, targetSDREOTF);
                pMonitor->m_previousFSWindow.reset(); // trigger CTM update
            }

            const auto WANTED  = wantHDR ? createHDRMetadata(pMonitor->m_imageDescription->value(), pMonitor) : NO_HDR_METADATA;
            const auto CURRENT = pMonitor->m_output->state->state().hdrMetadata;

            if (HDR_CHANGED || pMonitor->m_hdrMetadataFromSurface || (wantHDR && !hdrMetadataEqual(WANTED, CURRENT))) {
                Log::logger->log(Log::INFO, wantHDR ? "[CM] Updating HDR metadata from monitor" : "[CM] Restoring SDR mode");
                pMonitor->m_output->state->setHDRMetadata(WANTED);
                pMonitor->m_hdrMetadataFromSurface = false;
            }
            pMonitor->m_needsHDRupdate = true;
        }
    }

    const bool needsWCG = pMonitor->wantsWideColor();
    if (pMonitor->m_output->state->state().wideColorGamut != needsWCG) {
        Log::logger->log(Log::TRACE, "Setting wide color gamut {}", needsWCG ? "on" : "off");
        pMonitor->m_output->state->setWideColorGamut(needsWCG);

        // FIXME do not trust enabled10bit, auto switch to 10bit and back if needed
        if (needsWCG && !pMonitor->m_enabled10bit) {
            Log::logger->log(Log::WARN, "Wide color gamut is enabled but the display is not in 10bit mode");
            static bool shown = false;
            if (!shown) {
                Notification::overlay()->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, {{"name", pMonitor->m_name}}), CHyprColor{}, 15000,
                                                         ICON_WARNING);
                shown = true;
            }
        }
    }

    if (*PCT)
        pMonitor->m_output->state->setContentType(NContentType::toDRM(FULLSCREEN_WINDOW ? FULLSCREEN_WINDOW->getContentType() : CONTENT_TYPE_NONE));

    if (FULLSCREEN_WINDOW != pMonitor->m_previousFSWindow || (!FULLSCREEN_WINDOW && pMonitor->m_noShaderCTM) || pMonitor->m_ctmUpdated) {
        const bool INTEROP  = (*PNSINTEROP == 1 || (*PNSINTEROP == 2 && FULLSCREEN_WINDOW && FULLSCREEN_WINDOW->getContentType() == CONTENT_TYPE_NONE));
        bool       resetCTM = !FULLSCREEN_WINDOW;
        if (FULLSCREEN_WINDOW) {
            if (*PNONSHADER == CM_NS_IGNORE)
                resetCTM = true;
            else if (const auto FS_DESC = pMonitor->getFSImageDescription(); pMonitor->needsCM() && pMonitor->canNoShaderCM(!pMonitor->m_lastScanout.expired()) &&
                     FS_DESC.has_value() && (*PNONSHADER != CM_NS_ONDEMAND || !pMonitor->m_lastScanout.expired())) {
                Log::logger->log(Log::INFO, "[CM] Updating fullscreen CTM");
                pMonitor->m_noShaderCTM = true;
                pMonitor->m_ctmUpdated  = false;
                auto conversion         = FS_DESC.value()->getPrimaries()->convertMatrix(pMonitor->m_imageDescription->getPrimaries());
                if (pMonitor->m_ctm != Mat3x3::identity() && INTEROP) {
                    const auto&                          ctm    = pMonitor->m_ctm.getMatrix();
                    std::array<std::array<double, 3>, 3> values = {
                        {
                            {ctm[0], ctm[1], ctm[2]},
                            {ctm[3], ctm[4], ctm[5]},
                            {ctm[6], ctm[7], ctm[8]},
                        },
                    };
                    conversion = conversion * Hyprgraphics::CMatrix3(values);
                }
                const auto                 mat = conversion.mat();
                const std::array<float, 9> CTM = {
                    mat[0][0], mat[0][1], mat[0][2], //
                    mat[1][0], mat[1][1], mat[1][2], //
                    mat[2][0], mat[2][1], mat[2][2], //
                };
                pMonitor->m_output->state->setCTM(CTM);
            } else if (!INTEROP && pMonitor->m_ctm != Mat3x3::identity()) {
                Log::logger->log(Log::INFO, "[CM] Setting identity CTM");
                pMonitor->m_noShaderCTM = true;
                pMonitor->m_ctmUpdated  = false;

                pMonitor->m_output->state->setCTM(Mat3x3::identity());
            } else
                resetCTM = true;
        }

        if (resetCTM && pMonitor->m_noShaderCTM) {
            Log::logger->log(Log::INFO, "[CM] No fullscreen CTM, restoring previous one");
            pMonitor->m_noShaderCTM = false;
            pMonitor->m_ctmUpdated  = true;
        }
    }

    if (pMonitor->m_ctmUpdated && !pMonitor->m_noShaderCTM) {
        pMonitor->m_ctmUpdated = false;
        pMonitor->m_output->state->setCTM(pMonitor->m_ctm);
    }

    pMonitor->m_previousFSWindow = FULLSCREEN_WINDOW;
}

bool IHyprRenderer::commitPendingAndDoExplicitSync(PHLMONITOR pMonitor, std::optional<Monitor::CDamageRing::CTransaction> damage, const CRegion& renderedDamage) {
    handleFullscreenSettings(pMonitor);

    const auto                                staged = renderedDamage.empty() ? pMonitor->m_commitCoordinator->takeStagedRender() : std::nullopt;

    Monitor::COutputCommitCoordinator::SFrame frame{
        .kind              = Monitor::COutputCommitCoordinator::FRAME_COMPOSED,
        .damage            = std::move(damage),
        .renderedDamage    = staged ? staged->damage : renderedDamage,
        .rollbackSwapchain = true,
        .tearing           = pMonitor->m_output->state->state().presentationMode == Aquamarine::AQ_OUTPUT_PRESENTATION_IMMEDIATE,
        .vrr               = pMonitor->m_vrrActive,
        .copyFBPrepared    = staged ? staged->copyFBPrepared : pMonitor->needsACopyFB(),
    };

    const auto result = pMonitor->m_commitCoordinator->submit(std::move(frame));
    const bool ok     = result != Monitor::COutputCommitCoordinator::SUBMIT_FAILED;
    if (!ok)
        Log::logger->log(Log::TRACE, "Monitor state commit failed");

    return ok;
}

void IHyprRenderer::renderWorkspace(CRenderingContext& context, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry) {
    Vector2D translate = {geometry.x, geometry.y};
    float    scale     = sc<float>(geometry.width) / pMonitor->m_transformedSize.x;

    TRACY_GPU_ZONE("RenderWorkspace");

    if (!DELTALESSTHAN(sc<double>(geometry.width) / sc<double>(geometry.height), pMonitor->m_transformedSize.x / pMonitor->m_transformedSize.y, 0.01)) {
        Log::logger->log(Log::ERR, "Ignoring geometry in renderWorkspace: aspect ratio mismatch");
        scale     = 1.f;
        translate = Vector2D{};
    }

    renderAllClientsForWorkspace(context, pMonitor, pWorkspace, now, translate, scale);
}

bool IHyprRenderer::renderWorkspaceToBuffer(CRenderingContext& context, PHLWORKSPACE workspace, SP<IFramebuffer> framebuffer, bool sendFeedback) {
    return renderWorkspaceToBufferInternal(context, workspace, framebuffer, Time::steadyNow(), sendFeedback, false);
}

bool IHyprRenderer::renderWorkspaceSceneToBuffer(CRenderingContext& context, PHLWORKSPACE workspace, SP<IFramebuffer> framebuffer, const Time::steady_tp& time, bool sendFeedback) {
    return renderWorkspaceToBufferInternal(context, workspace, framebuffer, time, sendFeedback, true);
}

bool IHyprRenderer::renderWorkspaceToBufferInternal(CRenderingContext& context, PHLWORKSPACE workspace, SP<IFramebuffer> framebuffer, const Time::steady_tp& time,
                                                    bool sendFeedback, bool fullScene) {
    const auto MONITOR = workspace ? workspace->m_monitor.lock() : nullptr;
    auto&      parent  = context;
    if (!MONITOR || !framebuffer || !framebuffer->isAllocated() || framebuffer->m_size != MONITOR->m_transformedSize || !parent.currentFB)
        return false;
    if (framebuffer == parent.currentFB || framebuffer == parent.mainFB || framebuffer == parent.outFB)
        return false;

    if (!framebuffer->imageDescription())
        framebuffer->setImageDescription(MONITOR->workBufferImageDescription());

    CRenderPass       pass;
    CRenderingContext child{parent, pass, MONITOR};
    CRegion           damage{0, 0, sc<int>(MONITOR->m_transformedSize.x), sc<int>(MONITOR->m_transformedSize.y)};

    child.isolatedWorkspace          = workspace;
    child.isolatedWorkspaceFullScene = fullScene;
    child.blockSurfaceFeedback       = !sendFeedback;
    child.renderingSnapshot          = !fullScene;
    child.precomputeBlur             = false;
    child.mainFB                     = framebuffer;
    child.outFB.reset();
    child.fbSize                     = MONITOR->m_transformedSize;
    child.damage                     = damage;
    child.finalDamage                = damage;
    child.renderModif                = {};
    child.mouseZoomFactor            = 1.F;
    child.mouseZoomUseMouse          = false;
    child.transformDamage            = false;
    child.noSimplify                 = false;
    child.renderingTransformedSource = false;
    child.blockScreenShader          = true;
    child.currentWindow.reset();
    child.surface.reset();
    child.clipBox = {};

    auto framebufferGuard = bindTempFB(child, framebuffer);
    setProjectionType(child, RPT_EXPORT);

    addPassElement(child, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)}));
    if (fullScene) {
        renderAllClientsForWorkspace(child, MONITOR, workspace, time);
        const CBox renderBox = {{}, MONITOR->m_transformedSize};
        renderLockscreen(child, MONITOR, time, renderBox);
        renderIME(child, MONITOR, time, renderBox);
    } else if (Fullscreen::controller()->hasFullscreen(workspace))
        renderWorkspaceWindowsFullscreen(child, MONITOR, workspace, time);
    else
        renderWorkspaceWindows(child, MONITOR, workspace, time);

    pass.render(child, damage);

    return true;
}

bool IHyprRenderer::renderTextureToBuffer(CRenderingContext& context, SP<ITexture> texture, SP<IFramebuffer> framebuffer) {
    if (!texture || !framebuffer || !framebuffer->isAllocated() || !context.currentFB || framebuffer == context.currentFB || framebuffer == context.mainFB ||
        framebuffer == context.outFB || texture == framebuffer->getTexture())
        return false;

    if (texture->m_imageDescription)
        framebuffer->setImageDescription(texture->m_imageDescription);

    CRenderingContext child{context, context.renderPass()};
    const CRegion     damage{0, 0, sc<int>(framebuffer->m_size.x), sc<int>(framebuffer->m_size.y)};
    auto              framebufferGuard = bindTempFB(child, framebuffer);
    child.fbSize                       = framebuffer->m_size;
    child.damage                       = damage;
    child.finalDamage                  = damage;
    child.mainFB                       = framebuffer;
    child.outFB.reset();
    child.transformDamage             = false;
    child.renderModif                 = {};
    child.mouseZoomFactor             = 1.F;
    child.mouseZoomUseMouse           = false;
    child.useNearestNeighbor          = false;
    child.blockScreenShader           = true;
    child.primarySurfaceUVTopLeft     = {-1, -1};
    child.primarySurfaceUVBottomRight = {-1, -1};
    child.clipBox                     = {};
    child.currentWindow.reset();
    child.surface.reset();
    child.isolatedWorkspace.reset();
    child.isolatedWorkspaceFullScene = false;
    child.blockSurfaceFeedback       = true;
    child.renderingSnapshot          = false;
    child.precomputeBlur             = false;
    child.updatesMonitorBlurState    = false;
    child.applyFinalScreenShader     = false;
    setProjectionType(child, RPT_EXPORT);

    draw(child, CClearPassElement::SClearData{CHyprColor(0.F, 0.F, 0.F, 0.F)}, damage);
    draw(child, CTexPassElement::SRenderData{.tex = texture, .box = {{}, framebuffer->m_size}, .damage = damage}, damage);
    return true;
}

bool IHyprRenderer::renderMonitorToBuffer(CRenderingContext& context, PHLMONITOR monitor, SP<IFramebuffer> framebuffer, const Time::steady_tp& time, bool sendFeedback) {
    auto& parent = context;
    if (!monitor || !framebuffer || !framebuffer->isAllocated() || framebuffer->m_size != monitor->m_transformedSize || !parent.currentFB || parent.sceneMonitor != monitor)
        return false;
    if (framebuffer == parent.currentFB || framebuffer == parent.mainFB || framebuffer == parent.outFB)
        return false;

    if (!framebuffer->imageDescription())
        framebuffer->setImageDescription(monitor->workBufferImageDescription());

    CRenderPass       pass;
    CRenderingContext child{parent, pass, monitor};
    CRegion           damage{0, 0, sc<int>(monitor->m_transformedSize.x), sc<int>(monitor->m_transformedSize.y)};

    child.isolatedWorkspace.reset();
    child.isolatedWorkspaceFullScene = false;
    child.blockSurfaceFeedback       = !sendFeedback;
    child.renderingSnapshot          = false;
    child.mainFB                     = framebuffer;
    child.outFB.reset();
    child.fbSize                     = monitor->m_transformedSize;
    child.damage                     = damage;
    child.finalDamage                = damage;
    child.renderModif                = {};
    child.mouseZoomFactor            = 1.F;
    child.mouseZoomUseMouse          = false;
    child.transformDamage            = false;
    child.noSimplify                 = false;
    child.renderingTransformedSource = false;
    child.blockScreenShader          = true;
    child.currentWindow.reset();
    child.surface.reset();
    child.clipBox = {};

    auto framebufferGuard = bindTempFB(child, framebuffer);
    setProjectionType(child, RPT_EXPORT);

    addPassElement(child, makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)}));
    CMonitorScene{monitor}.draw(child, time);

    pass.render(child, damage);

    return true;
}

void IHyprRenderer::sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now) {
    for (const auto& view : Desktop::View::getViewsForWorkspace(pWorkspace)) {
        if (!view->mapped() || !view->acceptsInput() || !view->resource())
            continue;

        const auto alphaModifier = dynamicPointerCast<Desktop::View::IAlphaModifiable>(view);
        if (alphaModifier && !alphaModifier->alphaNonZero())
            continue;

        view->wlSurface()->resource()->frame(now);
    }
}

void IHyprRenderer::setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor) {
    if (!PROTO::linuxDma)
        return;

    PROTO::linuxDma->updateScanoutTranche(surface, monitor);
}

// taken from Sway.
// this is just too much of a spaghetti for me to understand
static void applyExclusive(CBox& usableArea, uint32_t anchor, int32_t exclusive, uint32_t exclusiveEdge, int32_t marginTop, int32_t marginRight, int32_t marginBottom,
                           int32_t marginLeft) {
    if (exclusive <= 0) {
        return;
    }
    struct {
        uint32_t singular_anchor;
        uint32_t anchor_triplet;
        double*  positive_axis;
        double*  negative_axis;
        int      margin;
    } edges[] = {
        // Top
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .positive_axis   = &usableArea.y,
            .negative_axis   = &usableArea.height,
            .margin          = marginTop,
        },
        // Bottom
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = nullptr,
            .negative_axis   = &usableArea.height,
            .margin          = marginBottom,
        },
        // Left
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = &usableArea.x,
            .negative_axis   = &usableArea.width,
            .margin          = marginLeft,
        },
        // Right
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = nullptr,
            .negative_axis   = &usableArea.width,
            .margin          = marginRight,
        },
    };
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        if ((exclusiveEdge == edges[i].singular_anchor || anchor == edges[i].singular_anchor || anchor == edges[i].anchor_triplet) && exclusive + edges[i].margin > 0) {
            if (edges[i].positive_axis) {
                *edges[i].positive_axis += exclusive + edges[i].margin;
            }
            if (edges[i].negative_axis) {
                *edges[i].negative_axis -= exclusive + edges[i].margin;
            }
            break;
        }
    }
}

void IHyprRenderer::arrangeLayerArray(PHLMONITOR pMonitor, const std::vector<PHLLSREF>& layerSurfaces, bool exclusiveZone, CBox* usableArea) {
    CBox full_area = {pMonitor->m_position.x, pMonitor->m_position.y, pMonitor->m_size.x, pMonitor->m_size.y};

    for (auto const& ls : layerSurfaces) {
        if (!ls || !ls->m_layerSurface || (ls->m_flags & LAYER_FLAG_DEAD))
            continue;

        const auto PLAYER = ls->m_layerSurface;
        const auto PSTATE = &PLAYER->m_current;
        if (exclusiveZone != (PSTATE->exclusive > 0))
            continue;

        CBox bounds;
        if (PSTATE->exclusive == -1)
            bounds = full_area;
        else
            bounds = *usableArea;

        const Vector2D OLDSIZE = {ls->m_geometry.width, ls->m_geometry.height};

        CBox           box = {{}, PSTATE->desiredSize};
        // Horizontal axis
        const uint32_t both_horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        if (box.width == 0)
            box.x = bounds.x;
        else if ((PSTATE->anchor & both_horiz) == both_horiz)
            box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT))
            box.x = bounds.x;
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT))
            box.x = bounds.x + (bounds.width - box.width);
        else
            box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));

        // Vertical axis
        const uint32_t both_vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        if (box.height == 0)
            box.y = bounds.y;
        else if ((PSTATE->anchor & both_vert) == both_vert)
            box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP))
            box.y = bounds.y;
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
            box.y = bounds.y + (bounds.height - box.height);
        else
            box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));

        // Margin
        if (box.width == 0) {
            box.x += PSTATE->margin.left;
            box.width = bounds.width - (PSTATE->margin.left + PSTATE->margin.right);
        } else if ((PSTATE->anchor & both_horiz) == both_horiz)
            ; // don't apply margins
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT))
            box.x += PSTATE->margin.left;
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT))
            box.x -= PSTATE->margin.right;

        if (box.height == 0) {
            box.y += PSTATE->margin.top;
            box.height = bounds.height - (PSTATE->margin.top + PSTATE->margin.bottom);
        } else if ((PSTATE->anchor & both_vert) == both_vert)
            ; // don't apply margins
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP))
            box.y += PSTATE->margin.top;
        else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
            box.y -= PSTATE->margin.bottom;

        if (box.width <= 0 || box.height <= 0) {
            Log::logger->log(Log::ERR, "LayerSurface {:x} has a negative/zero w/h???", rc<uintptr_t>(ls.get()));
            continue;
        }

        box.round(); // fix rounding errors

        ls->m_geometry = box;

        applyExclusive(*usableArea, PSTATE->anchor, PSTATE->exclusive, PSTATE->exclusiveEdge, PSTATE->margin.top, PSTATE->margin.right, PSTATE->margin.bottom, PSTATE->margin.left);

        if (Vector2D{box.width, box.height} != OLDSIZE)
            ls->m_layerSurface->configure(box.size());

        ls->setBox(box);
    }
}

void IHyprRenderer::arrangeLayersForMonitor(const MONITORID& monitor) {
    const auto PMONITOR = State::monitorState()->query().id(monitor).run();

    if (!PMONITOR || PMONITOR->m_size.x <= 0 || PMONITOR->m_size.y <= 0)
        return;

    // Reset the reserved
    PMONITOR->m_reservedArea.resetType(Desktop::RESERVED_DYNAMIC_TYPE_LS);

    const CBox ORIGINAL_USABLE_AREA = PMONITOR->logicalBoxMinusReserved();
    CBox       usableArea           = ORIGINAL_USABLE_AREA;

    for (auto& la : PMONITOR->m_layerSurfaceLayers) {
        std::ranges::stable_sort(
            la, [](const PHLLSREF& a, const PHLLSREF& b) { return a->m_ruleApplicator->order().valueOrDefault() > b->m_ruleApplicator->order().valueOrDefault(); });
    }

    for (auto const& la : PMONITOR->m_layerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, true, &usableArea);

    for (auto const& la : PMONITOR->m_layerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, false, &usableArea);

    PMONITOR->m_reservedArea.addType(Desktop::RESERVED_DYNAMIC_TYPE_LS, Desktop::CReservedArea{ORIGINAL_USABLE_AREA, usableArea});

    // damage the monitor if can
    damageMonitor(PMONITOR);

    g_layoutManager->invalidateMonitorGeometries(PMONITOR);
}

void IHyprRenderer::damageSurface(SP<CWLSurfaceResource> pSurface, double x, double y, double scale) {
    if (!pSurface)
        return; // wut?

    const auto WLSURF = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!WLSURF) {
        Log::logger->log(Log::ERR, "BUG THIS: No CWLSurface for surface in damageSurface!!!");
        return;
    }

    const auto SURFACE_BOX = WLSURF->getSurfaceBoxGlobal();

    // hack: schedule frame events
    if (!pSurface->m_current.callbacks.empty() && SURFACE_BOX && !SURFACE_BOX->empty()) {
        for (auto const& m : State::monitorState()->monitors()) {
            if (!m->m_output)
                continue;

            if (SURFACE_BOX->overlaps(m->logicalBox()))
                m->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
        }
    }

    CRegion damageBox = WLSURF->computeDamage(SURFACE_BOX);
    if (damageBox.empty())
        return;

    if (scale != 1.0)
        damageBox.scale(scale);

    damageBox.translate({x, y});

    const auto EXTENTS = damageBox.getExtents();

    CRegion    damageBoxForEach;

    for (auto const& m : State::monitorState()->monitors()) {
        if (!m->m_output)
            continue;
        if (!EXTENTS.overlaps(m->logicalBox()))
            continue;

        damageBoxForEach.set(damageBox);
        damageBoxForEach.translate({-m->m_position.x, -m->m_position.y}).scale(m->m_scale);

        m->addDamage(damageBoxForEach);
    }

    static auto PLOGDAMAGE = CConfigValue<Config::INTEGER>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Surface (extents): xy: {}, {} wh: {}, {}", damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y1,
                         damageBox.pixman()->extents.x2 - damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y2 - damageBox.pixman()->extents.y1);
}

void IHyprRenderer::damageWindow(PHLWINDOW pWindow, bool forceFull) {
    CBox       windowBox         = pWindow->getFullWindowBoundingBox();
    const auto PWINDOWWORKSPACE  = pWindow->m_workspace;
    const auto PWORKSPACEMONITOR = PWINDOWWORKSPACE ? PWINDOWWORKSPACE->m_monitor.lock() : nullptr;
    if (PWINDOWWORKSPACE && PWORKSPACEMONITOR && PWORKSPACEMONITOR->m_workspaceTransition->isAnimating(PWINDOWWORKSPACE) && !(pWindow->m_state & WINDOW_STATE_PINNED))
        windowBox.translate(PWORKSPACEMONITOR->m_workspaceTransition->offsetValue(PWINDOWWORKSPACE));
    windowBox.translate(pWindow->presentation().floatingOffset());
    windowBox = pWindow->effects().transformBoxForDamage(windowBox);

    const bool OVERVIEW_SELECTED = Overview::overview()->shouldRenderWorkspace(PWINDOWWORKSPACE);
    for (auto const& m : State::monitorState()->monitors()) {
        if (forceFull || shouldRenderWindowOnMonitor(pWindow, m) || (OVERVIEW_SELECTED && m == PWORKSPACEMONITOR)) { // only damage if window is rendered on monitor
            CBox fixedDamageBox = {windowBox.x - m->m_position.x, windowBox.y - m->m_position.y, windowBox.width, windowBox.height};
            fixedDamageBox.scale(m->m_scale).round();
            m->addDamage(fixedDamageBox);
        }
    }

    static auto PLOGDAMAGE = CConfigValue<Config::INTEGER>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Window ({}): xy: {}, {} wh: {}, {}", pWindow->metadata().title(), windowBox.x, windowBox.y, windowBox.width, windowBox.height);
}

void IHyprRenderer::damageMonitor(PHLMONITOR pMonitor) {
    if (!pMonitor || pMonitor->isMirror())
        return;

    CBox damageBox = {0, 0, INT16_MAX, INT16_MAX};
    pMonitor->addDamage(damageBox);

    static auto PLOGDAMAGE = CConfigValue<Config::INTEGER>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Monitor {}", pMonitor->m_name);
}

void IHyprRenderer::damageBox(const CBox& box, bool skipFrameSchedule) {
    for (auto const& m : State::monitorState()->monitors()) {
        if (m->isMirror())
            continue; // don't damage mirrors traditionally

        if (!skipFrameSchedule) {
            CBox damageBox = box.copy().translate(-m->m_position).scale(m->m_scale).round();
            m->addDamage(damageBox);
        }
    }

    static auto PLOGDAMAGE = CConfigValue<Config::INTEGER>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Box: xy: {}, {} wh: {}, {}", box.x, box.y, box.w, box.h);
}

void IHyprRenderer::damageBox(const int& x, const int& y, const int& w, const int& h) {
    CBox box = {x, y, w, h};
    damageBox(box);
}

void IHyprRenderer::damageRegion(const CRegion& rg) {
    rg.forEachRect([this](const auto& RECT) { damageBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1); });
}

void IHyprRenderer::damageMirrorsWith(PHLMONITOR pMonitor, const CRegion& pRegion) {
    for (auto const& mirror : pMonitor->m_mirrors) {

        // transform the damage here, so it won't get clipped by the monitor damage ring
        auto    monitor = mirror;

        CRegion transformed{pRegion};

        // we want to transform to the same box as in CHyprOpenGLImpl::renderMirrored
        double scale  = std::min(monitor->m_transformedSize.x / pMonitor->m_transformedSize.x, monitor->m_transformedSize.y / pMonitor->m_transformedSize.y);
        CBox   monbox = {0, 0, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
        monbox.x      = (monitor->m_transformedSize.x - monbox.w) / 2;
        monbox.y      = (monitor->m_transformedSize.y - monbox.h) / 2;

        transformed.scale(scale);
        transformed.translate(Vector2D(monbox.x, monbox.y));

        mirror->addDamage(transformed);

        if (auto m = mirror.lock())
            m->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }
}

void IHyprRenderer::renderDragIcon(CRenderingContext& context, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    PROTO::data->renderDND(context, pMonitor, time);
}

void IHyprRenderer::setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force) {
    m_cursorHasSurface = surf && surf->resource();

    m_lastCursorData.name     = "";
    m_lastCursorData.surf     = surf;
    m_lastCursorData.hotspotX = hotspotX;
    m_lastCursorData.hotspotY = hotspotY;

    if (m_cursorHidden && !force)
        return;

    Pointer::Cursor::mgr()->setCursorSurface(surf, {hotspotX, hotspotY});
}

void IHyprRenderer::setCursorFromName(const std::string& name, bool force) {
    m_cursorHasSurface = true;

    if (name == m_lastCursorData.name && !force)
        return;

    m_lastCursorData.name = name;

    static auto getShapeOrDefault = [](std::string_view name) -> wpCursorShapeDeviceV1Shape {
        const auto it = std::ranges::find(CURSOR_SHAPE_NAMES, name);

        if (it == CURSOR_SHAPE_NAMES.end()) {
            // clang-format off
            static const auto overrites = std::unordered_map<std::string_view, wpCursorShapeDeviceV1Shape> {
              {"top_side",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE},
              {"bottom_side",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE},
              {"left_side",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE},
              {"right_side",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE},
              {"top_left_corner",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE},
              {"bottom_left_corner",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE},
              {"top_right_corner",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE},
              {"bottom_right_corner",  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE},
            };
            // clang-format on

            if (overrites.contains(name))
                return overrites.at(name);

            return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
        }

        return sc<wpCursorShapeDeviceV1Shape>(std::distance(CURSOR_SHAPE_NAMES.begin(), it));
    };

    const auto newShape = getShapeOrDefault(name);

    if (newShape != m_lastCursorData.shape) {
        m_lastCursorData.shapePrevious = m_lastCursorData.shape;
        m_lastCursorData.switchedTimer.reset();
    }

    m_lastCursorData.shape = newShape;

    m_lastCursorData.surf.reset();

    if (m_cursorHidden && !force)
        return;

    Pointer::Cursor::mgr()->setCursorFromName(name);
}

void IHyprRenderer::ensureCursorRenderingMode() {
    static auto PINVISIBLE     = CConfigValue<Config::INTEGER>("cursor:invisible");
    static auto PCURSORTIMEOUT = CConfigValue<Config::FLOAT>("cursor:inactive_timeout");
    static auto PHIDEONTOUCH   = CConfigValue<Config::INTEGER>("cursor:hide_on_touch");
    static auto PHIDEONTABLET  = CConfigValue<Config::INTEGER>("cursor:hide_on_tablet");
    static auto PHIDEONKEY     = CConfigValue<Config::INTEGER>("cursor:hide_on_key_press");

    if (*PCURSORTIMEOUT <= 0)
        m_cursorHiddenConditions.hiddenOnTimeout = false;
    if (*PHIDEONTOUCH == 0)
        m_cursorHiddenConditions.hiddenOnTouch = false;
    if (*PHIDEONTABLET == 0)
        m_cursorHiddenConditions.hiddenOnTablet = false;
    if (*PHIDEONKEY == 0)
        m_cursorHiddenConditions.hiddenOnKeyboard = false;

    if (*PCURSORTIMEOUT > 0)
        m_cursorHiddenConditions.hiddenOnTimeout = *PCURSORTIMEOUT < g_pInputManager->m_lastCursorMovement.getSeconds();

    m_cursorHiddenByCondition =
        m_cursorHiddenConditions.hiddenOnTimeout || m_cursorHiddenConditions.hiddenOnTouch || m_cursorHiddenConditions.hiddenOnTablet || m_cursorHiddenConditions.hiddenOnKeyboard;

    const bool HIDE = m_cursorHiddenByCondition || (*PINVISIBLE != 0) || PROTO::inputCapture->isCaptured();

    if (HIDE == m_cursorHidden)
        return;

    if (HIDE)
        Log::logger->log(Log::DEBUG, "Hiding the cursor (hl-mandated)");
    else
        Log::logger->log(Log::DEBUG, "Showing the cursor (hl-mandated)");

    for (auto const& m : State::monitorState()->monitors()) {
        if (!Pointer::mgr()->softwareLockedFor(m))
            continue;

        Pointer::mgr()->damageCursor(m, m->shouldSkipScheduleFrameOnMouseEvent());
    }

    setCursorHidden(HIDE);
}

void IHyprRenderer::setCursorHidden(bool hide) {

    if (hide == m_cursorHidden)
        return;

    m_cursorHidden = hide;

    if (hide) {
        Pointer::mgr()->resetCursorImage();
        return;
    }

    if (m_lastCursorData.surf.has_value())
        setCursorSurface(m_lastCursorData.surf.value(), m_lastCursorData.hotspotX, m_lastCursorData.hotspotY, true);
    else if (!m_lastCursorData.name.empty())
        setCursorFromName(m_lastCursorData.name, true);
    else
        setCursorFromName("left_ptr", true);
}

bool IHyprRenderer::shouldRenderCursor() {
    return !m_cursorHidden && m_cursorHasSurface;
}

std::tuple<float, float, float> IHyprRenderer::getRenderTimes(PHLMONITOR pMonitor) {
    const auto POVERLAY = &Debug::overlay()->m_monitorOverlays[pMonitor];

    float      avgRenderTime = 0;
    float      maxRenderTime = 0;
    float      minRenderTime = 9999;
    for (auto const& rt : POVERLAY->m_lastRenderTimes) {
        maxRenderTime = std::max(rt, maxRenderTime);
        minRenderTime = std::min(rt, minRenderTime);
        avgRenderTime += rt;
    }
    avgRenderTime /= POVERLAY->m_lastRenderTimes.empty() ? 1 : POVERLAY->m_lastRenderTimes.size();

    return std::make_tuple<>(avgRenderTime, maxRenderTime, minRenderTime);
}

static int handleCrashLoop(void* data) {

    Notification::overlay()->addNotification(std::format("Hyprland will crash in {}s.", 10 - sc<int>(g_pHyprRenderer->m_crashingDistort * 2.f)), CHyprColor(0), 5000, ICON_INFO);

    g_pHyprRenderer->m_crashingDistort += 0.5f;

    if (g_pHyprRenderer->m_crashingDistort >= 5.5f)
        raise(SIGABRT);

    wl_event_source_timer_update(g_pHyprRenderer->m_crashingLoop, 1000);

    return 1;
}

void IHyprRenderer::initiateManualCrash() {
    Notification::overlay()->addNotification("Manual crash initiated. Farewell...", CHyprColor(0), 5000, ICON_INFO);

    m_crashingLoop = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_crashingLoop, 1000);

    m_crashingInProgress = true;
    m_crashingDistort    = 0.5;

    m_globalTimer.reset();

    **rc<Config::INTEGER* const*>(Config::mgr()->getConfigValue("debug:damage_tracking").dataptr) = 0;
}

SP<IRenderbuffer> IHyprRenderer::getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto it = std::ranges::find_if(m_renderbuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });

    if (it != m_renderbuffers.end())
        return *it;

    auto buf = getOrCreateRenderbufferInternal(buffer, fmt);

    if (!buf->good())
        return nullptr;

    m_renderbuffers.emplace_back(buf);
    return buf;
}

bool IHyprRenderer::beginFullFakeRender(CRenderingContext& context, CRegion& damage, SP<IFramebuffer> fb) {
    return beginRender(context, damage, RENDER_MODE_FULL_FAKE, nullptr, fb, true);
}

bool IHyprRenderer::beginRenderToBuffer(CRenderingContext& context, CRegion& damage, SP<IHLBuffer> buffer, bool simple) {
    return beginRender(context, damage, RENDER_MODE_TO_BUFFER, buffer, nullptr, simple);
}

void IHyprRenderer::onRenderbufferDestroy(IRenderbuffer* rb) {
    std::erase_if(m_renderbuffers, [&](const auto& rbo) { return rbo.get() == rb; });
}

bool IHyprRenderer::isNvidia() {
    return m_nvidia;
}

bool IHyprRenderer::isIntel() {
    return m_intel;
}

bool IHyprRenderer::isSoftware() {
    return m_software;
}

bool IHyprRenderer::isMgpu() {
    return m_mgpu;
}

void IHyprRenderer::addWindowToRenderUnfocused(PHLWINDOW window) {
    static auto PFPS = CConfigValue<Config::INTEGER>("misc:render_unfocused_fps");

    if (*PFPS <= 0)
        return;

    if (std::ranges::find(m_renderUnfocused, window) != m_renderUnfocused.end())
        return;

    m_renderUnfocused.emplace_back(window);

    if (!m_renderUnfocusedTimer->armed())
        m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
}

SP<IFramebuffer> IHyprRenderer::makeSnapshotFB(PHLWINDOW pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return nullptr;

    if (!shouldRenderWindow(pWindow))
        return nullptr; // ignore, window is not being rendered

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion    fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    const auto PFRAMEBUFFER = createFB("window snapshot");

    PFRAMEBUFFER->alloc(PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y, DRM_FORMAT_ABGR8888);
    PFRAMEBUFFER->setImageDescription(PMONITOR->workBufferImageDescription());

    CRenderPass       pass;
    CRenderingContext context{PMONITOR, pass};
    beginFullFakeRender(context, fakeDamage, PFRAMEBUFFER);

    context.renderingSnapshot = true;

    draw(context, CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
    startRenderPass(context);

    Log::logger->log(Log::DEBUG, "renderer: cleared a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    renderWindow(context, pWindow, PMONITOR, Time::steadyNow(), !pWindow->backend().traits().suggestsNoBorder, RENDER_PASS_ALL);

    Log::logger->log(Log::DEBUG, "renderer: rendered a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    endRender(context);

    Log::logger->log(Log::DEBUG, "renderer: made a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    return PFRAMEBUFFER;
}

SP<IFramebuffer> IHyprRenderer::makeSnapshotFB(PHLLS pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = pLayer->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return nullptr;

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of layer {:x}", rc<uintptr_t>(pLayer.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion    fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    const auto PFRAMEBUFFER = createFB("layer snapshot");

    PFRAMEBUFFER->alloc(PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y, DRM_FORMAT_ABGR8888);
    PFRAMEBUFFER->setImageDescription(PMONITOR->workBufferImageDescription());

    CRenderPass       pass;
    CRenderingContext context{PMONITOR, pass};
    beginFullFakeRender(context, fakeDamage, PFRAMEBUFFER);

    context.renderingSnapshot = true;

    draw(context, CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
    startRenderPass(context);

    Log::logger->log(Log::DEBUG, "renderer: cleared a snapshot of layer {:x}", rc<uintptr_t>(pLayer.get()));

    // draw the layer
    renderLayer(context, pLayer, PMONITOR, Time::steadyNow());

    Log::logger->log(Log::DEBUG, "renderer: rendered a snapshot of layer {:x}", rc<uintptr_t>(pLayer.get()));

    endRender(context);

    Log::logger->log(Log::DEBUG, "renderer: made a snapshot of layer {:x}", rc<uintptr_t>(pLayer.get()));

    return PFRAMEBUFFER;
}

SP<IFramebuffer> IHyprRenderer::makeSnapshotFB(WP<Desktop::View::CPopup> popup) {
    // we trust the window is valid.
    const auto PMONITOR = popup->getMonitor();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return nullptr;

    if (!popup->mapped() || !popup->acceptsInput() || !popup->alphaNonZero())
        return nullptr;

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(popup.get()));

    CRegion    fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    const auto PFRAMEBUFFER = createFB("popup shapshot");

    PFRAMEBUFFER->alloc(PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y, DRM_FORMAT_ABGR8888);
    PFRAMEBUFFER->setImageDescription(PMONITOR->workBufferImageDescription());

    CRenderPass       pass;
    CRenderingContext context{PMONITOR, pass};
    beginFullFakeRender(context, fakeDamage, PFRAMEBUFFER);

    context.renderingSnapshot = true;

    draw(context, CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});

    CSurfacePassElement::SRenderData renderdata;
    renderdata.pos             = popup->coordsGlobal();
    renderdata.alpha           = 1.F;
    renderdata.dontRound       = true; // don't round popups
    renderdata.pMonitor        = PMONITOR;
    renderdata.squishOversized = false; // don't squish popups
    renderdata.popup           = true;
    renderdata.blur            = false;

    popup->wlSurface()->resource()->breadthfirst(
        [this, &context, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = false;
            addPassElement(context, makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        nullptr);

    endRender(context);

    return PFRAMEBUFFER;
}

void IHyprRenderer::renderFadeouts(CRenderingContext& context, PHLMONITOR monitor, Desktop::eFadeoutPlane plane, PHLWORKSPACE workspace) {
    if (!monitor)
        return;

    std::vector<SP<Desktop::IFadeout>> fadeouts;
    for (auto const& fadeout : Desktop::fadingOutState()->fadeouts()) {
        if (!fadeout || fadeout->monitor() != monitor || fadeout->plane() != plane)
            continue;

        if (fadeout->workspace() && fadeout->workspace() != workspace)
            continue;

        fadeouts.emplace_back(fadeout);
    }

    std::ranges::sort(fadeouts, {}, [](const auto& fadeout) { return fadeout->zIndex(); });

    CRegion fakeDamage{0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y};
    for (auto const& fadeout : fadeouts) {
        const auto FB = fadeout->framebuffer();
        if (!FB || !FB->getTexture())
            continue;

        const auto EFFECTS = fadeout->effects();

        if (EFFECTS.dimAroundAlpha > 0.F) {
            CRectPassElement::SRectData data;
            data.box   = {0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y};
            data.color = CHyprColor(0, 0, 0, EFFECTS.dimAroundAlpha);
            addPassElement(context, makeUnique<CRectPassElement>(data));
        }

        if (EFFECTS.preBlur) {
            CRectPassElement::SRectData data;
            data.box           = EFFECTS.preBlur->box;
            data.color         = CHyprColor(0, 0, 0, 0);
            data.blur          = true;
            data.blurA         = EFFECTS.preBlur->alpha;
            data.round         = EFFECTS.preBlur->round;
            data.roundingPower = EFFECTS.preBlur->roundingPower;
            data.xray          = EFFECTS.preBlur->xray;
            addPassElement(context, makeUnique<CRectPassElement>(data));
        }

        CTexPassElement::SRenderData data;
        data.tex                   = FB->getTexture();
        data.box                   = fadeout->renderBox();
        data.a                     = fadeout->alpha();
        data.damage                = fakeDamage;
        data.blur                  = EFFECTS.textureBlur.enabled;
        data.blurA                 = EFFECTS.textureBlur.alpha;
        data.forceBlurBlend        = EFFECTS.textureBlur.forceBlend;
        data.blurShapeInvalid      = true;
        data.ignoreAlpha           = EFFECTS.textureBlur.ignoreAlpha;
        data.blockBlurOptimization = EFFECTS.textureBlur.blockBlurOptimization;

        addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
    }
}

NColorManagement::PImageDescription IHyprRenderer::workBufferImageDescription(const CRenderingContext& context) {
    if (!context.sceneMonitor)
        return LINEAR_IMAGE_DESCRIPTION;

    return context.sceneMonitor->workBufferImageDescription();
}

bool IHyprRenderer::shouldBlur(const CRenderingContext& context, PHLLS ls) {
    if (context.renderingSnapshot)
        return false;

    static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    if (!*PBLUR)
        return false;

    auto surface = ls->wlSurface();
    if (surface && surface->m_hasBackgroundEffect)
        return !surface->m_blurRegion.empty();

    return ls->m_ruleApplicator->blur().valueOrDefault();
}

bool IHyprRenderer::shouldBlur(const CRenderingContext& context, PHLWINDOW w) {
    if (context.renderingSnapshot)
        return false;

    static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    if (!*PBLUR)
        return false;

    const bool DONT_BLUR = w->m_ruleApplicator->noBlur().valueOrDefault() || w->m_ruleApplicator->RGBX().valueOrDefault() || w->presentation().opaque();
    if (DONT_BLUR)
        return false;

    auto surface = w->wlSurface();
    if (surface && surface->m_hasBackgroundEffect)
        return !surface->m_blurRegion.empty();

    return true;
}

bool IHyprRenderer::shouldBlur(const CRenderingContext&, WP<Desktop::View::CPopup> p) {
    static CConfigValue PBLURPOPUPS = CConfigValue<Config::INTEGER>("decoration:blur:popups");
    static CConfigValue PBLUR       = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    return *PBLURPOPUPS && *PBLUR;
}

SP<ITexture> IHyprRenderer::renderSplash(const std::function<SP<ITexture>(const int, const int, unsigned char* const)>& handleData, const int fontSize, const int maxWidth,
                                         const int maxHeight) {
    static auto PSPLASHCOLOR = CConfigValue<Config::INTEGER>("misc:col.splash");
    static auto PSPLASHFONT  = CConfigValue<std::string>("misc:splash_font_family");
    static auto FALLBACKFONT = CConfigValue<std::string>("misc:font_family");

    const auto  FONTFAMILY = *PSPLASHFONT != STRVAL_EMPTY ? *PSPLASHFONT : *FALLBACKFONT;
    const auto  COLOR      = CHyprColor(*PSPLASHCOLOR);

    const auto  CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, maxWidth, maxHeight);
    const auto  CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_antialias(CAIRO, CAIRO_ANTIALIAS_GOOD);
    cairo_save(CAIRO);
    cairo_set_source_rgba(CAIRO, 0, 0, 0, 0);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_SOURCE);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, fontSize * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layoutText, pangoFD);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);
    int textW = 0, textH = 0;
    pango_layout_set_text(layoutText, g_pCompositor->m_currentSplash.c_str(), -1);
    pango_layout_get_size(layoutText, &textW, &textH);
    textW = std::ceil((float)textW / PANGO_SCALE + fontSize / 10.f);
    textH = std::ceil((float)textH / PANGO_SCALE + fontSize / 10.f);

    cairo_move_to(CAIRO, 0, 0);
    pango_cairo_show_layout(CAIRO, layoutText);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);

    const auto smallSurf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textW, textH);
    const auto small     = cairo_create(smallSurf);
    cairo_set_source_surface(small, CAIROSURFACE, 0, 0);
    cairo_rectangle(small, 0, 0, textW, textH);
    cairo_set_operator(small, CAIRO_OPERATOR_SOURCE);
    cairo_fill(small);
    cairo_surface_flush(smallSurf);

    auto tex = handleData(textW, textH, cairo_image_surface_get_data(smallSurf));

    cairo_surface_destroy(smallSurf);
    cairo_destroy(small);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);
    return tex;
}

using ColorConversionKey = std::tuple<float, float, float, float, uint64_t>;

struct SColorConversionKeyHash {
    size_t operator()(const ColorConversionKey& key) const {
        size_t hash = 0;

        // fold each tuple element to a order sensitive hash the constant is
        // the 64-bit golden-ratio value used to
        // distribute bits and reduce collisions between adjacent fields.
        const auto hashCombine = [&hash](const auto& value) { hash ^= std::hash<std::decay_t<decltype(value)>>{}(value) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2); };

        hashCombine(std::get<0>(key));
        hashCombine(std::get<1>(key));
        hashCombine(std::get<2>(key));
        hashCombine(std::get<3>(key));
        hashCombine(std::get<4>(key));

        return hash;
    }
};

constexpr const size_t MAX_COLOR_CONVERSION_CACHE_SIZE = 4096;

static auto            colorConversionCache = []() {
    std::unordered_map<ColorConversionKey, CHyprColor, SColorConversionKeyHash> cache;
    cache.reserve(MAX_COLOR_CONVERSION_CACHE_SIZE);
    return cache;
}();

//
CHyprColor IHyprRenderer::getConvertedColor(const CRenderingContext& context, const CHyprColor& color) {
    const auto DESCR = context.currentFB ? context.currentFB->imageDescription() : workBufferImageDescription(context);

    if (!DESCR) {
        Log::logger->log(Log::ERR, "getConvertedColor: failed to get image description");
        return color;
    }

    if (colorConversionCache.size() >= MAX_COLOR_CONVERSION_CACHE_SIZE)
        colorConversionCache.clear();

    const ColorConversionKey key = {color.r, color.g, color.b, color.a, DESCR->id()};

    if (const auto IT = colorConversionCache.find(key); IT != colorConversionCache.end())
        return IT->second;

    const auto converted = convertColor(context, color, DEFAULT_SRGB_IMAGE_DESCRIPTION, DESCR);
    colorConversionCache.emplace(key, converted);

    return converted;
}
