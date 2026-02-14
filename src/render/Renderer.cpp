#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <cmath>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../managers/CursorManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../desktop/view/Window.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/GlobalViewMethods.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/LinuxDMABUF.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../debug/HyprDebugOverlay.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../i18n/Engine.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/CursorShapes.hpp"
#include "helpers/MainLoopExecutor.hpp"
#include "helpers/Monitor.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/RendererHintsPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "debug/log/Logger.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "render/AsyncResourceGatherer.hpp"
#include "render/Framebuffer.hpp"
#include "render/OpenGL.hpp"
#include "render/Texture.hpp"
#include "render/pass/BorderPassElement.hpp"
#include "render/pass/PreBlurElement.hpp"
#include <pango/pangocairo.h>

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;
using namespace NColorManagement;

extern "C" {
#include <xf86drm.h>
}

static int cursorTicker(void* data) {
    g_pHyprRenderer->ensureCursorRenderingMode();
    wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 500);
    return 0;
}

IHyprRenderer::IHyprRenderer() {
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

    static auto P = g_pHookSystem->hookDynamic("keyPress", [&](void* self, SCallbackInfo& info, std::any param) {
        if (m_cursorHiddenConditions.hiddenOnKeyboard)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = true;
        ensureCursorRenderingMode();
    });

    static auto P2 = g_pHookSystem->hookDynamic("mouseMove", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_cursorHiddenConditions.hiddenOnKeyboard && m_cursorHiddenConditions.hiddenOnTouch == g_pInputManager->m_lastInputTouch &&
            m_cursorHiddenConditions.hiddenOnTablet == g_pInputManager->m_lastInputTablet && !m_cursorHiddenConditions.hiddenOnTimeout)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = false;
        m_cursorHiddenConditions.hiddenOnTimeout  = false;
        m_cursorHiddenConditions.hiddenOnTouch    = g_pInputManager->m_lastInputTouch;
        m_cursorHiddenConditions.hiddenOnTablet   = g_pInputManager->m_lastInputTablet;
        ensureCursorRenderingMode();
    });

    static auto P3 = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        g_pEventLoopManager->doLater([this]() {
            if (!g_pHyprError->active())
                return;
            for (auto& m : g_pCompositor->m_monitors) {
                arrangeLayersForMonitor(m->m_id);
            }
        });
    });

    static auto P4 = g_pHookSystem->hookDynamic("windowUpdateRules", [&](void* self, SCallbackInfo& info, std::any param) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(param);

        if (PWINDOW->m_ruleApplicator->renderUnfocused().valueOrDefault())
            addWindowToRenderUnfocused(PWINDOW);
    });

    m_cursorTicker = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, cursorTicker, nullptr);
    wl_event_source_timer_update(m_cursorTicker, 500);

    m_renderUnfocusedTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            static auto PFPS = CConfigValue<Hyprlang::INT>("misc:render_unfocused_fps");

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

                w->wlSurface()->resource()->frame(Time::steadyNow());
                auto FEEDBACK = makeUnique<CQueuedPresentationData>(w->wlSurface()->resource());
                FEEDBACK->attachMonitor(Desktop::focusState()->monitor());
                FEEDBACK->discarded();
                PROTO::presentation->queueData(std::move(FEEDBACK));
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

bool IHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    if (!pWindow->visibleOnMonitor(pMonitor))
        return false;

    if (!pWindow->m_workspace && !pWindow->m_fadingOut)
        return false;

    if (!pWindow->m_workspace && pWindow->m_fadingOut)
        return pWindow->workspaceID() == pMonitor->activeWorkspaceID() || pWindow->workspaceID() == pMonitor->activeSpecialWorkspaceID();

    if (pWindow->m_pinned)
        return true;

    // if the window is being moved to a workspace that is not invisible, and the alpha is > 0.F, render it.
    if (pWindow->m_monitorMovedFrom != -1 && pWindow->m_movingToWorkspaceAlpha->isBeingAnimated() && pWindow->m_movingToWorkspaceAlpha->value() > 0.F && pWindow->m_workspace &&
        !pWindow->m_workspace->isVisible())
        return true;

    const auto PWINDOWWORKSPACE = pWindow->m_workspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_monitor == pMonitor) {
        if (PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() || PWINDOWWORKSPACE->m_alpha->isBeingAnimated() || PWINDOWWORKSPACE->m_forceRendering)
            return true;

        // if hidden behind fullscreen
        if (PWINDOWWORKSPACE->m_hasFullscreenWindow && !pWindow->isFullscreen() && (!pWindow->m_isFloating || !pWindow->m_createdOverFullscreen) && pWindow->m_alpha->value() == 0)
            return false;

        if (!PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOWWORKSPACE->m_alpha->isBeingAnimated() && !PWINDOWWORKSPACE->isVisible())
            return false;
    }

    if (pWindow->m_monitor == pMonitor)
        return true;

    if ((!pWindow->m_workspace || !pWindow->m_workspace->isVisible()) && pWindow->m_monitor != pMonitor)
        return false;

    // if not, check if it maybe is active on a different monitor.
    if (pWindow->m_workspace && pWindow->m_workspace->isVisible() && pWindow->m_isFloating /* tiled windows can't be multi-ws */)
        return !pWindow->isFullscreen(); // Do not draw fullscreen windows on other monitors

    if (pMonitor->m_activeSpecialWorkspace == pWindow->m_workspace)
        return true;

    // if window is tiled and it's flying in, don't render on other mons (for slide)
    if (!pWindow->m_isFloating && pWindow->m_realPosition->isBeingAnimated() && pWindow->m_animatingIn && pWindow->m_monitor != pMonitor)
        return false;

    if (pWindow->m_realPosition->isBeingAnimated()) {
        if (PWINDOWWORKSPACE && !PWINDOWWORKSPACE->m_isSpecialWorkspace && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated())
            return false;
        // render window if window and monitor intersect
        // (when moving out of or through a monitor)
        CBox windowBox = pWindow->getFullWindowBoundingBox();
        if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated())
            windowBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
        windowBox.translate(pWindow->m_floatingOffset);

        const CBox monitorBox = {pMonitor->m_position, pMonitor->m_size};
        if (!windowBox.intersection(monitorBox).empty() && (pWindow->workspaceID() == pMonitor->activeWorkspaceID() || pWindow->m_monitorMovedFrom != -1))
            return true;
    }

    return false;
}

bool IHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow) {

    if (!validMapped(pWindow))
        return false;

    const auto PWORKSPACE = pWindow->m_workspace;

    if (!pWindow->m_workspace)
        return false;

    if (pWindow->m_pinned || PWORKSPACE->m_forceRendering)
        return true;

    if (PWORKSPACE && PWORKSPACE->isVisible())
        return true;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (PWORKSPACE && PWORKSPACE->m_monitor == m && (PWORKSPACE->m_renderOffset->isBeingAnimated() || PWORKSPACE->m_alpha->isBeingAnimated()))
            return true;

        if (m->m_activeSpecialWorkspace && pWindow->onSpecialWorkspace())
            return true;
    }

    return false;
}

void IHyprRenderer::renderWorkspaceWindowsFullscreen(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    PHLWINDOW pWorkspaceWindow = nullptr;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOWS);

    // loop over the tiled windows that are fading out
    for (auto const& w : g_pCompositor->m_windows) {
        if (!shouldRenderWindow(w, pMonitor))
            continue;

        if (w->m_alpha->value() == 0.f)
            continue;

        if (w->isFullscreen() || w->m_isFloating)
            continue;

        if (pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and floating ones too
    for (auto const& w : g_pCompositor->m_windows) {
        if (!shouldRenderWindow(w, pMonitor))
            continue;

        if (w->m_alpha->value() == 0.f)
            continue;

        if (w->isFullscreen() || !w->m_isFloating)
            continue;

        if (w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    // TODO: this pass sucks
    for (auto const& w : g_pCompositor->m_windows) {
        const auto PWORKSPACE = w->m_workspace;

        if (w->m_workspace != pWorkspace || !w->isFullscreen()) {
            if (!(PWORKSPACE && (PWORKSPACE->m_renderOffset->isBeingAnimated() || PWORKSPACE->m_alpha->isBeingAnimated() || PWORKSPACE->m_forceRendering)))
                continue;

            if (w->m_monitor != pMonitor)
                continue;
        }

        if (!w->isFullscreen())
            continue;

        if (w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (shouldRenderWindow(w, pMonitor))
            renderWindow(w, pMonitor, time, pWorkspace->m_fullscreenMode != FSMODE_FULLSCREEN, RENDER_PASS_ALL);

        if (w->m_workspace != pWorkspace)
            continue;

        pWorkspaceWindow = w;
    }

    if (!pWorkspaceWindow) {
        // ?? happens sometimes...
        pWorkspace->m_hasFullscreenWindow = false;
        return; // this will produce one blank frame. Oh well.
    }

    // then render windows over fullscreen.
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->workspaceID() != pWorkspaceWindow->workspaceID() || !w->m_isFloating || (!w->m_createdOverFullscreen && !w->m_pinned) || (!w->m_isMapped && !w->m_fadingOut) ||
            w->isFullscreen())
            continue;

        if (w->m_monitor == pWorkspace->m_monitor && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }
}

void IHyprRenderer::renderWorkspaceWindows(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    PHLWINDOW lastWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOWS);

    std::vector<PHLWINDOWREF> windows, tiledFadingOut;
    windows.reserve(g_pCompositor->m_windows.size());

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->isHidden() || (!w->m_isMapped && !w->m_fadingOut))
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        windows.emplace_back(w);
    }

    // Non-floating main
    for (auto& w : windows) {
        if (w->m_isFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render active window after all others of this pass
        if (w == Desktop::focusState()->window()) {
            lastWindow = w.lock();
            continue;
        }

        // render tiled fading out after others
        if (w->m_fadingOut) {
            tiledFadingOut.emplace_back(w);
            w.reset();
            continue;
        }

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
        w.reset();
    }

    if (lastWindow)
        renderWindow(lastWindow, pMonitor, time, true, RENDER_PASS_MAIN);

    lastWindow.reset();

    // render tiled windows that are fading out after other tiled to not hide them behind
    for (auto& w : tiledFadingOut) {
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
    }

    // Non-floating popup
    for (auto& w : windows) {
        if (!w)
            continue;

        if (w->m_isFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_POPUP);
        w.reset();
    }

    // floating on top
    for (auto& w : windows) {
        if (!w)
            continue;

        if (!w->m_isFloating || w->m_pinned)
            continue;

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_ALL);
    }
}

void IHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, const Time::steady_tp& time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool standalone) {
    if (pWindow->isHidden() && !standalone)
        return;

    if (pWindow->m_fadingOut) {
        if (pMonitor == pWindow->m_monitor) // TODO: fix this
            renderSnapshot(pWindow);
        return;
    }

    if (!pWindow->m_isMapped)
        return;

    TRACY_GPU_ZONE("RenderWindow");

    const auto                       PWORKSPACE = pWindow->m_workspace;
    const auto                       REALPOS    = pWindow->m_realPosition->value() + (pWindow->m_pinned ? Vector2D{} : PWORKSPACE->m_renderOffset->value());
    static auto                      PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time};
    CBox                             textureBox = {REALPOS.x, REALPOS.y, std::max(pWindow->m_realSize->value().x, 5.0), std::max(pWindow->m_realSize->value().y, 5.0)};

    renderdata.pos.x = textureBox.x;
    renderdata.pos.y = textureBox.y;
    renderdata.w     = textureBox.w;
    renderdata.h     = textureBox.h;

    if (ignorePosition) {
        renderdata.pos.x = pMonitor->m_position.x;
        renderdata.pos.y = pMonitor->m_position.y;
    } else {
        const bool ANR = pWindow->isNotResponding();
        if (ANR && pWindow->m_notRespondingTint->goal() != 0.2F)
            *pWindow->m_notRespondingTint = 0.2F;
        else if (!ANR && pWindow->m_notRespondingTint->goal() != 0.F)
            *pWindow->m_notRespondingTint = 0.F;
    }

    if (standalone)
        decorate = false;

    // whether to use m_fMovingToWorkspaceAlpha, only if fading out into an invisible ws
    const bool USE_WORKSPACE_FADE_ALPHA = pWindow->m_monitorMovedFrom != -1 && (!PWORKSPACE || !PWORKSPACE->isVisible());

    renderdata.surface   = pWindow->wlSurface()->resource();
    renderdata.dontRound = pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderdata.fadeAlpha = pWindow->m_alpha->value() * (pWindow->m_pinned || USE_WORKSPACE_FADE_ALPHA ? 1.f : PWORKSPACE->m_alpha->value()) *
        (USE_WORKSPACE_FADE_ALPHA ? pWindow->m_movingToWorkspaceAlpha->value() : 1.F) * pWindow->m_movingFromWorkspaceAlpha->value();
    renderdata.alpha         = pWindow->m_activeInactiveAlpha->value();
    renderdata.decorate      = decorate && !pWindow->m_X11DoesntWantBorders && !pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderdata.rounding      = standalone || renderdata.dontRound ? 0 : pWindow->rounding() * pMonitor->m_scale;
    renderdata.roundingPower = standalone || renderdata.dontRound ? 2.0f : pWindow->roundingPower();
    renderdata.blur          = !standalone && shouldBlur(pWindow);
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
    g_pHyprOpenGL->m_renderData.currentWindow = pWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOW);

    const auto fullAlpha = renderdata.alpha * renderdata.fadeAlpha;

    if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {
        CBox                        monbox = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};
        CRectPassElement::SRectData data;
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * fullAlpha);
        data.box   = monbox;
        m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    renderdata.pos.x += pWindow->m_floatingOffset.x;
    renderdata.pos.y += pWindow->m_floatingOffset.y;

    // if window is floating and we have a slide animation, clip it to its full bb
    if (!ignorePosition && pWindow->m_isFloating && !pWindow->isFullscreen() && PWORKSPACE->m_renderOffset->isBeingAnimated() && !pWindow->m_pinned) {
        CRegion rg =
            pWindow->getFullWindowBoundingBox().translate(-pMonitor->m_position + PWORKSPACE->m_renderOffset->value() + pWindow->m_floatingOffset).scale(pMonitor->m_scale);
        renderdata.clipBox = rg.getExtents();
    }

    // render window decorations first, if not fullscreen full
    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_MAIN) {

        const bool TRANSFORMERSPRESENT = !pWindow->m_transformers.empty();

        if (TRANSFORMERSPRESENT) {
            g_pHyprOpenGL->bindOffMain();

            for (auto const& t : pWindow->m_transformers) {
                t->preWindowRender(&renderdata);
            }
        }

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->m_windowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_BOTTOM)
                    continue;

                wd->draw(pMonitor, fullAlpha);
            }

            for (auto const& wd : pWindow->m_windowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_UNDER)
                    continue;

                wd->draw(pMonitor, fullAlpha);
            }
        }

        static auto PXWLUSENN = CConfigValue<Hyprlang::INT>("xwayland:use_nearest_neighbor");
        if ((pWindow->m_isX11 && *PXWLUSENN) || pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())
            renderdata.useNearestNeighbor = true;

        if (pWindow->wlSurface()->small() && !pWindow->wlSurface()->m_fillIgnoreSmall && renderdata.blur) {
            CBox wb = {renderdata.pos.x - pMonitor->m_position.x, renderdata.pos.y - pMonitor->m_position.y, renderdata.w, renderdata.h};
            wb.scale(pMonitor->m_scale).round();
            CRectPassElement::SRectData data;
            data.color = CHyprColor(0, 0, 0, 0);
            data.box   = wb;
            data.round = renderdata.dontRound ? 0 : renderdata.rounding - 1;
            data.blur  = true;
            data.blurA = renderdata.fadeAlpha;
            data.xray  = shouldUseNewBlurOptimizations(nullptr, pWindow);
            m_renderPass.add(makeUnique<CRectPassElement>(data));
            renderdata.blur = false;
        }

        renderdata.surfaceCounter = 0;
        pWindow->wlSurface()->resource()->breadthfirst(
            [this, &renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                if (!s->m_current.texture)
                    return;

                if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                    return;

                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pWindow->wlSurface()->resource();
                m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            nullptr);

        renderdata.useNearestNeighbor = false;

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->m_windowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVER)
                    continue;

                wd->draw(pMonitor, fullAlpha);
            }
        }

        if (TRANSFORMERSPRESENT) {
            IFramebuffer* last = g_pHyprOpenGL->m_renderData.currentFB.get();
            for (auto const& t : pWindow->m_transformers) {
                last = t->transform(last);
            }

            g_pHyprOpenGL->bindBackOnMain();
            g_pHyprOpenGL->renderOffToMain(last);
        }
    }

    m_renderData.clipBox = CBox();

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_POPUP) {
        if (!pWindow->m_isX11) {
            CBox geom = pWindow->m_xdgSurface->m_current.geometry;

            renderdata.pos -= geom.pos();
            renderdata.dontRound       = true; // don't round popups
            renderdata.pMonitor        = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            renderdata.popup           = true;

            static CConfigValue PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:popups_ignorealpha");

            renderdata.blur = shouldBlur(pWindow->m_popupHead);

            if (renderdata.blur) {
                renderdata.discardMode |= DISCARD_ALPHA;
                renderdata.discardOpacity = *PBLURIGNOREA;
            }

            if (pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())
                renderdata.useNearestNeighbor = true;

            renderdata.surfaceCounter = 0;

            pWindow->m_popupHead->breadthfirst(
                [this, &renderdata](WP<Desktop::View::CPopup> popup, void* data) {
                    if (popup->m_fadingOut) {
                        renderSnapshot(popup);
                        return;
                    }

                    if (!popup->aliveAndVisible())
                        return;

                    const auto     pos    = popup->coordsRelativeToParent();
                    const Vector2D oldPos = renderdata.pos;
                    renderdata.pos += pos;
                    renderdata.fadeAlpha = popup->m_alpha->value();

                    popup->wlSurface()->resource()->breadthfirst(
                        [this, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                            if (!s->m_current.texture)
                                return;

                            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                                return;

                            renderdata.localPos    = offset;
                            renderdata.texture     = s->m_current.texture;
                            renderdata.surface     = s;
                            renderdata.mainSurface = false;
                            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                            renderdata.surfaceCounter++;
                        },
                        data);

                    renderdata.pos = oldPos;
                },
                &renderdata);

            renderdata.alpha = 1.F;
        }

        if (decorate) {
            for (auto const& wd : pWindow->m_windowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVERLAY)
                    continue;

                wd->draw(pMonitor, fullAlpha);
            }
        }
    }

    EMIT_HOOK_EVENT("render", RENDER_POST_WINDOW);

    g_pHyprOpenGL->m_renderData.currentWindow.reset();
}

void IHyprRenderer::drawSurface(CSurfacePassElement* element, const CRegion& damage) {
    draw(element, damage);
    if (!m_bBlockSurfaceFeedback)
        element->m_data.surface->presentFeedback(element->m_data.when, element->m_data.pMonitor->m_self.lock());

    // add async (dmabuf) buffers to usedBuffers so we can handle release later
    // sync (shm) buffers will be released in commitState, so no need to track them here
    if (element->m_data.surface->m_current.buffer && !element->m_data.surface->m_current.buffer->isSynchronous())
        m_usedAsyncBuffers.emplace_back(element->m_data.surface->m_current.buffer);
}

void IHyprRenderer::draw(WP<IPassElement> element, const CRegion& damage) {
    if (!element)
        return;

    switch (element->kind()) {
        case EK_BORDER: draw(dc<CBorderPassElement*>(element.get()), damage); break;
        case EK_CLEAR: draw(dc<CClearPassElement*>(element.get()), damage); break;
        case EK_FRAMEBUFFER: draw(dc<CFramebufferElement*>(element.get()), damage); break;
        case EK_PRE_BLUR: draw(dc<CPreBlurElement*>(element.get()), damage); break;
        case EK_RECT: draw(dc<CRectPassElement*>(element.get()), damage); break;
        case EK_HINTS: draw(dc<CRendererHintsPassElement*>(element.get()), damage); break;
        case EK_SHADOW: draw(dc<CShadowPassElement*>(element.get()), damage); break;
        case EK_SURFACE: drawSurface(dc<CSurfacePassElement*>(element.get()), damage); break;
        case EK_TEXTURE: draw(dc<CTexPassElement*>(element.get()), damage); break;
        case EK_TEXTURE_MATTE: draw(dc<CTextureMatteElement*>(element.get()), damage); break;
        default: Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
    }
}

bool IHyprRenderer::preBlurQueued(PHLMONITORREF pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    if (!pMonitor)
        return false;
    return m_renderData.pMonitor->m_blurFBDirty && *PBLURNEWOPTIMIZE && *PBLUR && m_renderData.pMonitor->m_blurFBShouldRender;
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

    auto image = createImage(buffer);

    if (!image) {
        Log::logger->log(Log::ERR, "Cannot create a texture: failed to create an EGLImage");
        return createTexture(buffer->opaque);
    }

    return createTexture(attrs, image, buffer->opaque);
}

void IHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, const Time::steady_tp& time, bool popups, bool lockscreen) {
    if (!pLayer)
        return;

    // skip rendering based on abovelock rule and make sure to not render abovelock layers twice
    if ((pLayer->m_ruleApplicator->aboveLock().valueOrDefault() && !lockscreen && g_pSessionLockManager->isSessionLocked()) ||
        (lockscreen && !pLayer->m_ruleApplicator->aboveLock().valueOrDefault()))
        return;

    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    if (*PDIMAROUND && pLayer->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && !popups) {
        CRectPassElement::SRectData data;
        data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pLayer->m_alpha->value());
        m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    if (pLayer->m_fadingOut) {
        if (!popups)
            renderSnapshot(pLayer);
        return;
    }

    TRACY_GPU_ZONE("RenderLayer");

    const auto                       REALPOS = pLayer->m_realPosition->value();
    const auto                       REALSIZ = pLayer->m_realSize->value();

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, REALPOS};
    renderdata.fadeAlpha                        = pLayer->m_alpha->value();
    renderdata.blur                             = shouldBlur(pLayer);
    renderdata.surface                          = pLayer->wlSurface()->resource();
    renderdata.decorate                         = false;
    renderdata.w                                = REALSIZ.x;
    renderdata.h                                = REALSIZ.y;
    renderdata.pLS                              = pLayer;
    renderdata.blockBlurOptimization            = pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    renderdata.clipBox = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y}.scale(pMonitor->m_scale);

    if (renderdata.blur && pLayer->m_ruleApplicator->ignoreAlpha().hasValue()) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = pLayer->m_ruleApplicator->ignoreAlpha().valueOrDefault();
    }

    if (!popups)
        pLayer->wlSurface()->resource()->breadthfirst(
            [this, &renderdata, &pLayer](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                if (!s->m_current.texture)
                    return;

                if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                    return;

                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pLayer->wlSurface()->resource();
                m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    renderdata.popup           = true;
    renderdata.blur            = pLayer->m_ruleApplicator->blurPopups().valueOrDefault();
    renderdata.surfaceCounter  = 0;
    if (popups) {
        pLayer->m_popupHead->breadthfirst(
            [this, &renderdata](WP<Desktop::View::CPopup> popup, void* data) {
                if (!popup->aliveAndVisible())
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
                m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);
    }
}

void IHyprRenderer::renderIMEPopup(CInputPopup* pPopup, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    const auto                       POS = pPopup->globalBox().pos();

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, POS};

    const auto                       SURF = pPopup->getSurface();

    renderdata.surface  = SURF;
    renderdata.decorate = false;
    renderdata.w        = SURF->m_current.size.x;
    renderdata.h        = SURF->m_current.size.y;

    static auto PBLUR        = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PBLURIMES    = CConfigValue<Hyprlang::INT>("decoration:blur:input_methods");
    static auto PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:input_methods_ignorealpha");

    renderdata.blur = *PBLURIMES && *PBLUR;
    if (renderdata.blur) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = *PBLURIGNOREA;
    }

    SURF->breadthfirst(
        [this, &renderdata, &SURF](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == SURF;
            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void IHyprRenderer::renderSessionLockSurface(WP<SSessionLockSurface> pSurface, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->m_position, pMonitor->m_position};

    renderdata.blur     = false;
    renderdata.surface  = pSurface->surface->surface();
    renderdata.decorate = false;
    renderdata.w        = pMonitor->m_size.x;
    renderdata.h        = pMonitor->m_size.y;

    renderdata.surface->breadthfirst(
        [this, &renderdata, &pSurface](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pSurface->surface->surface();
            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void IHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate, const float& scale) {
    static auto PDIMSPECIAL      = CConfigValue<Hyprlang::FLOAT>("decoration:dim_special");
    static auto PBLURSPECIAL     = CConfigValue<Hyprlang::INT>("decoration:blur:special");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PXPMODE          = CConfigValue<Hyprlang::INT>("render:xp_mode");
    static auto PSESSIONLOCKXRAY = CConfigValue<Hyprlang::INT>("misc:session_lock_xray");

    if UNLIKELY (!pMonitor)
        return;

    if UNLIKELY (g_pSessionLockManager->isSessionLocked() && !*PSESSIONLOCKXRAY) {
        // We stop to render workspaces as soon as the lockscreen was sent the "locked" or "finished" (aka denied) event.
        // In addition we make sure to stop rendering workspaces after misc:lockdead_screen_delay has passed.
        if (g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied())
            return;
    }

    // todo: matrices are buggy atm for some reason, but probably would be preferable in the long run
    // g_pHyprOpenGL->saveMatrix();
    // g_pHyprOpenGL->setMatrixScaleTranslate(translate, scale);

    SRenderModifData RENDERMODIFDATA;
    if (translate != Vector2D{0, 0})
        RENDERMODIFDATA.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate));
    if UNLIKELY (scale != 1.f)
        RENDERMODIFDATA.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale));

    if UNLIKELY (!RENDERMODIFDATA.modifs.empty())
        m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RENDERMODIFDATA}));

    CScopeGuard x([&RENDERMODIFDATA] {
        if (!RENDERMODIFDATA.modifs.empty()) {
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        }
    });

    if UNLIKELY (!pWorkspace) {
        // allow rendering without a workspace. In this case, just render layers.

        renderBackground(pMonitor);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        EMIT_HOOK_EVENT("render", RENDER_POST_WALLPAPER);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        return;
    }

    if LIKELY (!*PXPMODE) {
        renderBackground(pMonitor);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        EMIT_HOOK_EVENT("render", RENDER_POST_WALLPAPER);

        for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
    }

    // pre window pass
    if (preBlurQueued(pMonitor))
        m_renderPass.add(makeUnique<CPreBlurElement>());

    if UNLIKELY /* subjective? */ (pWorkspace->m_hasFullscreenWindow)
        renderWorkspaceWindowsFullscreen(pMonitor, pWorkspace, time);
    else
        renderWorkspaceWindows(pMonitor, pWorkspace, time);

    // and then special
    if UNLIKELY (pMonitor->m_specialFade->value() != 0.F) {
        const auto SPECIALANIMPROGRS = pMonitor->m_specialFade->getCurveValue();
        const bool ANIMOUT           = !pMonitor->m_activeSpecialWorkspace;

        if (*PDIMSPECIAL != 0.f) {
            CRectPassElement::SRectData data;
            data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
            data.color = CHyprColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS));

            m_renderPass.add(makeUnique<CRectPassElement>(data));
        }

        if (*PBLURSPECIAL && *PBLUR) {
            CRectPassElement::SRectData data;
            data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
            data.color = CHyprColor(0, 0, 0, 0);
            data.blur  = true;
            data.blurA = (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS);

            m_renderPass.add(makeUnique<CRectPassElement>(data));
        }
    }

    // special
    for (auto const& ws : g_pCompositor->getWorkspaces()) {
        if (ws->m_alpha->value() <= 0.F || !ws->m_isSpecialWorkspace)
            continue;

        if (ws->m_hasFullscreenWindow)
            renderWorkspaceWindowsFullscreen(pMonitor, ws.lock(), time);
        else
            renderWorkspaceWindows(pMonitor, ws.lock(), time);
    }

    // pinned always above
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->isHidden() && !w->m_isMapped && !w->m_fadingOut)
            continue;

        if (!w->m_pinned || !w->m_isFloating)
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        // render the bad boy
        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    EMIT_HOOK_EVENT("render", RENDER_POST_WINDOWS);

    // Render surfaces above windows for monitor
    for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        renderLayer(ls.lock(), pMonitor, time);
    }

    // Render IME popups
    for (auto const& imep : g_pInputManager->m_relay.m_inputMethodPopups) {
        renderIMEPopup(imep.get(), pMonitor, time);
    }

    for (auto const& ls : pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls.lock(), pMonitor, time);
    }

    for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
        for (auto const& ls : lsl) {
            renderLayer(ls.lock(), pMonitor, time, true);
        }
    }

    renderDragIcon(pMonitor, time);

    //g_pHyprOpenGL->restoreMatrix();
}

SP<ITexture> IHyprRenderer::getBackground(PHLMONITOR pMonitor) {

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
    if (!backgroundTexture->ok())
        return nullptr;
    Log::logger->log(Log::DEBUG, "Background created for monitor {}", pMonitor->m_name);

    // clear the resource after we're done using it
    g_pEventLoopManager->doLater([this] { m_backgroundResource.reset(); });

    // set the animation to start for fading this background in nicely
    pMonitor->m_backgroundOpacity->setValueAndWarp(0.F);
    *pMonitor->m_backgroundOpacity = 1.F;

    return backgroundTexture;
}

void IHyprRenderer::renderBackground(PHLMONITOR pMonitor) {
    static auto PRENDERTEX       = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");
    static auto PNOSPLASH        = CConfigValue<Hyprlang::INT>("misc:disable_splash_rendering");

    if (*PRENDERTEX /* inverted cfg flag */ || pMonitor->m_backgroundOpacity->isBeingAnimated())
        m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));

    if (!*PRENDERTEX) {
        static auto PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");

        if (!pMonitor->m_background)
            pMonitor->m_background = getBackground(pMonitor);

        if (!pMonitor->m_background)
            m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));
        else {
            CTexPassElement::SRenderData data;
            const double                 MONRATIO = m_renderData.pMonitor->m_transformedSize.x / m_renderData.pMonitor->m_transformedSize.y;
            const double                 WPRATIO  = pMonitor->m_background->m_size.x / pMonitor->m_background->m_size.y;
            Vector2D                     origin;
            double                       scale = 1.0;

            if (MONRATIO > WPRATIO) {
                scale    = m_renderData.pMonitor->m_transformedSize.x / pMonitor->m_background->m_size.x;
                origin.y = (m_renderData.pMonitor->m_transformedSize.y - pMonitor->m_background->m_size.y * scale) / 2.0;
            } else {
                scale    = m_renderData.pMonitor->m_transformedSize.y / pMonitor->m_background->m_size.y;
                origin.x = (m_renderData.pMonitor->m_transformedSize.x - pMonitor->m_background->m_size.x * scale) / 2.0;
            }

            if (MONRATIO != WPRATIO)
                m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));

            // data.box          = {origin, pMonitor->m_background->m_size * scale};
            data.box = {origin, pMonitor->m_background->m_size * scale};
            // data.clipBox      = {{}, m_renderData.pMonitor->m_transformedSize};
            data.a            = m_renderData.pMonitor->m_backgroundOpacity->value();
            data.flipEndFrame = true;
            data.tex          = pMonitor->m_background;
            m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
        }
    }

    if (!*PNOSPLASH) {
        if (!pMonitor->m_splash)
            pMonitor->m_splash = renderSplash([this, pMonitor](auto width, auto height, const auto DATA) { return createTexture(width, height, DATA); },
                                              pMonitor->m_pixelSize.y / 76, pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y);

        if (pMonitor->m_splash) {
            CTexPassElement::SRenderData data;
            data.box = {{(pMonitor->m_pixelSize.x - pMonitor->m_splash->m_size.x) / 2.0, pMonitor->m_pixelSize.y * 0.98 - pMonitor->m_splash->m_size.y},
                        pMonitor->m_splash->m_size};
            data.tex = pMonitor->m_splash;
            m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
        }
    }
}

void IHyprRenderer::requestBackgroundResource() {
    if (m_backgroundResource)
        return;

    static auto PNOWALLPAPER    = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PFORCEWALLPAPER = CConfigValue<Hyprlang::INT>("misc:force_default_wallpaper");

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
        for (const auto& m : g_pCompositor->m_monitors) {
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
        std::string     p = std::string{e} + "/hypr/" + filename;
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

SP<ITexture> IHyprRenderer::getBlurTexture(PHLMONITORREF pMonitor) {
    return nullptr;
}

bool IHyprRenderer::shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");

    if (!getBlurTexture(m_renderData.pMonitor))
        return false;

    if (pWindow && pWindow->m_ruleApplicator->xray().hasValue() && !pWindow->m_ruleApplicator->xray().valueOrDefault())
        return false;

    if (pLayer && pLayer->m_ruleApplicator->xray().valueOrDefault() == 0)
        return false;

    if ((*PBLURNEWOPTIMIZE && pWindow && !pWindow->m_isFloating && !pWindow->onSpecialWorkspace()) || *PBLURXRAY)
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

    auto                  CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1920, 1080 /* arbitrary, just for size */);
    auto                  CAIRO        = cairo_create(CAIROSURFACE);

    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, sc<PangoWeight>(weight));
    pango_layout_set_font_description(layoutText, pangoFD);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int textW = 0, textH = 0;
    pango_layout_set_text(layoutText, text.c_str(), -1);

    if (maxWidth > 0) {
        pango_layout_set_width(layoutText, maxWidth * PANGO_SCALE);
        pango_layout_set_ellipsize(layoutText, PANGO_ELLIPSIZE_END);
    }

    pango_layout_get_size(layoutText, &textW, &textH);
    textW /= PANGO_SCALE;
    textH /= PANGO_SCALE;

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textW, textH);
    CAIRO        = cairo_create(CAIROSURFACE);

    layoutText = pango_cairo_create_layout(CAIRO);
    pangoFD    = pango_font_description_new();

    pango_font_description_set_family_static(pangoFD, FONTFAMILY.c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, sc<PangoWeight>(weight));
    pango_layout_set_font_description(layoutText, pangoFD);
    pango_layout_set_text(layoutText, text.c_str(), -1);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    cairo_move_to(CAIRO, 0, 0);
    pango_cairo_show_layout(CAIRO, layoutText);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);

    auto tex = createTexture(cairo_image_surface_get_width(CAIROSURFACE), cairo_image_surface_get_height(CAIROSURFACE), cairo_image_surface_get_data(CAIROSURFACE));

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    return tex;
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

        const auto VT = g_pCompositor->getVTNr();

        m_lockTtyTextTexture = renderText(std::format("Running on tty {}", VT.has_value() ? std::to_string(*VT) : "unknown"), CHyprColor{0.9F, 0.9F, 0.9F, 0.7F}, 20, true);
    } else {
        m_lockDeadTexture.reset();
        m_lockDead2Texture.reset();
        m_lockTtyTextTexture.reset();
    }
}

void IHyprRenderer::renderLockscreen(PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry) {
    TRACY_GPU_ZONE("RenderLockscreen");

    const bool LOCKED = g_pSessionLockManager->isSessionLocked();
    if (!LOCKED) {
        ensureLockTexturesRendered(false);
        return;
    }

    const bool RENDERPRIMER = g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied();
    if (RENDERPRIMER)
        renderSessionLockPrimer(pMonitor);

    const auto PSLS              = g_pSessionLockManager->getSessionLockSurfaceForMonitor(pMonitor->m_id);
    const bool RENDERLOCKMISSING = (PSLS.expired() || g_pSessionLockManager->clientDenied()) && g_pSessionLockManager->shallConsiderLockMissing();

    ensureLockTexturesRendered(RENDERLOCKMISSING);

    if (RENDERLOCKMISSING)
        renderSessionLockMissing(pMonitor);
    else if (PSLS) {
        renderSessionLockSurface(PSLS, pMonitor, now);
        g_pSessionLockManager->onLockscreenRenderedOnMonitor(pMonitor->m_id);

        // render layers and then their popups for abovelock rule
        for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                renderLayer(ls.lock(), pMonitor, now, false, true);
            }
        }
        for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                renderLayer(ls.lock(), pMonitor, now, true, true);
            }
        }
    }
}

void IHyprRenderer::renderSessionLockPrimer(PHLMONITOR pMonitor) {
    static auto PSESSIONLOCKXRAY = CConfigValue<Hyprlang::INT>("misc:session_lock_xray");
    if (*PSESSIONLOCKXRAY)
        return;

    CRectPassElement::SRectData data;
    data.color = CHyprColor(0, 0, 0, 1.f);
    data.box   = CBox{{}, pMonitor->m_pixelSize};

    m_renderPass.add(makeUnique<CRectPassElement>(data));
}

void IHyprRenderer::renderSessionLockMissing(PHLMONITOR pMonitor) {
    const bool ANY_PRESENT = g_pSessionLockManager->anySessionLockSurfacesPresent();

    // ANY_PRESENT: render image2, without instructions. Lock still "alive", unless texture dead
    // else: render image, with instructions. Lock is gone.
    CBox                         monbox = {{}, pMonitor->m_pixelSize};
    CTexPassElement::SRenderData data;
    data.tex = (ANY_PRESENT) ? m_lockDead2Texture : m_lockDeadTexture;
    data.box = monbox;
    data.a   = 1;

    m_renderPass.add(makeUnique<CTexPassElement>(data));

    if (!ANY_PRESENT && m_lockTtyTextTexture) {
        // also render text for the tty number
        CBox texbox = {{}, m_lockTtyTextTexture->m_size};
        data.tex    = m_lockTtyTextTexture;
        data.box    = texbox;

        m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
    }
}

static std::optional<Vector2D> getSurfaceExpectedSize(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main) {
    const auto CAN_USE_WINDOW       = pWindow && main;
    const auto WINDOW_SIZE_MISALIGN = CAN_USE_WINDOW && pWindow->getReportedSize() != pWindow->wlSurface()->resource()->m_current.size;

    if (pSurface->m_current.viewport.hasDestination)
        return (pSurface->m_current.viewport.destination * pMonitor->m_scale).round();

    if (pSurface->m_current.viewport.hasSource)
        return (pSurface->m_current.viewport.source.size() * pMonitor->m_scale).round();

    if (WINDOW_SIZE_MISALIGN)
        return (pSurface->m_current.size * pMonitor->m_scale).round();

    if (CAN_USE_WINDOW)
        return (pWindow->getReportedSize() * pMonitor->m_scale).round();

    return std::nullopt;
}

void IHyprRenderer::calculateUVForSurface(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main, const Vector2D& projSize,
                                          const Vector2D& projSizeUnscaled, bool fixMisalignedFSV1) {
    if (!pWindow || !pWindow->m_isX11) {
        static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");

        Vector2D    uvTL;
        Vector2D    uvBR = Vector2D(1, 1);

        if (pSurface->m_current.viewport.hasSource) {
            // we stretch it to dest. if no dest, to 1,1
            Vector2D const& bufferSize   = pSurface->m_current.bufferSize;
            auto const&     bufferSource = pSurface->m_current.viewport.source;

            // calculate UV for the basic src_box. Assume dest == size. Scale to dest later
            uvTL = Vector2D(bufferSource.x / bufferSize.x, bufferSource.y / bufferSize.y);
            uvBR = Vector2D((bufferSource.x + bufferSource.width) / bufferSize.x, (bufferSource.y + bufferSource.height) / bufferSize.y);

            if (uvBR.x < 0.01f || uvBR.y < 0.01f) {
                uvTL = Vector2D();
                uvBR = Vector2D(1, 1);
            }
        }

        if (projSize != Vector2D{} && fixMisalignedFSV1) {
            // instead of nearest_neighbor (we will repeat / skip)
            // just cut off / expand surface
            const Vector2D PIXELASUV   = Vector2D{1, 1} / pSurface->m_current.bufferSize;
            const auto&    BUFFER_SIZE = pSurface->m_current.bufferSize;

            // compute MISALIGN from the adjusted UV coordinates.
            const Vector2D MISALIGNMENT = (uvBR - uvTL) * BUFFER_SIZE - projSize;

            if (MISALIGNMENT != Vector2D{})
                uvBR -= MISALIGNMENT * PIXELASUV;
        } else {
            // if the surface is smaller than our viewport, extend its edges.
            // this will break if later on xdg geometry is hit, but we really try
            // to let the apps know to NOT add CSD. Also if source is there.
            // there is no way to fix this if that's the case
            const auto MONITOR_WL_SCALE = std::ceil(pMonitor->m_scale);
            const bool SCALE_UNAWARE    = pMonitor->m_scale != 1.f && (MONITOR_WL_SCALE == pSurface->m_current.scale || !pSurface->m_current.viewport.hasDestination);
            const auto EXPECTED_SIZE    = getSurfaceExpectedSize(pWindow, pSurface, pMonitor, main).value_or((projSize * pMonitor->m_scale).round());

            const auto RATIO = projSize / EXPECTED_SIZE;
            if (!SCALE_UNAWARE || MONITOR_WL_SCALE == 1) {
                if (*PEXPANDEDGES && !SCALE_UNAWARE && (RATIO.x > 1 || RATIO.y > 1)) {
                    const auto FIX = RATIO.clamp(Vector2D{1, 1}, Vector2D{1000000, 1000000});
                    uvBR           = uvBR * FIX;
                }

                // FIXME: probably do this for in anims on all views...
                const auto SHOULD_SKIP = !pWindow || pWindow->m_animatingIn;
                if (!SHOULD_SKIP && (RATIO.x < 1 || RATIO.y < 1)) {
                    const auto FIX = RATIO.clamp(Vector2D{0.0001, 0.0001}, Vector2D{1, 1});
                    uvBR           = uvBR * FIX;
                }
            }
        }

        m_renderData.primarySurfaceUVTopLeft     = uvTL;
        m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (m_renderData.primarySurfaceUVTopLeft == Vector2D() && m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        if (!main || !pWindow)
            return;

        // FIXME: this doesn't work. We always set MAXIMIZED anyways, so this doesn't need to work, but it's problematic.

        // CBox geom = pWindow->m_xdgSurface->m_current.geometry;

        // // Adjust UV based on the xdg_surface geometry
        // if (geom.x != 0 || geom.y != 0 || geom.w != 0 || geom.h != 0) {
        //     const auto XPERC = geom.x / pSurface->m_current.size.x;
        //     const auto YPERC = geom.y / pSurface->m_current.size.y;
        //     const auto WPERC = (geom.x + geom.w ? geom.w : pSurface->m_current.size.x) / pSurface->m_current.size.x;
        //     const auto HPERC = (geom.y + geom.h ? geom.h : pSurface->m_current.size.y) / pSurface->m_current.size.y;

        //     const auto TOADDTL = Vector2D(XPERC * (uvBR.x - uvTL.x), YPERC * (uvBR.y - uvTL.y));
        //     uvBR               = uvBR - Vector2D((1.0 - WPERC) * (uvBR.x - uvTL.x), (1.0 - HPERC) * (uvBR.y - uvTL.y));
        //     uvTL               = uvTL + TOADDTL;
        // }

        m_renderData.primarySurfaceUVTopLeft     = uvTL;
        m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (m_renderData.primarySurfaceUVTopLeft == Vector2D() && m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

bool IHyprRenderer::beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, SP<IFramebuffer> fb, bool simple) {
    m_renderPass.clear();
    m_renderMode          = mode;
    m_renderData.pMonitor = pMonitor;

    if (m_renderMode == RENDER_MODE_FULL_FAKE)
        return beginFullFakeRenderInternal(pMonitor, damage, fb, simple);

    int bufferAge = 0;

    if (!buffer) {
        m_currentBuffer = pMonitor->m_output->swapchain->next(&bufferAge);
        if (!m_currentBuffer) {
            Log::logger->log(Log::ERR, "Failed to acquire swapchain buffer for {}", pMonitor->m_name);
            return false;
        }
    } else
        m_currentBuffer = buffer;

    initRender();

    if (!initRenderBuffer(m_currentBuffer, pMonitor->m_output->state->state().drmFormat)) {
        Log::logger->log(Log::ERR, "failed to start a render pass for output {}, no RBO could be obtained", pMonitor->m_name);
        return false;
    }

    if (m_renderMode == RENDER_MODE_NORMAL) {
        damage = pMonitor->m_damage.getBufferDamage(bufferAge);
        pMonitor->m_damage.rotate();
    }

    const auto  res     = beginRenderInternal(pMonitor, damage, simple);
    static bool initial = true;
    if (initial) {
        initAssets();
        initial = false;
    }

    return res;
}

void IHyprRenderer::setDamage(const CRegion& damage_, std::optional<CRegion> finalDamage) {
    m_renderData.damage.set(damage_);
    m_renderData.finalDamage.set(finalDamage.value_or(damage_));
}

void IHyprRenderer::renderMirrored() {
    auto         monitor  = m_renderData.pMonitor;
    auto         mirrored = monitor->m_mirrorOf;

    const double scale  = std::min(monitor->m_transformedSize.x / mirrored->m_transformedSize.x, monitor->m_transformedSize.y / mirrored->m_transformedSize.y);
    CBox         monbox = {0, 0, mirrored->m_transformedSize.x * scale, mirrored->m_transformedSize.y * scale};

    // transform box as it will be drawn on a transformed projection
    monbox.transform(Math::wlTransformToHyprutils(mirrored->m_transform), mirrored->m_transformedSize.x * scale, mirrored->m_transformedSize.y * scale);

    monbox.x = (monitor->m_transformedSize.x - monbox.w) / 2;
    monbox.y = (monitor->m_transformedSize.y - monbox.h) / 2;

    if (!monitor->m_monitorMirrorFB)
        monitor->m_monitorMirrorFB = createFB();

    const auto PFB = mirrored->m_monitorMirrorFB;
    if (!PFB || !PFB->isAllocated() || !PFB->getTexture())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)}));

    CTexPassElement::SRenderData data;
    data.tex               = PFB->getTexture();
    data.box               = monbox;
    data.replaceProjection = Mat3x3::identity()
                                 .translate(monitor->m_pixelSize / 2.0)
                                 .transform(Math::wlTransformToHyprutils(monitor->m_transform))
                                 .transform(Math::wlTransformToHyprutils(Math::invertTransform(mirrored->m_transform)))
                                 .translate(-monitor->m_transformedSize / 2.0);

    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void IHyprRenderer::renderMonitor(PHLMONITOR pMonitor, bool commit) {
    static std::chrono::high_resolution_clock::time_point renderStart        = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point renderStartOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay   = std::chrono::high_resolution_clock::now();

    static auto                                           PDEBUGOVERLAY       = CConfigValue<Hyprlang::INT>("debug:overlay");
    static auto                                           PDAMAGETRACKINGMODE = CConfigValue<Hyprlang::INT>("debug:damage_tracking");
    static auto                                           PDAMAGEBLINK        = CConfigValue<Hyprlang::INT>("debug:damage_blink");
    static auto                                           PSOLDAMAGE          = CConfigValue<Hyprlang::INT>("debug:render_solitary_wo_damage");
    static auto                                           PVFR                = CConfigValue<Hyprlang::INT>("misc:vfr");

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
        g_pDebugOverlay->frameData(pMonitor);
    }

    if (!g_pCompositor->m_sessionActive)
        return;

    if (g_pAnimationManager)
        g_pAnimationManager->frameTick();

    if (pMonitor->m_id == m_mostHzMonitor->m_id ||
        *PVFR == 1) { // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_wantsMonitorReload)
            g_pConfigManager->performMonitorReload();
    }

    if (pMonitor->m_scheduledRecalc) {
        pMonitor->m_scheduledRecalc = false;
        pMonitor->m_activeWorkspace->m_space->recalculate();
    }

    if (!pMonitor->m_output->needsFrame && pMonitor->m_forceFullFrames == 0)
        return;

    // tearing and DS first
    bool shouldTear = pMonitor->updateTearing();

    if (pMonitor->attemptDirectScanout()) {
        pMonitor->m_directScanoutIsActive = true;
        return;
    } else if (!pMonitor->m_lastScanout.expired() || pMonitor->m_directScanoutIsActive) {
        Log::logger->log(Log::DEBUG, "Left a direct scanout.");
        pMonitor->m_lastScanout.reset();
        pMonitor->m_directScanoutIsActive = false;

        // reset DRM format, but only if needed since it might modeset
        if (pMonitor->m_output->state->state().drmFormat != pMonitor->m_prevDrmFormat)
            pMonitor->m_output->state->setFormat(pMonitor->m_prevDrmFormat);

        pMonitor->m_drmFormat = pMonitor->m_prevDrmFormat;
    }

    EMIT_HOOK_EVENT("preRender", pMonitor);

    const auto NOW = Time::steadyNow();

    // check the damage
    bool hasChanged = pMonitor->m_output->needsFrame || pMonitor->m_damage.hasChanged();

    if (!hasChanged && *PDAMAGETRACKINGMODE != DAMAGE_TRACKING_NONE && pMonitor->m_forceFullFrames == 0 && damageBlinkCleanup == 0)
        return;

    if (*PDAMAGETRACKINGMODE == -1) {
        Log::logger->log(Log::CRIT, "Damage tracking mode -1 ????");
        return;
    }

    EMIT_HOOK_EVENT("render", RENDER_PRE);

    pMonitor->m_renderingActive = true;

    // we need to cleanup fading out when rendering the appropriate context
    g_pCompositor->cleanupFadingOut(pMonitor->m_id);

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    TRACY_GPU_ZONE("Render");

    static bool zoomLock = false;
    if (zoomLock && ZOOMFACTOR == 1.f) {
        g_pPointerManager->unlockSoftwareAll();
        zoomLock = false;
    } else if (!zoomLock && ZOOMFACTOR != 1.f) {
        g_pPointerManager->lockSoftwareAll();
        zoomLock = true;
    }

    if (pMonitor == g_pCompositor->getMonitorFromCursor())
        m_renderData.mouseZoomFactor = std::clamp(ZOOMFACTOR, 1.f, INFINITY);
    else
        m_renderData.mouseZoomFactor = 1.f;

    if (pMonitor->m_zoomAnimProgress->value() != 1) {
        m_renderData.mouseZoomFactor    = 2.0 - pMonitor->m_zoomAnimProgress->value(); // 2x zoom -> 1x zoom
        m_renderData.mouseZoomUseMouse  = false;
        m_renderData.useNearestNeighbor = false;
    }

    CRegion damage, finalDamage;
    if (!beginRender(pMonitor, damage, RENDER_MODE_NORMAL)) {
        Log::logger->log(Log::ERR, "renderer: couldn't beginRender()!");
        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->m_forceFullFrames > 0 || damageBlinkCleanup > 0)
        damage = {0, 0, sc<int>(pMonitor->m_transformedSize.x) * 10, sc<int>(pMonitor->m_transformedSize.y) * 10};

    finalDamage = damage;

    // update damage in renderdata as we modified it
    setDamage(damage, finalDamage);

    if (pMonitor->m_forceFullFrames > 0) {
        pMonitor->m_forceFullFrames -= 1;
        if (pMonitor->m_forceFullFrames > 10)
            pMonitor->m_forceFullFrames = 0;
    }

    EMIT_HOOK_EVENT("render", RENDER_BEGIN);

    bool renderCursor = true;

    if (pMonitor->m_solitaryClient && (!finalDamage.empty() || *PSOLDAMAGE))
        renderWindow(pMonitor->m_solitaryClient.lock(), pMonitor, NOW, false, RENDER_PASS_MAIN /* solitary = no popups */);
    else if (!finalDamage.empty()) {
        if (pMonitor->isMirror()) {
            g_pHyprOpenGL->blend(false);
            renderMirrored();
            g_pHyprOpenGL->blend(true);
            EMIT_HOOK_EVENT("render", RENDER_POST_MIRROR);
            renderCursor = false;
        } else {
            CBox renderBox = {0, 0, sc<int>(pMonitor->m_pixelSize.x), sc<int>(pMonitor->m_pixelSize.y)};
            renderWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW, renderBox);

            renderLockscreen(pMonitor, NOW, renderBox);

            if (pMonitor == Desktop::focusState()->monitor()) {
                g_pHyprNotificationOverlay->draw(pMonitor);
                g_pHyprError->draw();
            }

            // for drawing the debug overlay
            if (pMonitor == g_pCompositor->m_monitors.front() && *PDEBUGOVERLAY == 1) {
                renderStartOverlay = std::chrono::high_resolution_clock::now();
                g_pDebugOverlay->draw();
                endRenderOverlay = std::chrono::high_resolution_clock::now();
            }

            if (*PDAMAGEBLINK && damageBlinkCleanup == 0) {
                CRectPassElement::SRectData data;
                data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
                data.color = CHyprColor(1.0, 0.0, 1.0, 100.0 / 255.0);
                m_renderPass.add(makeUnique<CRectPassElement>(data));
                damageBlinkCleanup = 1;
            } else if (*PDAMAGEBLINK) {
                damageBlinkCleanup++;
                if (damageBlinkCleanup > 3)
                    damageBlinkCleanup = 0;
            }
        }
    } else if (!pMonitor->isMirror()) {
        sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW);
        if (pMonitor->m_activeSpecialWorkspace)
            sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeSpecialWorkspace, NOW);
    }

    renderCursor = renderCursor && shouldRenderCursor();

    if (renderCursor) {
        TRACY_GPU_ZONE("RenderCursor");
        g_pPointerManager->renderSoftwareCursorsFor(pMonitor->m_self.lock(), NOW, m_renderData.damage);
    }

    if (pMonitor->m_dpmsBlackOpacity->value() != 0.F) {
        // render the DPMS black if we are animating
        CRectPassElement::SRectData data;
        data.box   = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
        data.color = Colors::BLACK.modifyA(pMonitor->m_dpmsBlackOpacity->value());
        m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    EMIT_HOOK_EVENT("render", RENDER_LAST_MOMENT);

    endRender();

    TRACY_GPU_COLLECT;

    CRegion    frameDamage{m_renderData.damage};

    const auto TRANSFORM = Math::invertTransform(pMonitor->m_transform);
    frameDamage.transform(Math::wlTransformToHyprutils(TRANSFORM), pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y);

    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
        frameDamage.add(0, 0, sc<int>(pMonitor->m_transformedSize.x), sc<int>(pMonitor->m_transformedSize.y));

    if (*PDAMAGEBLINK)
        frameDamage.add(damage);

    if (!pMonitor->m_mirrors.empty())
        damageMirrorsWith(pMonitor, frameDamage);

    pMonitor->m_renderingActive = false;

    EMIT_HOOK_EVENT("render", RENDER_POST);

    pMonitor->m_output->state->addDamage(frameDamage);
    pMonitor->m_output->state->setPresentationMode(shouldTear ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE :
                                                                Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC);

    if (commit)
        commitPendingAndDoExplicitSync(pMonitor);

    if (shouldTear)
        pMonitor->m_tearingState.busy = true;

    if (*PDAMAGEBLINK || *PVFR == 0 || pMonitor->m_pendingFrame)
        g_pCompositor->scheduleFrameForMonitor(pMonitor, Aquamarine::IOutput::AQ_SCHEDULE_RENDER_MONITOR);

    pMonitor->m_pendingFrame = false;

    if (*PDEBUGOVERLAY == 1) {
        const float durationUs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000.f;
        g_pDebugOverlay->renderData(pMonitor, durationUs);

        if (pMonitor == g_pCompositor->m_monitors.front()) {
            const float noOverlayUs = durationUs - std::chrono::duration_cast<std::chrono::nanoseconds>(endRenderOverlay - renderStartOverlay).count() / 1000.f;
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, noOverlayUs);
        } else
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, durationUs);
    }
}

static const hdr_output_metadata NO_HDR_METADATA = {.hdmi_metadata_type1 = hdr_metadata_infoframe{.eotf = 0}};

static hdr_output_metadata       createHDRMetadata(SImageDescription settings, SP<CMonitor> monitor) {
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

bool IHyprRenderer::commitPendingAndDoExplicitSync(PHLMONITOR pMonitor) {
    static auto PCT        = CConfigValue<Hyprlang::INT>("render:send_content_type");
    static auto PPASS      = CConfigValue<Hyprlang::INT>("render:cm_fs_passthrough");
    static auto PAUTOHDR   = CConfigValue<Hyprlang::INT>("render:cm_auto_hdr");
    static auto PNONSHADER = CConfigValue<Hyprlang::INT>("render:non_shader_cm");

    const bool  configuredHDR = (pMonitor->m_cmType == NCMType::CM_HDR_EDID || pMonitor->m_cmType == NCMType::CM_HDR);
    bool        wantHDR       = configuredHDR;

    const auto  FS_WINDOW = pMonitor->getFullscreenWindow();

    if (pMonitor->supportsHDR()) {
        // HDR metadata determined by
        // HDR scRGB - monitor settings
        // HDR PQ surface & DS is active - surface settings
        // PPASS = 0 monitor settings
        // PPASS = 1
        //           windowed: monitor settings
        //           fullscreen surface: surface settings FIXME: fullscreen SDR surface passthrough - pass degamma, gamma if needed
        // PPASS = 2
        //           windowed: monitor settings
        //           fullscreen SDR surface: monitor settings
        //           fullscreen HDR surface: surface settings

        bool hdrIsHandled = false;
        if (FS_WINDOW) {
            const auto ROOT_SURF = FS_WINDOW->wlSurface()->resource();
            const auto SURF      = ROOT_SURF->findWithCM();

            // we have a surface with image description
            if (SURF && SURF->m_colorManagement.valid() && SURF->m_colorManagement->hasImageDescription()) {
                const bool surfaceIsHDR = SURF->m_colorManagement->isHDR();
                if (!SURF->m_colorManagement->isWindowsScRGB() && (*PPASS == 1 || ((*PPASS == 2 || !pMonitor->m_lastScanout.expired()) && surfaceIsHDR))) {
                    // passthrough
                    bool needsHdrMetadataUpdate = SURF->m_colorManagement->needsHdrMetadataUpdate() || pMonitor->m_previousFSWindow != FS_WINDOW || pMonitor->m_needsHDRupdate;
                    if (SURF->m_colorManagement->needsHdrMetadataUpdate()) {
                        Log::logger->log(Log::INFO, "[CM] Recreating HDR metadata for surface");
                        SURF->m_colorManagement->setHDRMetadata(createHDRMetadata(SURF->m_colorManagement->imageDescription(), pMonitor));
                    }
                    if (needsHdrMetadataUpdate) {
                        Log::logger->log(Log::INFO, "[CM] Updating HDR metadata from surface");
                        pMonitor->m_output->state->setHDRMetadata(SURF->m_colorManagement->hdrMetadata());
                    }
                    hdrIsHandled               = true;
                    pMonitor->m_needsHDRupdate = false;
                } else if (*PAUTOHDR && surfaceIsHDR)
                    wantHDR = true; // auto-hdr: hdr on
            }
        }

        if (!hdrIsHandled) {
            if (pMonitor->inHDR() != wantHDR) {
                if (*PAUTOHDR && !(pMonitor->inHDR() && configuredHDR)) {
                    // modify or restore monitor image description for auto-hdr
                    // FIXME ok for now, will need some other logic if monitor image description can be modified some other way
                    const auto targetCM      = wantHDR ? (*PAUTOHDR == 2 ? NCMType::CM_HDR_EDID : NCMType::CM_HDR) : pMonitor->m_cmType;
                    const auto targetSDREOTF = pMonitor->m_sdrEotf;
                    Log::logger->log(Log::INFO, "[CM] Auto HDR: changing monitor cm to {}", sc<uint8_t>(targetCM));
                    pMonitor->applyCMType(targetCM, targetSDREOTF);
                    pMonitor->m_previousFSWindow.reset(); // trigger CTM update
                }
                Log::logger->log(Log::INFO, wantHDR ? "[CM] Updating HDR metadata from monitor" : "[CM] Restoring SDR mode");
                pMonitor->m_output->state->setHDRMetadata(wantHDR ? createHDRMetadata(pMonitor->m_imageDescription->value(), pMonitor) : NO_HDR_METADATA);
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
                g_pHyprNotificationOverlay->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, {{"name", pMonitor->m_name}}), CHyprColor{}, 15000,
                                                            ICON_WARNING);
                shown = true;
            }
        }
    }

    if (*PCT)
        pMonitor->m_output->state->setContentType(NContentType::toDRM(FS_WINDOW ? FS_WINDOW->getContentType() : CONTENT_TYPE_NONE));

    if (FS_WINDOW != pMonitor->m_previousFSWindow || (!FS_WINDOW && pMonitor->m_noShaderCTM)) {
        if (*PNONSHADER == CM_NS_IGNORE || !FS_WINDOW || !pMonitor->needsCM() || !pMonitor->canNoShaderCM() ||
            (*PNONSHADER == CM_NS_ONDEMAND && pMonitor->m_lastScanout.expired() && *PPASS != 1)) {
            if (pMonitor->m_noShaderCTM) {
                Log::logger->log(Log::INFO, "[CM] No fullscreen CTM, restoring previous one");
                pMonitor->m_noShaderCTM = false;
                pMonitor->m_ctmUpdated  = true;
            }
        } else {
            const auto FS_DESC = pMonitor->getFSImageDescription();
            if (FS_DESC.has_value()) {
                Log::logger->log(Log::INFO, "[CM] Updating fullscreen CTM");
                pMonitor->m_noShaderCTM               = true;
                auto                       conversion = FS_DESC.value()->getPrimaries()->convertMatrix(pMonitor->m_imageDescription->getPrimaries());
                const auto                 mat        = conversion.mat();
                const std::array<float, 9> CTM        = {
                    mat[0][0], mat[0][1], mat[0][2], //
                    mat[1][0], mat[1][1], mat[1][2], //
                    mat[2][0], mat[2][1], mat[2][2], //
                };
                pMonitor->m_output->state->setCTM(CTM);
            }
        }
    }

    if (pMonitor->m_ctmUpdated && !pMonitor->m_noShaderCTM) {
        pMonitor->m_ctmUpdated = false;
        pMonitor->m_output->state->setCTM(pMonitor->m_ctm);
    }

    pMonitor->m_previousFSWindow = FS_WINDOW;

    bool ok = pMonitor->m_state.commit();
    if (!ok) {
        if (pMonitor->m_inFence.isValid()) {
            Log::logger->log(Log::TRACE, "Monitor state commit failed, retrying without a fence");
            pMonitor->m_output->state->resetExplicitFences();
            ok = pMonitor->m_state.commit();
        }

        if (!ok) {
            Log::logger->log(Log::TRACE, "Monitor state commit failed");
            // rollback the buffer to avoid writing to the front buffer that is being
            // displayed
            pMonitor->m_output->swapchain->rollback();
            pMonitor->m_damage.damageEntire();
        }
    }

    return ok;
}

void IHyprRenderer::renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry) {
    Vector2D translate = {geometry.x, geometry.y};
    float    scale     = sc<float>(geometry.width) / pMonitor->m_pixelSize.x;

    TRACY_GPU_ZONE("RenderWorkspace");

    if (!DELTALESSTHAN(sc<double>(geometry.width) / sc<double>(geometry.height), pMonitor->m_pixelSize.x / pMonitor->m_pixelSize.y, 0.01)) {
        Log::logger->log(Log::ERR, "Ignoring geometry in renderWorkspace: aspect ratio mismatch");
        scale     = 1.f;
        translate = Vector2D{};
    }

    renderAllClientsForWorkspace(pMonitor, pWorkspace, now, translate, scale);
}

void IHyprRenderer::sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now) {
    for (const auto& view : Desktop::View::getViewsForWorkspace(pWorkspace)) {
        if (!view->aliveAndVisible())
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
        if (!ls || ls->m_fadingOut || ls->m_readyToDelete || !ls->m_layerSurface || ls->m_noProcess)
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

        *ls->m_realPosition = box.pos();
        *ls->m_realSize     = box.size();
    }
}

void IHyprRenderer::arrangeLayersForMonitor(const MONITORID& monitor) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitor);

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

    if (g_pCompositor->m_unsafeState)
        return;

    const auto WLSURF = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!WLSURF) {
        Log::logger->log(Log::ERR, "BUG THIS: No CWLSurface for surface in damageSurface!!!");
        return;
    }

    // hack: schedule frame events
    if (!WLSURF->resource()->m_current.callbacks.empty() && pSurface->m_hlSurface) {
        const auto BOX = pSurface->m_hlSurface->getSurfaceBoxGlobal();
        if (BOX && !BOX->empty()) {
            for (auto const& m : g_pCompositor->m_monitors) {
                if (!m->m_output)
                    continue;

                if (BOX->overlaps(m->logicalBox()))
                    g_pCompositor->scheduleFrameForMonitor(m, Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
            }
        }
    }

    CRegion damageBox = WLSURF->computeDamage();
    if (damageBox.empty())
        return;

    if (scale != 1.0)
        damageBox.scale(scale);

    damageBox.translate({x, y});

    CRegion damageBoxForEach;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!m->m_output)
            continue;

        damageBoxForEach.set(damageBox);
        damageBoxForEach.translate({-m->m_position.x, -m->m_position.y}).scale(m->m_scale);

        m->addDamage(damageBoxForEach);
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Surface (extents): xy: {}, {} wh: {}, {}", damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y1,
                         damageBox.pixman()->extents.x2 - damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y2 - damageBox.pixman()->extents.y1);
}

void IHyprRenderer::damageWindow(PHLWINDOW pWindow, bool forceFull) {
    if (g_pCompositor->m_unsafeState)
        return;

    CBox       windowBox        = pWindow->getFullWindowBoundingBox();
    const auto PWINDOWWORKSPACE = pWindow->m_workspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !pWindow->m_pinned)
        windowBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
    windowBox.translate(pWindow->m_floatingOffset);

    for (auto const& m : g_pCompositor->m_monitors) {
        if (forceFull || shouldRenderWindow(pWindow, m)) { // only damage if window is rendered on monitor
            CBox fixedDamageBox = {windowBox.x - m->m_position.x, windowBox.y - m->m_position.y, windowBox.width, windowBox.height};
            fixedDamageBox.scale(m->m_scale);
            m->addDamage(fixedDamageBox);
        }
    }

    for (auto const& wd : pWindow->m_windowDecorations)
        wd->damageEntire();

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Window ({}): xy: {}, {} wh: {}, {}", pWindow->m_title, windowBox.x, windowBox.y, windowBox.width, windowBox.height);
}

void IHyprRenderer::damageMonitor(PHLMONITOR pMonitor) {
    if (g_pCompositor->m_unsafeState || pMonitor->isMirror())
        return;

    CBox damageBox = {0, 0, INT16_MAX, INT16_MAX};
    pMonitor->addDamage(damageBox);

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Monitor {}", pMonitor->m_name);
}

void IHyprRenderer::damageBox(const CBox& box, bool skipFrameSchedule) {
    if (g_pCompositor->m_unsafeState)
        return;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->isMirror())
            continue; // don't damage mirrors traditionally

        if (!skipFrameSchedule) {
            CBox damageBox = box.copy().translate(-m->m_position).scale(m->m_scale).round();
            m->addDamage(damageBox);
        }
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

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
        transformed.transform(Math::wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_pixelSize.x * scale, pMonitor->m_pixelSize.y * scale);
        transformed.translate(Vector2D(monbox.x, monbox.y));

        mirror->addDamage(transformed);

        g_pCompositor->scheduleFrameForMonitor(mirror.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }
}

void IHyprRenderer::renderDragIcon(PHLMONITOR pMonitor, const Time::steady_tp& time) {
    PROTO::data->renderDND(pMonitor, time);
}

void IHyprRenderer::setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force) {
    m_cursorHasSurface = surf && surf->resource();

    m_lastCursorData.name     = "";
    m_lastCursorData.surf     = surf;
    m_lastCursorData.hotspotX = hotspotX;
    m_lastCursorData.hotspotY = hotspotY;

    if (m_cursorHidden && !force)
        return;

    g_pCursorManager->setCursorSurface(surf, {hotspotX, hotspotY});
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

    g_pCursorManager->setCursorFromName(name);
}

void IHyprRenderer::ensureCursorRenderingMode() {
    static auto PINVISIBLE     = CConfigValue<Hyprlang::INT>("cursor:invisible");
    static auto PCURSORTIMEOUT = CConfigValue<Hyprlang::FLOAT>("cursor:inactive_timeout");
    static auto PHIDEONTOUCH   = CConfigValue<Hyprlang::INT>("cursor:hide_on_touch");
    static auto PHIDEONTABLET  = CConfigValue<Hyprlang::INT>("cursor:hide_on_tablet");
    static auto PHIDEONKEY     = CConfigValue<Hyprlang::INT>("cursor:hide_on_key_press");

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

    const bool HIDE = m_cursorHiddenByCondition || (*PINVISIBLE != 0);

    if (HIDE == m_cursorHidden)
        return;

    if (HIDE)
        Log::logger->log(Log::DEBUG, "Hiding the cursor (hl-mandated)");
    else
        Log::logger->log(Log::DEBUG, "Showing the cursor (hl-mandated)");

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!g_pPointerManager->softwareLockedFor(m))
            continue;

        g_pPointerManager->damageCursor(m, m->shouldSkipScheduleFrameOnMouseEvent());
    }

    setCursorHidden(HIDE);
}

void IHyprRenderer::setCursorHidden(bool hide) {

    if (hide == m_cursorHidden)
        return;

    m_cursorHidden = hide;

    if (hide) {
        g_pPointerManager->resetCursorImage();
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
    const auto POVERLAY = &g_pDebugOverlay->m_monitorOverlays[pMonitor];

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

    g_pHyprNotificationOverlay->addNotification("Hyprland will crash in " + std::to_string(10 - sc<int>(g_pHyprRenderer->m_crashingDistort * 2.f)) + "s.", CHyprColor(0), 5000,
                                                ICON_INFO);

    g_pHyprRenderer->m_crashingDistort += 0.5f;

    if (g_pHyprRenderer->m_crashingDistort >= 5.5f)
        raise(SIGABRT);

    wl_event_source_timer_update(g_pHyprRenderer->m_crashingLoop, 1000);

    return 1;
}

void IHyprRenderer::initiateManualCrash() {
    g_pHyprNotificationOverlay->addNotification("Manual crash initiated. Farewell...", CHyprColor(0), 5000, ICON_INFO);

    m_crashingLoop = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_crashingLoop, 1000);

    m_crashingInProgress = true;
    m_crashingDistort    = 0.5;

    m_globalTimer.reset();

    static auto PDT = rc<Hyprlang::INT* const*>(g_pConfigManager->getConfigValuePtr("debug:damage_tracking"));

    **PDT = 0;
}

const SRenderData& IHyprRenderer::renderData() {
    return m_renderData;
}

SP<CRenderbuffer> IHyprRenderer::getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto it = std::ranges::find_if(m_renderbuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });

    if (it != m_renderbuffers.end())
        return *it;

    auto buf = makeShared<CRenderbuffer>(buffer, fmt);

    if (!buf->good())
        return nullptr;

    m_renderbuffers.emplace_back(buf);
    return buf;
}

bool IHyprRenderer::beginFullFakeRender(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    return beginRender(pMonitor, damage, RENDER_MODE_FULL_FAKE, nullptr, fb, simple);
}

bool IHyprRenderer::beginRenderToBuffer(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer, bool simple) {
    return beginRender(pMonitor, damage, RENDER_MODE_TO_BUFFER, buffer, nullptr, simple);
}

void IHyprRenderer::onRenderbufferDestroy(CRenderbuffer* rb) {
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
    static auto PFPS = CConfigValue<Hyprlang::INT>("misc:render_unfocused_fps");

    if (std::ranges::find(m_renderUnfocused, window) != m_renderUnfocused.end())
        return;

    m_renderUnfocused.emplace_back(window);

    if (!m_renderUnfocusedTimer->armed())
        m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
}

void IHyprRenderer::makeSnapshot(PHLWINDOW pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    if (!shouldRenderWindow(pWindow))
        return; // ignore, window is not being rendered

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion      fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    PHLWINDOWREF ref{pWindow};

    if (!ref->m_snapshotFB)
        ref->m_snapshotFB = g_pHyprRenderer->createFB();

    const auto PFRAMEBUFFER = ref->m_snapshotFB;

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginFullFakeRender(PMONITOR, fakeDamage, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    auto clear = makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
    draw(clear.get(), {});

    renderWindow(pWindow, PMONITOR, Time::steadyNow(), !pWindow->m_X11DoesntWantBorders, RENDER_PASS_ALL);

    endRender();

    m_bRenderingSnapshot = false;
}

void IHyprRenderer::makeSnapshot(PHLLS pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = pLayer->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(pLayer.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    g_pHyprOpenGL->makeEGLCurrent();

    if (!g_pHyprOpenGL->m_layerFramebuffers.contains(pLayer))
        g_pHyprOpenGL->m_layerFramebuffers[pLayer] = g_pHyprRenderer->createFB();

    const auto PFRAMEBUFFER = g_pHyprOpenGL->m_layerFramebuffers[pLayer];

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginFullFakeRender(PMONITOR, fakeDamage, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    // draw the layer
    renderLayer(pLayer, PMONITOR, Time::steadyNow());

    endRender();

    m_bRenderingSnapshot = false;
}

void IHyprRenderer::makeSnapshot(WP<Desktop::View::CPopup> popup) {
    // we trust the window is valid.
    const auto PMONITOR = popup->getMonitor();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    if (!popup->aliveAndVisible())
        return;

    Log::logger->log(Log::DEBUG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(popup.get()));

    CRegion fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    g_pHyprOpenGL->makeEGLCurrent();

    if (g_pHyprOpenGL->m_popupFramebuffers.contains(popup))
        g_pHyprOpenGL->m_popupFramebuffers[popup] = g_pHyprRenderer->createFB();

    const auto PFRAMEBUFFER = g_pHyprOpenGL->m_popupFramebuffers[popup];

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginFullFakeRender(PMONITOR, fakeDamage, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    CSurfacePassElement::SRenderData renderdata;
    renderdata.pos             = popup->coordsGlobal();
    renderdata.alpha           = 1.F;
    renderdata.dontRound       = true; // don't round popups
    renderdata.pMonitor        = PMONITOR;
    renderdata.squishOversized = false; // don't squish popups
    renderdata.popup           = true;
    renderdata.blur            = false;

    popup->wlSurface()->resource()->breadthfirst(
        [this, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = false;
            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        nullptr);

    endRender();

    m_bRenderingSnapshot = false;
}

void IHyprRenderer::renderSnapshot(PHLWINDOW pWindow) {
    static auto  PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    PHLWINDOWREF ref{pWindow};

    if (!ref->m_snapshotFB)
        return;

    const auto FBDATA = ref->m_snapshotFB;

    if (!FBDATA->getTexture())
        return;

    const auto PMONITOR = pWindow->m_monitor.lock();

    CBox       windowBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->m_scale * pWindow->m_realSize->value().x / (pWindow->m_originalClosedSize.x * PMONITOR->m_scale)),
                                (PMONITOR->m_scale * pWindow->m_realSize->value().y / (pWindow->m_originalClosedSize.y * PMONITOR->m_scale)));

    windowBox.width  = PMONITOR->m_transformedSize.x * scaleXY.x;
    windowBox.height = PMONITOR->m_transformedSize.y * scaleXY.y;
    windowBox.x      = ((pWindow->m_realPosition->value().x - PMONITOR->m_position.x) * PMONITOR->m_scale) - ((pWindow->m_originalClosedPos.x * PMONITOR->m_scale) * scaleXY.x);
    windowBox.y      = ((pWindow->m_realPosition->value().y - PMONITOR->m_position.y) * PMONITOR->m_scale) - ((pWindow->m_originalClosedPos.y * PMONITOR->m_scale) * scaleXY.y);

    CRegion fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault()) {
        CRectPassElement::SRectData data;

        data.box   = {0, 0, m_renderData.pMonitor->m_pixelSize.x, m_renderData.pMonitor->m_pixelSize.y};
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pWindow->m_alpha->value());

        m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    if (shouldBlur(pWindow)) {
        CRectPassElement::SRectData data;
        data.box           = CBox{pWindow->m_realPosition->value(), pWindow->m_realSize->value()}.translate(-PMONITOR->m_position).scale(PMONITOR->m_scale).round();
        data.color         = CHyprColor{0, 0, 0, 0};
        data.blur          = true;
        data.blurA         = sqrt(pWindow->m_alpha->value()); // sqrt makes the blur fadeout more realistic.
        data.round         = pWindow->rounding();
        data.roundingPower = pWindow->roundingPower();
        data.xray          = pWindow->m_ruleApplicator->xray().valueOr(false);

        m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    CTexPassElement::SRenderData data;
    data.flipEndFrame = true;
    data.tex          = FBDATA->getTexture();
    data.box          = windowBox;
    data.a            = pWindow->m_alpha->value();
    data.damage       = fakeDamage;

    m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void IHyprRenderer::renderSnapshot(PHLLS pLayer) {
    if (!g_pHyprOpenGL->m_layerFramebuffers.contains(pLayer))
        return;

    const auto FBDATA = g_pHyprOpenGL->m_layerFramebuffers.at(pLayer);

    if (!FBDATA->getTexture())
        return;

    const auto PMONITOR = pLayer->m_monitor.lock();

    CBox       layerBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->m_scale * pLayer->m_realSize->value().x / (pLayer->m_geometry.w * PMONITOR->m_scale)),
                                (PMONITOR->m_scale * pLayer->m_realSize->value().y / (pLayer->m_geometry.h * PMONITOR->m_scale)));

    layerBox.width  = PMONITOR->m_transformedSize.x * scaleXY.x;
    layerBox.height = PMONITOR->m_transformedSize.y * scaleXY.y;
    layerBox.x =
        ((pLayer->m_realPosition->value().x - PMONITOR->m_position.x) * PMONITOR->m_scale) - (((pLayer->m_geometry.x - PMONITOR->m_position.x) * PMONITOR->m_scale) * scaleXY.x);
    layerBox.y =
        ((pLayer->m_realPosition->value().y - PMONITOR->m_position.y) * PMONITOR->m_scale) - (((pLayer->m_geometry.y - PMONITOR->m_position.y) * PMONITOR->m_scale) * scaleXY.y);

    CRegion                      fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    const bool                   SHOULD_BLUR = shouldBlur(pLayer);

    CTexPassElement::SRenderData data;
    data.flipEndFrame = true;
    data.tex          = FBDATA->getTexture();
    data.box          = layerBox;
    data.a            = pLayer->m_alpha->value();
    data.damage       = fakeDamage;
    data.blur         = SHOULD_BLUR;
    data.blurA        = sqrt(pLayer->m_alpha->value()); // sqrt makes the blur fadeout more realistic.
    if (SHOULD_BLUR)
        data.ignoreAlpha = pLayer->m_ruleApplicator->ignoreAlpha().valueOr(0.01F) /* ignore the alpha 0 regions */;

    m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void IHyprRenderer::renderSnapshot(WP<Desktop::View::CPopup> popup) {
    if (!g_pHyprOpenGL->m_popupFramebuffers.contains(popup))
        return;

    static CConfigValue PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:popups_ignorealpha");

    const auto          FBDATA = g_pHyprOpenGL->m_popupFramebuffers.at(popup);

    if (!FBDATA->getTexture())
        return;

    const auto PMONITOR = popup->getMonitor();

    if (!PMONITOR)
        return;

    CRegion                      fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    const bool                   SHOULD_BLUR = shouldBlur(popup);

    CTexPassElement::SRenderData data;
    data.flipEndFrame          = true;
    data.tex                   = FBDATA->getTexture();
    data.box                   = {{}, PMONITOR->m_transformedSize};
    data.a                     = popup->m_alpha->value();
    data.damage                = fakeDamage;
    data.blur                  = SHOULD_BLUR;
    data.blurA                 = sqrt(popup->m_alpha->value()); // sqrt makes the blur fadeout more realistic.
    data.blockBlurOptimization = SHOULD_BLUR;                   // force no xray on this (popups never have xray)
    if (SHOULD_BLUR)
        data.ignoreAlpha = std::max(*PBLURIGNOREA, 0.01F); /* ignore the alpha 0 regions */

    m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

bool IHyprRenderer::shouldBlur(PHLLS ls) {
    if (m_bRenderingSnapshot)
        return false;

    static auto PBLUR = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    return *PBLUR && ls->m_ruleApplicator->blur().valueOrDefault();
}

bool IHyprRenderer::shouldBlur(PHLWINDOW w) {
    if (m_bRenderingSnapshot)
        return false;

    static auto PBLUR     = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    const bool  DONT_BLUR = w->m_ruleApplicator->noBlur().valueOrDefault() || w->m_ruleApplicator->RGBX().valueOrDefault() || w->opaque();
    return *PBLUR && !DONT_BLUR;
}

bool IHyprRenderer::shouldBlur(WP<Desktop::View::CPopup> p) {
    static CConfigValue PBLURPOPUPS = CConfigValue<Hyprlang::INT>("decoration:blur:popups");
    static CConfigValue PBLUR       = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    return *PBLURPOPUPS && *PBLUR;
}

SP<ITexture> IHyprRenderer::renderSplash(const std::function<SP<ITexture>(const int, const int, unsigned char* const)>& handleData, const int fontSize, const int maxWidth,
                                         const int maxHeight) {
    static auto PSPLASHCOLOR = CConfigValue<Hyprlang::INT>("misc:col.splash");
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
