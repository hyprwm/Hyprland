#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/sync/SyncReleaser.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <cstring>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../managers/CursorManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/LinuxDMABUF.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "debug/Log.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

extern "C" {
#include <xf86drm.h>
}

static int cursorTicker(void* data) {
    g_pHyprRenderer->ensureCursorRenderingMode();
    wl_event_source_timer_update(g_pHyprRenderer->m_pCursorTicker, 500);
    return 0;
}

CHyprRenderer::CHyprRenderer() {
    if (g_pCompositor->m_pAqBackend->hasSession()) {
        for (auto const& dev : g_pCompositor->m_pAqBackend->session->sessionDevices) {
            const auto DRMV = drmGetVersion(dev->fd);
            if (!DRMV)
                continue;
            std::string name = std::string{DRMV->name, DRMV->name_len};
            std::transform(name.begin(), name.end(), name.begin(), tolower);

            if (name.contains("nvidia"))
                m_bNvidia = true;

            Debug::log(LOG, "DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor, DRMV->version_patchlevel,
                       std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});

            drmFreeVersion(DRMV);
        }
    } else {
        Debug::log(LOG, "Aq backend has no session, omitting full DRM node checks");

        const auto DRMV = drmGetVersion(g_pCompositor->m_iDRMFD);

        if (DRMV) {
            std::string name = std::string{DRMV->name, DRMV->name_len};
            std::transform(name.begin(), name.end(), name.begin(), tolower);

            if (name.contains("nvidia"))
                m_bNvidia = true;

            Debug::log(LOG, "Primary DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor, DRMV->version_patchlevel,
                       std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});
        } else {
            Debug::log(LOG, "No primary DRM driver information found");
        }

        drmFreeVersion(DRMV);
    }

    if (m_bNvidia)
        Debug::log(WARN, "NVIDIA detected, please remember to follow nvidia instructions on the wiki");

    // cursor hiding stuff

    static auto P = g_pHookSystem->hookDynamic("keyPress", [&](void* self, SCallbackInfo& info, std::any param) {
        if (m_sCursorHiddenConditions.hiddenOnKeyboard)
            return;

        m_sCursorHiddenConditions.hiddenOnKeyboard = true;
        ensureCursorRenderingMode();
    });

    static auto P2 = g_pHookSystem->hookDynamic("mouseMove", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_sCursorHiddenConditions.hiddenOnKeyboard && m_sCursorHiddenConditions.hiddenOnTouch == g_pInputManager->m_bLastInputTouch &&
            !m_sCursorHiddenConditions.hiddenOnTimeout)
            return;

        m_sCursorHiddenConditions.hiddenOnKeyboard = false;
        m_sCursorHiddenConditions.hiddenOnTimeout  = false;
        m_sCursorHiddenConditions.hiddenOnTouch    = g_pInputManager->m_bLastInputTouch;
        ensureCursorRenderingMode();
    });

    static auto P3 = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        g_pEventLoopManager->doLater([this]() {
            if (!g_pHyprError->active())
                return;
            for (auto& m : g_pCompositor->m_vMonitors) {
                arrangeLayersForMonitor(m->ID);
            }
        });
    });

    m_pCursorTicker = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, cursorTicker, nullptr);
    wl_event_source_timer_update(m_pCursorTicker, 500);

    m_tRenderUnfocusedTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            static auto PFPS = CConfigValue<Hyprlang::INT>("misc:render_unfocused_fps");

            if (m_vRenderUnfocused.empty())
                return;

            timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            bool dirty = false;
            for (auto& w : m_vRenderUnfocused) {
                if (!w) {
                    dirty = true;
                    continue;
                }

                if (!w->m_pWLSurface || !w->m_pWLSurface->resource() || shouldRenderWindow(w.lock()))
                    continue;

                w->m_pWLSurface->resource()->frame(&now);
                auto FEEDBACK = makeShared<CQueuedPresentationData>(w->m_pWLSurface->resource());
                FEEDBACK->attachMonitor(g_pCompositor->m_pLastMonitor.lock());
                FEEDBACK->discarded();
                PROTO::presentation->queueData(FEEDBACK);
            }

            if (dirty)
                std::erase_if(m_vRenderUnfocused, [](const auto& e) { return !e || !e->m_sWindowData.renderUnfocused.valueOr(false); });

            if (!m_vRenderUnfocused.empty())
                m_tRenderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
        },
        nullptr);

    g_pEventLoopManager->addTimer(m_tRenderUnfocusedTimer);
}

CHyprRenderer::~CHyprRenderer() {
    if (m_pCursorTicker)
        wl_event_source_remove(m_pCursorTicker);
}

static void renderSurface(SP<CWLSurfaceResource> surface, int x, int y, void* data) {
    if (!surface->current.texture)
        return;

    const auto& TEXTURE = surface->current.texture;

    // this is bad, probably has been logged elsewhere. Means the texture failed
    // uploading to the GPU.
    if (!TEXTURE->m_iTexID)
        return;

    // explicit sync: wait for the timeline, if any
    if (surface->syncobj && surface->syncobj->current.acquireTimeline) {
        if (!g_pHyprOpenGL->waitForTimelinePoint(surface->syncobj->current.acquireTimeline->timeline, surface->syncobj->current.acquirePoint)) {
            Debug::log(ERR, "Renderer: failed to wait for explicit timeline");
            return;
        }
    }

    const auto RDATA                       = (SRenderData*)data;
    const auto INTERACTIVERESIZEINPROGRESS = RDATA->pWindow && g_pInputManager->currentlyDraggedWindow && g_pInputManager->dragMode == MBIND_RESIZE;
    TRACY_GPU_ZONE("RenderSurface");

    double      outputX = -RDATA->pMonitor->vecPosition.x, outputY = -RDATA->pMonitor->vecPosition.y;

    auto        PSURFACE = CWLSurface::fromResource(surface);

    const float ALPHA = RDATA->alpha * RDATA->fadeAlpha * (PSURFACE ? PSURFACE->m_pAlphaModifier : 1.F);
    const bool  BLUR  = RDATA->blur && (!TEXTURE->m_bOpaque || ALPHA < 1.F);

    CBox        windowBox;
    if (RDATA->surface && surface == RDATA->surface) {
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, RDATA->w, RDATA->h};

        // however, if surface buffer w / h < box, we need to adjust them
        const auto PWINDOW = PSURFACE ? PSURFACE->getWindow() : nullptr;

        // center the surface if it's smaller than the viewport we assign it
        if (PSURFACE && !PSURFACE->m_bFillIgnoreSmall && PSURFACE->small() /* guarantees PWINDOW */) {
            const auto CORRECT = PSURFACE->correctSmallVec();
            const auto SIZE    = PSURFACE->getViewporterCorrectedSize();

            if (!INTERACTIVERESIZEINPROGRESS) {
                windowBox.translate(CORRECT);

                windowBox.width  = SIZE.x * (PWINDOW->m_vRealSize.value().x / PWINDOW->m_vReportedSize.x);
                windowBox.height = SIZE.y * (PWINDOW->m_vRealSize.value().y / PWINDOW->m_vReportedSize.y);
            } else {
                windowBox.width  = SIZE.x;
                windowBox.height = SIZE.y;
            }
        }

    } else { //  here we clamp to 2, these might be some tiny specks
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, std::max((float)surface->current.size.x, 2.F), std::max((float)surface->current.size.y, 2.F)};
        if (RDATA->pWindow && RDATA->pWindow->m_vRealSize.isBeingAnimated() && RDATA->surface && RDATA->surface != surface && RDATA->squishOversized /* subsurface */) {
            // adjust subsurfaces to the window
            windowBox.width  = (windowBox.width / RDATA->pWindow->m_vReportedSize.x) * RDATA->pWindow->m_vRealSize.value().x;
            windowBox.height = (windowBox.height / RDATA->pWindow->m_vReportedSize.y) * RDATA->pWindow->m_vRealSize.value().y;
        }
    }

    if (RDATA->squishOversized) {
        if (x + windowBox.width > RDATA->w)
            windowBox.width = RDATA->w - x;
        if (y + windowBox.height > RDATA->h)
            windowBox.height = RDATA->h - y;
    }

    const auto PROJSIZEUNSCALED = windowBox.size();

    windowBox.scale(RDATA->pMonitor->scale);
    windowBox.round();

    if (windowBox.width <= 1 || windowBox.height <= 1) {
        if (!g_pHyprRenderer->m_bBlockSurfaceFeedback) {
            Debug::log(TRACE, "presentFeedback for invisible surface");
            surface->presentFeedback(RDATA->when, RDATA->pMonitor->self.lock());
        }

        return; // invisible
    }

    const bool MISALIGNEDFSV1 = std::floor(RDATA->pMonitor->scale) != RDATA->pMonitor->scale /* Fractional */ && surface->current.scale == 1 /* fs protocol */ &&
        windowBox.size() != surface->current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, surface->current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, surface->current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!RDATA->pWindow || (!RDATA->pWindow->m_vRealSize.isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */;

    g_pHyprRenderer->calculateUVForSurface(RDATA->pWindow, surface, RDATA->pMonitor->self.lock(), RDATA->surface == surface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    const auto NEARESTNEIGHBORSET = g_pHyprOpenGL->m_RenderData.useNearestNeighbor;
    if (MISALIGNEDFSV1)
        g_pHyprOpenGL->m_RenderData.useNearestNeighbor = true;

    float rounding = RDATA->rounding;

    rounding -= 1; // to fix a border issue

    if (RDATA->dontRound)
        rounding = 0;

    const bool WINDOWOPAQUE    = RDATA->pWindow && RDATA->pWindow->m_pWLSurface->resource() == surface ? RDATA->pWindow->opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && rounding == 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprOpenGL->blend(false);
    else
        g_pHyprOpenGL->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (RDATA->surfaceCounter == 0 && !RDATA->popup) {
        if (BLUR)
            g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, ALPHA, surface, rounding, RDATA->blockBlurOptimization, RDATA->fadeAlpha);
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, ALPHA, rounding, false, true);
    } else {
        if (BLUR && RDATA->popup)
            g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, ALPHA, surface, rounding, true, RDATA->fadeAlpha);
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, ALPHA, rounding, false, true);
    }

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback)
        surface->presentFeedback(RDATA->when, RDATA->pMonitor->self.lock());

    g_pHyprOpenGL->blend(true);

    // reset props
    g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    g_pHyprOpenGL->m_RenderData.useNearestNeighbor          = NEARESTNEIGHBORSET;

    // up the counter so that we dont blur any surfaces above this one
    RDATA->surfaceCounter++;
}

bool CHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    if (!pWindow->visibleOnMonitor(pMonitor))
        return false;

    if (!pWindow->m_pWorkspace && !pWindow->m_bFadingOut)
        return false;

    if (!pWindow->m_pWorkspace && pWindow->m_bFadingOut)
        return pWindow->workspaceID() == pMonitor->activeWorkspaceID();

    if (pWindow->m_bPinned)
        return true;

    // if the window is being moved to a workspace that is not invisible, and the alpha is > 0.F, render it.
    if (pWindow->m_iMonitorMovedFrom != -1 && pWindow->m_fMovingToWorkspaceAlpha.isBeingAnimated() && pWindow->m_fMovingToWorkspaceAlpha.value() > 0.F && pWindow->m_pWorkspace &&
        !pWindow->m_pWorkspace->isVisible())
        return true;

    const auto PWINDOWWORKSPACE = pWindow->m_pWorkspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_pMonitor == pMonitor) {
        if (PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWINDOWWORKSPACE->m_fAlpha.isBeingAnimated() || PWINDOWWORKSPACE->m_bForceRendering)
            return true;

        // if hidden behind fullscreen
        if (PWINDOWWORKSPACE->m_bHasFullscreenWindow && !pWindow->isFullscreen() && (!pWindow->m_bIsFloating || !pWindow->m_bCreatedOverFullscreen) &&
            pWindow->m_fAlpha.value() == 0)
            return false;

        if (!PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated() && !PWINDOWWORKSPACE->m_fAlpha.isBeingAnimated() && !PWINDOWWORKSPACE->isVisible())
            return false;
    }

    if (pWindow->m_pMonitor == pMonitor)
        return true;

    if ((!pWindow->m_pWorkspace || !pWindow->m_pWorkspace->isVisible()) && pWindow->m_pMonitor != pMonitor)
        return false;

    // if not, check if it maybe is active on a different monitor.
    if (pWindow->m_pWorkspace && pWindow->m_pWorkspace->isVisible() && pWindow->m_bIsFloating /* tiled windows can't be multi-ws */)
        return !pWindow->isFullscreen(); // Do not draw fullscreen windows on other monitors

    if (pMonitor->activeSpecialWorkspace == pWindow->m_pWorkspace)
        return true;

    // if window is tiled and it's flying in, don't render on other mons (for slide)
    if (!pWindow->m_bIsFloating && pWindow->m_vRealPosition.isBeingAnimated() && pWindow->m_bAnimatingIn && pWindow->m_pMonitor != pMonitor)
        return false;

    if (pWindow->m_vRealPosition.isBeingAnimated()) {
        if (PWINDOWWORKSPACE && !PWINDOWWORKSPACE->m_bIsSpecialWorkspace && PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated())
            return false;
        // render window if window and monitor intersect
        // (when moving out of or through a monitor)
        CBox windowBox = pWindow->getFullWindowBoundingBox();
        if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated())
            windowBox.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
        windowBox.translate(pWindow->m_vFloatingOffset);

        const CBox monitorBox = {pMonitor->vecPosition, pMonitor->vecSize};
        if (!windowBox.intersection(monitorBox).empty() && (pWindow->workspaceID() == pMonitor->activeWorkspaceID() || pWindow->m_iMonitorMovedFrom != -1))
            return true;
    }

    return false;
}

bool CHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow) {

    if (!validMapped(pWindow))
        return false;

    const auto PWORKSPACE = pWindow->m_pWorkspace;

    if (!pWindow->m_pWorkspace)
        return false;

    if (pWindow->m_bPinned || PWORKSPACE->m_bForceRendering)
        return true;

    if (PWORKSPACE && PWORKSPACE->isVisible())
        return true;

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (PWORKSPACE && PWORKSPACE->m_pMonitor == m && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated()))
            return true;

        if (m->activeSpecialWorkspace && pWindow->onSpecialWorkspace())
            return true;
    }

    return false;
}

void CHyprRenderer::renderWorkspaceWindowsFullscreen(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* time) {
    PHLWINDOW pWorkspaceWindow = nullptr;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOWS);

    // loop over the tiled windows that are fading out
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!shouldRenderWindow(w, pMonitor))
            continue;

        if (w->m_fAlpha.value() == 0.f)
            continue;

        if (w->isFullscreen() || w->m_bIsFloating)
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and floating ones too
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!shouldRenderWindow(w, pMonitor))
            continue;

        if (w->m_fAlpha.value() == 0.f)
            continue;

        if (w->isFullscreen() || !w->m_bIsFloating)
            continue;

        if (w->m_pMonitor == pWorkspace->m_pMonitor && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace && w->m_pMonitor != pWorkspace->m_pMonitor)
            continue; // special on another are rendered as a part of the base pass

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    // TODO: this pass sucks
    for (auto const& w : g_pCompositor->m_vWindows) {
        const auto PWORKSPACE = w->m_pWorkspace;

        if (w->m_pWorkspace != pWorkspace || !w->isFullscreen()) {
            if (!(PWORKSPACE && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated() || PWORKSPACE->m_bForceRendering)))
                continue;

            if (w->m_pMonitor != pMonitor)
                continue;
        }

        if (!w->isFullscreen())
            continue;

        if (w->m_pMonitor == pWorkspace->m_pMonitor && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (shouldRenderWindow(w, pMonitor))
            renderWindow(w, pMonitor, time, pWorkspace->m_efFullscreenMode != FSMODE_FULLSCREEN, RENDER_PASS_ALL);

        if (w->m_pWorkspace != pWorkspace)
            continue;

        pWorkspaceWindow = w;
    }

    if (!pWorkspaceWindow) {
        // ?? happens sometimes...
        pWorkspace->m_bHasFullscreenWindow = false;
        return; // this will produce one blank frame. Oh well.
    }

    // then render windows over fullscreen.
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != pWorkspaceWindow->m_pWorkspace || (!w->m_bCreatedOverFullscreen && !w->m_bPinned) || (!w->m_bIsMapped && !w->m_bFadingOut) || w->isFullscreen())
            continue;

        if (w->m_pMonitor == pWorkspace->m_pMonitor && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace && w->m_pMonitor != pWorkspace->m_pMonitor)
            continue; // special on another are rendered as a part of the base pass

        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }
}

void CHyprRenderer::renderWorkspaceWindows(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* time) {
    PHLWINDOW lastWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOWS);

    std::vector<PHLWINDOWREF> windows;
    windows.reserve(g_pCompositor->m_vWindows.size());

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || (!w->m_bIsMapped && !w->m_bFadingOut))
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        windows.push_back(w);
    }

    // Non-floating main
    for (auto& w : windows) {
        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        if (pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render active window after all others of this pass
        if (w == g_pCompositor->m_pLastWindow) {
            lastWindow = w.lock();
            continue;
        }

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
    }

    if (lastWindow)
        renderWindow(lastWindow, pMonitor, time, true, RENDER_PASS_MAIN);

    // Non-floating popup
    for (auto& w : windows) {
        if (!w)
            continue;

        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        if (pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_POPUP);
        w.reset();
    }

    // floating on top
    for (auto& w : windows) {
        if (!w)
            continue;

        if (!w->m_bIsFloating || w->m_bPinned)
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace && w->m_pMonitor != pWorkspace->m_pMonitor)
            continue; // special on another are rendered as a part of the base pass

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_ALL);
    }
}

void CHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, timespec* time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool ignoreAllGeometry) {
    if (pWindow->isHidden())
        return;

    if (pWindow->m_bFadingOut) {
        if (pMonitor == pWindow->m_pMonitor) // TODO: fix this
            g_pHyprOpenGL->renderSnapshot(pWindow);
        return;
    }

    if (!pWindow->m_bIsMapped)
        return;

    TRACY_GPU_ZONE("RenderWindow");

    const auto  PWORKSPACE = pWindow->m_pWorkspace;
    const auto  REALPOS    = pWindow->m_vRealPosition.value() + (pWindow->m_bPinned ? Vector2D{} : PWORKSPACE->m_vRenderOffset.value());
    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");
    static auto PBLUR      = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    SRenderData renderdata = {pMonitor, time};
    CBox        textureBox = {REALPOS.x, REALPOS.y, std::max(pWindow->m_vRealSize.value().x, 5.0), std::max(pWindow->m_vRealSize.value().y, 5.0)};

    renderdata.x = textureBox.x;
    renderdata.y = textureBox.y;
    renderdata.w = textureBox.w;
    renderdata.h = textureBox.h;

    if (ignorePosition) {
        renderdata.x = pMonitor->vecPosition.x;
        renderdata.y = pMonitor->vecPosition.y;
    }

    if (ignoreAllGeometry)
        decorate = false;

    // whether to use m_fMovingToWorkspaceAlpha, only if fading out into an invisible ws
    const bool USE_WORKSPACE_FADE_ALPHA = pWindow->m_iMonitorMovedFrom != -1 && (!PWORKSPACE || !PWORKSPACE->isVisible());
    const bool DONT_BLUR                = pWindow->m_sWindowData.noBlur.valueOrDefault() || pWindow->m_sWindowData.RGBX.valueOrDefault() || pWindow->opaque();

    renderdata.surface   = pWindow->m_pWLSurface->resource();
    renderdata.dontRound = pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN) || pWindow->m_sWindowData.noRounding.valueOrDefault();
    renderdata.fadeAlpha = pWindow->m_fAlpha.value() * (pWindow->m_bPinned || USE_WORKSPACE_FADE_ALPHA ? 1.f : PWORKSPACE->m_fAlpha.value()) *
        (USE_WORKSPACE_FADE_ALPHA ? pWindow->m_fMovingToWorkspaceAlpha.value() : 1.F);
    renderdata.alpha    = pWindow->m_fActiveInactiveAlpha.value();
    renderdata.decorate = decorate && !pWindow->m_bX11DoesntWantBorders && !pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderdata.rounding = ignoreAllGeometry || renderdata.dontRound ? 0 : pWindow->rounding() * pMonitor->scale;
    renderdata.blur     = !ignoreAllGeometry && *PBLUR && !DONT_BLUR;
    renderdata.pWindow  = pWindow;

    if (ignoreAllGeometry) {
        renderdata.alpha     = 1.f;
        renderdata.fadeAlpha = 1.f;
    }

    // apply opaque
    if (pWindow->m_sWindowData.opaque.valueOrDefault())
        renderdata.alpha = 1.f;

    g_pHyprOpenGL->m_pCurrentWindow = pWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOW);

    if (*PDIMAROUND && pWindow->m_sWindowData.dimAround.valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {
        CBox monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};
        g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMAROUND * renderdata.alpha * renderdata.fadeAlpha));
    }

    renderdata.x += pWindow->m_vFloatingOffset.x;
    renderdata.y += pWindow->m_vFloatingOffset.y;

    // if window is floating and we have a slide animation, clip it to its full bb
    if (!ignorePosition && pWindow->m_bIsFloating && !pWindow->isFullscreen() && PWORKSPACE->m_vRenderOffset.isBeingAnimated() && !pWindow->m_bPinned) {
        CRegion rg =
            pWindow->getFullWindowBoundingBox().translate(-pMonitor->vecPosition + PWORKSPACE->m_vRenderOffset.value() + pWindow->m_vFloatingOffset).scale(pMonitor->scale);
        g_pHyprOpenGL->m_RenderData.clipBox = rg.getExtents();
    }

    // render window decorations first, if not fullscreen full
    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_MAIN) {

        const bool TRANSFORMERSPRESENT = !pWindow->m_vTransformers.empty();

        if (TRANSFORMERSPRESENT) {
            g_pHyprOpenGL->bindOffMain();

            for (auto const& t : pWindow->m_vTransformers) {
                t->preWindowRender(&renderdata);
            }
        }

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->m_dWindowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_BOTTOM)
                    continue;

                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha);
            }

            for (auto const& wd : pWindow->m_dWindowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_UNDER)
                    continue;

                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha);
            }
        }

        static auto PXWLUSENN = CConfigValue<Hyprlang::INT>("xwayland:use_nearest_neighbor");
        if ((pWindow->m_bIsX11 && *PXWLUSENN) || pWindow->m_sWindowData.nearestNeighbor.valueOrDefault())
            g_pHyprOpenGL->m_RenderData.useNearestNeighbor = true;

        if (!pWindow->m_sWindowData.noBlur.valueOrDefault() && pWindow->m_pWLSurface->small() && !pWindow->m_pWLSurface->m_bFillIgnoreSmall && renderdata.blur && *PBLUR) {
            CBox wb = {renderdata.x - pMonitor->vecPosition.x, renderdata.y - pMonitor->vecPosition.y, renderdata.w, renderdata.h};
            wb.scale(pMonitor->scale).round();
            g_pHyprOpenGL->renderRectWithBlur(&wb, CColor(0, 0, 0, 0), renderdata.dontRound ? 0 : renderdata.rounding - 1, renderdata.fadeAlpha,
                                              g_pHyprOpenGL->shouldUseNewBlurOptimizations(nullptr, pWindow));
            renderdata.blur = false;
        }

        renderdata.surfaceCounter = 0;
        pWindow->m_pWLSurface->resource()->breadthfirst([](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) { renderSurface(s, offset.x, offset.y, data); },
                                                        &renderdata);

        g_pHyprOpenGL->m_RenderData.useNearestNeighbor = false;

        if (renderdata.decorate) {
            for (auto const& wd : pWindow->m_dWindowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVER)
                    continue;

                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha);
            }
        }

        if (TRANSFORMERSPRESENT) {

            CFramebuffer* last = g_pHyprOpenGL->m_RenderData.currentFB;
            for (auto const& t : pWindow->m_vTransformers) {
                last = t->transform(last);
            }

            g_pHyprOpenGL->bindBackOnMain();
            g_pHyprOpenGL->renderOffToMain(last);
        }
    }

    g_pHyprOpenGL->m_RenderData.clipBox = CBox();

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_POPUP) {
        if (!pWindow->m_bIsX11) {
            CBox geom = pWindow->m_pXDGSurface->current.geometry;

            renderdata.x -= geom.x;
            renderdata.y -= geom.y;

            renderdata.dontRound       = true; // don't round popups
            renderdata.pMonitor        = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            renderdata.popup           = true;

            static CConfigValue PBLURPOPUPS  = CConfigValue<Hyprlang::INT>("decoration:blur:popups");
            static CConfigValue PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:popups_ignorealpha");

            renderdata.blur = *PBLURPOPUPS && *PBLUR;

            const auto DM = g_pHyprOpenGL->m_RenderData.discardMode;
            const auto DA = g_pHyprOpenGL->m_RenderData.discardOpacity;

            if (renderdata.blur) {
                g_pHyprOpenGL->m_RenderData.discardMode |= DISCARD_ALPHA;
                g_pHyprOpenGL->m_RenderData.discardOpacity = *PBLURIGNOREA;
            }

            if (pWindow->m_sWindowData.nearestNeighbor.valueOrDefault())
                g_pHyprOpenGL->m_RenderData.useNearestNeighbor = true;

            renderdata.surfaceCounter = 0;

            pWindow->m_pPopupHead->breadthfirst(
                [](CPopup* popup, void* data) {
                    if (!popup->m_pWLSurface || !popup->m_pWLSurface->resource() || !popup->m_bMapped)
                        return;
                    const auto     pos    = popup->coordsRelativeToParent();
                    auto           rd     = (SRenderData*)data;
                    const Vector2D oldPos = {rd->x, rd->y};
                    rd->x += pos.x;
                    rd->y += pos.y;

                    popup->m_pWLSurface->resource()->breadthfirst([](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) { renderSurface(s, offset.x, offset.y, data); },
                                                                  data);

                    rd->x = oldPos.x;
                    rd->y = oldPos.y;
                },
                &renderdata);

            g_pHyprOpenGL->m_RenderData.useNearestNeighbor = false;

            g_pHyprOpenGL->m_RenderData.discardMode    = DM;
            g_pHyprOpenGL->m_RenderData.discardOpacity = DA;
        }

        if (decorate) {
            for (auto const& wd : pWindow->m_dWindowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVERLAY)
                    continue;

                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha);
            }
        }
    }

    EMIT_HOOK_EVENT("render", RENDER_POST_WINDOW);

    g_pHyprOpenGL->m_pCurrentWindow.reset();
    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
}

void CHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, timespec* time, bool popups) {
    if (!pLayer)
        return;

    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    if (*PDIMAROUND && pLayer->dimAround && !m_bRenderingSnapshot && !popups) {
        CBox monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};
        g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMAROUND * pLayer->alpha.value()));
    }

    if (pLayer->fadingOut) {
        if (!popups)
            g_pHyprOpenGL->renderSnapshot(pLayer);
        return;
    }

    static auto PBLUR = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    TRACY_GPU_ZONE("RenderLayer");

    const auto  REALPOS = pLayer->realPosition.value();
    const auto  REALSIZ = pLayer->realSize.value();

    SRenderData renderdata           = {pMonitor, time, REALPOS.x, REALPOS.y};
    renderdata.fadeAlpha             = pLayer->alpha.value();
    renderdata.blur                  = pLayer->forceBlur && *PBLUR;
    renderdata.surface               = pLayer->surface->resource();
    renderdata.decorate              = false;
    renderdata.w                     = REALSIZ.x;
    renderdata.h                     = REALSIZ.y;
    renderdata.blockBlurOptimization = pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox{0, 0, pMonitor->vecSize.x, pMonitor->vecSize.y}.scale(pMonitor->scale);

    g_pHyprOpenGL->m_pCurrentLayer = pLayer;

    const auto DM = g_pHyprOpenGL->m_RenderData.discardMode;
    const auto DA = g_pHyprOpenGL->m_RenderData.discardOpacity;

    if (renderdata.blur && pLayer->ignoreAlpha) {
        g_pHyprOpenGL->m_RenderData.discardMode |= DISCARD_ALPHA;
        g_pHyprOpenGL->m_RenderData.discardOpacity = pLayer->ignoreAlphaValue;
    }

    if (!popups)
        pLayer->surface->resource()->breadthfirst([](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) { renderSurface(s, offset.x, offset.y, data); }, &renderdata);

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    renderdata.popup           = true;
    renderdata.blur            = pLayer->forceBlurPopups;
    renderdata.surfaceCounter  = 0;
    if (popups) {
        pLayer->popupHead->breadthfirst(
            [](CPopup* popup, void* data) {
                if (!popup->m_pWLSurface || !popup->m_pWLSurface->resource() || !popup->m_bMapped)
                    return;

                Vector2D pos = popup->coordsRelativeToParent();
                renderSurface(popup->m_pWLSurface->resource(), pos.x, pos.y, data);
            },
            &renderdata);
    }

    g_pHyprOpenGL->m_pCurrentLayer             = nullptr;
    g_pHyprOpenGL->m_RenderData.clipBox        = {};
    g_pHyprOpenGL->m_RenderData.discardMode    = DM;
    g_pHyprOpenGL->m_RenderData.discardOpacity = DA;
}

void CHyprRenderer::renderIMEPopup(CInputPopup* pPopup, PHLMONITOR pMonitor, timespec* time) {
    const auto  POS = pPopup->globalBox().pos();

    SRenderData renderdata = {pMonitor, time, POS.x, POS.y};

    const auto  SURF = pPopup->getSurface();

    renderdata.surface  = SURF;
    renderdata.decorate = false;
    renderdata.w        = SURF->current.size.x;
    renderdata.h        = SURF->current.size.y;

    static auto PBLUR        = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PBLURIMES    = CConfigValue<Hyprlang::INT>("decoration:blur:input_methods");
    static auto PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:input_methods_ignorealpha");

    // TODO: make push/pop methods for this.
    const auto DM = g_pHyprOpenGL->m_RenderData.discardMode;
    const auto DA = g_pHyprOpenGL->m_RenderData.discardOpacity;

    renderdata.blur = *PBLURIMES && *PBLUR;
    if (renderdata.blur) {
        g_pHyprOpenGL->m_RenderData.discardMode |= DISCARD_ALPHA;
        g_pHyprOpenGL->m_RenderData.discardOpacity = *PBLURIGNOREA;
    }

    SURF->breadthfirst([](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) { renderSurface(s, offset.x, offset.y, data); }, &renderdata);

    g_pHyprOpenGL->m_RenderData.discardMode    = DM;
    g_pHyprOpenGL->m_RenderData.discardOpacity = DA;
}

void CHyprRenderer::renderSessionLockSurface(SSessionLockSurface* pSurface, PHLMONITOR pMonitor, timespec* time) {
    SRenderData renderdata = {pMonitor, time, pMonitor->vecPosition.x, pMonitor->vecPosition.y};

    renderdata.blur     = false;
    renderdata.surface  = pSurface->surface->surface();
    renderdata.decorate = false;
    renderdata.w        = pMonitor->vecSize.x;
    renderdata.h        = pMonitor->vecSize.y;

    renderdata.surface->breadthfirst([](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) { renderSurface(s, offset.x, offset.y, data); }, &renderdata);
}

void CHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* time, const Vector2D& translate, const float& scale) {
    static auto      PDIMSPECIAL      = CConfigValue<Hyprlang::FLOAT>("decoration:dim_special");
    static auto      PBLURSPECIAL     = CConfigValue<Hyprlang::INT>("decoration:blur:special");
    static auto      PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto      PRENDERTEX       = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto      PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");

    SRenderModifData RENDERMODIFDATA;
    if (translate != Vector2D{0, 0})
        RENDERMODIFDATA.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate});
    if (scale != 1.f)
        RENDERMODIFDATA.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale});

    if (!pMonitor)
        return;

    if (g_pSessionLockManager->isSessionLocked() && !g_pSessionLockManager->isSessionLockPresent()) {
        // locked with no exclusive, draw only red
        renderSessionLockMissing(pMonitor);
        return;
    }

    // todo: matrices are buggy atm for some reason, but probably would be preferable in the long run
    // g_pHyprOpenGL->saveMatrix();
    // g_pHyprOpenGL->setMatrixScaleTranslate(translate, scale);
    g_pHyprOpenGL->m_RenderData.renderModif = RENDERMODIFDATA;

    if (!pWorkspace) {
        // allow rendering without a workspace. In this case, just render layers.
        g_pHyprOpenGL->blend(false);
        if (!canSkipBackBufferClear(pMonitor)) {
            if (*PRENDERTEX /* inverted cfg flag */)
                g_pHyprOpenGL->clear(CColor(*PBACKGROUNDCOLOR));
            else
                g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"
        }
        g_pHyprOpenGL->blend(true);

        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        g_pHyprOpenGL->m_RenderData.renderModif = {};

        return;
    }

    // for storing damage when we optimize for occlusion
    CRegion preOccludedDamage{g_pHyprOpenGL->m_RenderData.damage};

    // Render layer surfaces below windows for monitor
    // if we have a fullscreen, opaque window that convers the screen, we can skip this.
    // TODO: check better with solitary after MR for tearing.
    const auto PFULLWINDOW = pWorkspace ? pWorkspace->getFullscreenWindow() : nullptr;
    if (!pWorkspace->m_bHasFullscreenWindow || pWorkspace->m_efFullscreenMode != FSMODE_FULLSCREEN || !PFULLWINDOW || PFULLWINDOW->m_vRealSize.isBeingAnimated() ||
        !PFULLWINDOW->opaque() || pWorkspace->m_vRenderOffset.value() != Vector2D{} || g_pHyprOpenGL->preBlurQueued()) {

        if (!g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender)
            setOccludedForBackLayers(g_pHyprOpenGL->m_RenderData.damage, pWorkspace);

        g_pHyprOpenGL->blend(false);
        if (!canSkipBackBufferClear(pMonitor)) {
            if (*PRENDERTEX /* inverted cfg flag */)
                g_pHyprOpenGL->clear(CColor(*PBACKGROUNDCOLOR));
            else
                g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"
        }
        g_pHyprOpenGL->blend(true);

        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(ls.lock(), pMonitor, time);
        }

        g_pHyprOpenGL->m_RenderData.damage = preOccludedDamage;
    }

    // pre window pass
    g_pHyprOpenGL->preWindowPass();

    setOccludedForMainWorkspace(g_pHyprOpenGL->m_RenderData.damage, pWorkspace);

    if (pWorkspace->m_bHasFullscreenWindow)
        renderWorkspaceWindowsFullscreen(pMonitor, pWorkspace, time);
    else
        renderWorkspaceWindows(pMonitor, pWorkspace, time);

    g_pHyprOpenGL->m_RenderData.damage = preOccludedDamage;

    // and then special
    for (auto const& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_pMonitor == pMonitor && ws->m_fAlpha.value() > 0.f && ws->m_bIsSpecialWorkspace) {
            const auto SPECIALANIMPROGRS = ws->m_vRenderOffset.isBeingAnimated() ? ws->m_vRenderOffset.getCurveValue() : ws->m_fAlpha.getCurveValue();
            const bool ANIMOUT           = !pMonitor->activeSpecialWorkspace;

            if (*PDIMSPECIAL != 0.f) {
                CBox monbox = {translate.x, translate.y, pMonitor->vecTransformedSize.x * scale, pMonitor->vecTransformedSize.y * scale};
                g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS)));
            }

            if (*PBLURSPECIAL && *PBLUR) {
                CBox monbox = {translate.x, translate.y, pMonitor->vecTransformedSize.x * scale, pMonitor->vecTransformedSize.y * scale};
                g_pHyprOpenGL->renderRectWithBlur(&monbox, CColor(0, 0, 0, 0), 0, (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS));
            }

            break;
        }
    }

    // special
    for (auto const& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_fAlpha.value() > 0.f && ws->m_bIsSpecialWorkspace) {
            if (ws->m_bHasFullscreenWindow)
                renderWorkspaceWindowsFullscreen(pMonitor, ws, time);
            else
                renderWorkspaceWindows(pMonitor, ws, time);
        }
    }

    // pinned always above
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (!w->m_bPinned || !w->m_bIsFloating)
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        // render the bad boy
        renderWindow(w, pMonitor, time, true, RENDER_PASS_ALL);
    }

    EMIT_HOOK_EVENT("render", RENDER_POST_WINDOWS);

    // Render surfaces above windows for monitor
    for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        renderLayer(ls.lock(), pMonitor, time);
    }

    // Render IME popups
    for (auto const& imep : g_pInputManager->m_sIMERelay.m_vIMEPopups) {
        renderIMEPopup(imep.get(), pMonitor, time);
    }

    for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls.lock(), pMonitor, time);
    }

    for (auto const& lsl : pMonitor->m_aLayerSurfaceLayers) {
        for (auto const& ls : lsl) {
            renderLayer(ls.lock(), pMonitor, time, true);
        }
    }

    renderDragIcon(pMonitor, time);

    //g_pHyprOpenGL->restoreMatrix();
    g_pHyprOpenGL->m_RenderData.renderModif = {};
}

void CHyprRenderer::renderLockscreen(PHLMONITOR pMonitor, timespec* now, const CBox& geometry) {
    TRACY_GPU_ZONE("RenderLockscreen");

    if (g_pSessionLockManager->isSessionLocked()) {
        Vector2D   translate = {geometry.x, geometry.y};

        const auto PSLS = g_pSessionLockManager->getSessionLockSurfaceForMonitor(pMonitor->ID);
        if (!PSLS) {
            if (g_pSessionLockManager->shallConsiderLockMissing())
                renderSessionLockMissing(pMonitor);
        } else {
            renderSessionLockSurface(PSLS, pMonitor, now);
            g_pSessionLockManager->onLockscreenRenderedOnMonitor(pMonitor->ID);
        }
    }
}

void CHyprRenderer::renderSessionLockMissing(PHLMONITOR pMonitor) {
    const auto ALPHA = g_pSessionLockManager->getRedScreenAlphaForMonitor(pMonitor->ID);

    CBox       monbox = {{}, pMonitor->vecPixelSize};

    const bool ANY_PRESENT = g_pSessionLockManager->anySessionLockSurfacesPresent();

    if (ANY_PRESENT) {
        // render image2, without instructions. Lock still "alive", unless texture dead
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockDead2Texture, &monbox, ALPHA);
    } else {
        // render image, with instructions. Lock is gone.
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockDeadTexture, &monbox, ALPHA);

        // also render text for the tty number
        if (g_pHyprOpenGL->m_pLockTtyTextTexture) {
            CBox texbox = {{}, g_pHyprOpenGL->m_pLockTtyTextTexture->m_vSize};
            g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockTtyTextTexture, &texbox, 1.F);
        }
    }

    if (ALPHA < 1.f) /* animate */
        damageMonitor(pMonitor);
    else
        g_pSessionLockManager->onLockscreenRenderedOnMonitor(pMonitor->ID);
}

void CHyprRenderer::calculateUVForSurface(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main, const Vector2D& projSize,
                                          const Vector2D& projSizeUnscaled, bool fixMisalignedFSV1) {
    if (!pWindow || !pWindow->m_bIsX11) {
        static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");

        Vector2D    uvTL;
        Vector2D    uvBR = Vector2D(1, 1);

        if (pSurface->current.viewport.hasSource) {
            // we stretch it to dest. if no dest, to 1,1
            Vector2D const& bufferSize   = pSurface->current.bufferSize;
            auto const&     bufferSource = pSurface->current.viewport.source;

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
            const Vector2D PIXELASUV    = Vector2D{1, 1} / pSurface->current.bufferSize;
            const Vector2D MISALIGNMENT = pSurface->current.bufferSize - projSize;
            if (MISALIGNMENT != Vector2D{})
                uvBR -= MISALIGNMENT * PIXELASUV;
        }

        // if the surface is smaller than our viewport, extend its edges.
        // this will break if later on xdg geometry is hit, but we really try
        // to let the apps know to NOT add CSD. Also if source is there.
        // there is no way to fix this if that's the case
        if (*PEXPANDEDGES) {
            const auto MONITOR_WL_SCALE = std::ceil(pMonitor->scale);
            const bool SCALE_UNAWARE    = MONITOR_WL_SCALE != pSurface->current.scale && !pSurface->current.viewport.hasDestination;
            const auto EXPECTED_SIZE =
                ((pSurface->current.viewport.hasDestination ? pSurface->current.viewport.destination : pSurface->current.bufferSize / pSurface->current.scale) * pMonitor->scale)
                    .round();
            if (!SCALE_UNAWARE && (EXPECTED_SIZE.x < projSize.x || EXPECTED_SIZE.y < projSize.y)) {
                // this will not work with shm AFAIK, idk why.
                // NOTE: this math is wrong if we have a source... or geom updates later, but I don't think we can do much
                const auto FIX = projSize / EXPECTED_SIZE;
                uvBR           = uvBR * FIX;
            }
        }

        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        if (!main || !pWindow)
            return;

        CBox geom = pWindow->m_pXDGSurface->current.geometry;

        // ignore X and Y, adjust uv
        if (geom.x != 0 || geom.y != 0 || geom.width > projSizeUnscaled.x || geom.height > projSizeUnscaled.y) {
            const auto XPERC = (double)geom.x / (double)pSurface->current.size.x;
            const auto YPERC = (double)geom.y / (double)pSurface->current.size.y;
            const auto WPERC = (double)(geom.x + geom.width) / (double)pSurface->current.size.x;
            const auto HPERC = (double)(geom.y + geom.height) / (double)pSurface->current.size.y;

            const auto TOADDTL = Vector2D(XPERC * (uvBR.x - uvTL.x), YPERC * (uvBR.y - uvTL.y));
            uvBR               = uvBR - Vector2D((1.0 - WPERC) * (uvBR.x - uvTL.x), (1.0 - HPERC) * (uvBR.y - uvTL.y));
            uvTL               = uvTL + TOADDTL;

            auto maxSize = projSizeUnscaled;

            if (pWindow->m_pWLSurface->small() && !pWindow->m_pWLSurface->m_bFillIgnoreSmall)
                maxSize = pWindow->m_pWLSurface->getViewporterCorrectedSize();

            if (geom.width > maxSize.x)
                uvBR.x = uvBR.x * (maxSize.x / geom.width);
            if (geom.height > maxSize.y)
                uvBR.y = uvBR.y * (maxSize.y / geom.height);
        }

        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

void CHyprRenderer::renderMonitor(PHLMONITOR pMonitor) {
    static std::chrono::high_resolution_clock::time_point renderStart        = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point renderStartOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay   = std::chrono::high_resolution_clock::now();

    static auto                                           PDEBUGOVERLAY       = CConfigValue<Hyprlang::INT>("debug:overlay");
    static auto                                           PDAMAGETRACKINGMODE = CConfigValue<Hyprlang::INT>("debug:damage_tracking");
    static auto                                           PDAMAGEBLINK        = CConfigValue<Hyprlang::INT>("debug:damage_blink");
    static auto                                           PDIRECTSCANOUT      = CConfigValue<Hyprlang::INT>("render:direct_scanout");
    static auto                                           PVFR                = CConfigValue<Hyprlang::INT>("misc:vfr");
    static auto                                           PZOOMFACTOR         = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    static auto                                           PANIMENABLED        = CConfigValue<Hyprlang::INT>("animations:enabled");
    static auto                                           PFIRSTLAUNCHANIM    = CConfigValue<Hyprlang::INT>("animations:first_launch_animation");
    static auto                                           PTEARINGENABLED     = CConfigValue<Hyprlang::INT>("general:allow_tearing");

    static int                                            damageBlinkCleanup = 0; // because double-buffered

    if (!*PDAMAGEBLINK)
        damageBlinkCleanup = 0;

    static bool firstLaunch           = true;
    static bool firstLaunchAnimActive = *PFIRSTLAUNCHANIM;

    float       zoomInFactorFirstLaunch = 1.f;

    if (firstLaunch) {
        firstLaunch = false;
        m_tRenderTimer.reset();
    }

    if (m_tRenderTimer.getSeconds() < 1.5f && firstLaunchAnimActive) { // TODO: make the animation system more damage-flexible so that this can be migrated to there
        if (!*PANIMENABLED) {
            zoomInFactorFirstLaunch = 1.f;
            firstLaunchAnimActive   = false;
        } else {
            zoomInFactorFirstLaunch = 2.f - g_pAnimationManager->getBezier("default")->getYForPoint(m_tRenderTimer.getSeconds() / 1.5);
            damageMonitor(pMonitor);
        }
    } else {
        firstLaunchAnimActive = false;
    }

    renderStart = std::chrono::high_resolution_clock::now();

    if (*PDEBUGOVERLAY == 1)
        g_pDebugOverlay->frameData(pMonitor);

    if (pMonitor->framesToSkip > 0) {
        pMonitor->framesToSkip -= 1;

        if (!pMonitor->noFrameSchedule)
            g_pCompositor->scheduleFrameForMonitor(pMonitor, Aquamarine::IOutput::AQ_SCHEDULE_RENDER_MONITOR);
        else
            Debug::log(LOG, "NoFrameSchedule hit for {}.", pMonitor->szName);

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);

        if (pMonitor->framesToSkip > 10)
            pMonitor->framesToSkip = 0;
        return;
    }

    // checks //
    if (pMonitor->ID == m_pMostHzMonitor->ID ||
        *PVFR == 1) { // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that
        g_pCompositor->sanityCheckWorkspaces();

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();
    }
    //       //

    if (pMonitor->scheduledRecalc) {
        pMonitor->scheduledRecalc = false;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);
    }

    if (!pMonitor->output->needsFrame && pMonitor->forceFullFrames == 0)
        return;

    // tearing and DS first
    bool shouldTear = false;
    if (pMonitor->tearingState.nextRenderTorn) {
        pMonitor->tearingState.nextRenderTorn = false;

        if (!*PTEARINGENABLED) {
            Debug::log(WARN, "Tearing commit requested but the master switch general:allow_tearing is off, ignoring");
            return;
        }

        if (g_pHyprOpenGL->m_RenderData.mouseZoomFactor != 1.0) {
            Debug::log(WARN, "Tearing commit requested but scale factor is not 1, ignoring");
            return;
        }

        if (!pMonitor->tearingState.canTear) {
            Debug::log(WARN, "Tearing commit requested but monitor doesn't support it, ignoring");
            return;
        }

        if (!pMonitor->solitaryClient.expired())
            shouldTear = true;
    }

    pMonitor->tearingState.activelyTearing = shouldTear;

    if (*PDIRECTSCANOUT && !shouldTear) {
        if (pMonitor->attemptDirectScanout()) {
            return;
        } else if (!pMonitor->lastScanout.expired()) {
            Debug::log(LOG, "Left a direct scanout.");
            pMonitor->lastScanout.reset();

            // reset DRM format, make sure it's the one we want.
            pMonitor->output->state->setFormat(pMonitor->prevDrmFormat);
            pMonitor->drmFormat = pMonitor->prevDrmFormat;
        }
    }

    EMIT_HOOK_EVENT("preRender", pMonitor);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // check the damage
    bool hasChanged = pMonitor->output->needsFrame || pMonitor->damage.hasChanged();

    if (!hasChanged && *PDAMAGETRACKINGMODE != DAMAGE_TRACKING_NONE && pMonitor->forceFullFrames == 0 && damageBlinkCleanup == 0)
        return;

    if (*PDAMAGETRACKINGMODE == -1) {
        Debug::log(CRIT, "Damage tracking mode -1 ????");
        return;
    }

    EMIT_HOOK_EVENT("render", RENDER_PRE);

    pMonitor->renderingActive = true;

    // we need to cleanup fading out when rendering the appropriate context
    g_pCompositor->cleanupFadingOut(pMonitor->ID);

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    TRACY_GPU_ZONE("Render");

    static bool zoomLock = false;
    if (zoomLock && *PZOOMFACTOR == 1.f) {
        g_pPointerManager->unlockSoftwareAll();
        zoomLock = false;
    } else if (!zoomLock && *PZOOMFACTOR != 1.f) {
        g_pPointerManager->lockSoftwareAll();
        zoomLock = true;
    }

    if (pMonitor == g_pCompositor->getMonitorFromCursor())
        g_pHyprOpenGL->m_RenderData.mouseZoomFactor = std::clamp(*PZOOMFACTOR, 1.f, INFINITY);
    else
        g_pHyprOpenGL->m_RenderData.mouseZoomFactor = 1.f;

    if (zoomInFactorFirstLaunch > 1.f) {
        g_pHyprOpenGL->m_RenderData.mouseZoomFactor    = zoomInFactorFirstLaunch;
        g_pHyprOpenGL->m_RenderData.mouseZoomUseMouse  = false;
        g_pHyprOpenGL->m_RenderData.useNearestNeighbor = false;
        pMonitor->forceFullFrames                      = 10;
    }

    CRegion damage, finalDamage;
    if (!beginRender(pMonitor, damage, RENDER_MODE_NORMAL)) {
        Debug::log(ERR, "renderer: couldn't beginRender()!");
        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->forceFullFrames > 0 || damageBlinkCleanup > 0) {
        damage      = {0, 0, (int)pMonitor->vecTransformedSize.x * 10, (int)pMonitor->vecTransformedSize.y * 10};
        finalDamage = damage;
    } else {
        static auto PBLURENABLED = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

        // if we use blur we need to expand the damage for proper blurring
        // if framebuffer was not offloaded we're not doing introspection aka not blurring so this is redundant and dumb
        if (*PBLURENABLED == 1 && g_pHyprOpenGL->m_bOffloadedFramebuffer) {
            // TODO: can this be optimized?
            static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
            static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
            const auto  BLURRADIUS =
                *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES); // is this 2^pass? I don't know but it works... I think.

            // now, prep the damage, get the extended damage region
            damage.expand(BLURRADIUS); // expand for proper blurring

            finalDamage = damage;

            damage.expand(BLURRADIUS); // expand for proper blurring
        } else
            finalDamage = damage;
    }

    // update damage in renderdata as we modified it
    g_pHyprOpenGL->setDamage(damage, finalDamage);

    if (pMonitor->forceFullFrames > 0) {
        pMonitor->forceFullFrames -= 1;
        if (pMonitor->forceFullFrames > 10)
            pMonitor->forceFullFrames = 0;
    }

    EMIT_HOOK_EVENT("render", RENDER_BEGIN);

    bool renderCursor = true;

    if (!finalDamage.empty()) {
        if (pMonitor->solitaryClient.expired()) {
            if (pMonitor->isMirror()) {
                g_pHyprOpenGL->blend(false);
                g_pHyprOpenGL->renderMirrored();
                g_pHyprOpenGL->blend(true);
                EMIT_HOOK_EVENT("render", RENDER_POST_MIRROR);
                renderCursor = false;
            } else {
                CBox renderBox = {0, 0, (int)pMonitor->vecPixelSize.x, (int)pMonitor->vecPixelSize.y};
                renderWorkspace(pMonitor, pMonitor->activeWorkspace, &now, renderBox);

                renderLockscreen(pMonitor, &now, renderBox);

                if (pMonitor == g_pCompositor->m_pLastMonitor) {
                    g_pHyprNotificationOverlay->draw(pMonitor);
                    g_pHyprError->draw();
                }

                // for drawing the debug overlay
                if (pMonitor == g_pCompositor->m_vMonitors.front() && *PDEBUGOVERLAY == 1) {
                    renderStartOverlay = std::chrono::high_resolution_clock::now();
                    g_pDebugOverlay->draw();
                    endRenderOverlay = std::chrono::high_resolution_clock::now();
                }

                if (*PDAMAGEBLINK && damageBlinkCleanup == 0) {
                    CBox monrect = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
                    g_pHyprOpenGL->renderRect(&monrect, CColor(1.0, 0.0, 1.0, 100.0 / 255.0), 0);
                    damageBlinkCleanup = 1;
                } else if (*PDAMAGEBLINK) {
                    damageBlinkCleanup++;
                    if (damageBlinkCleanup > 3)
                        damageBlinkCleanup = 0;
                }
            }
        } else
            g_pHyprRenderer->renderWindow(pMonitor->solitaryClient.lock(), pMonitor, &now, false, RENDER_PASS_MAIN /* solitary = no popups */);
    } else if (!pMonitor->isMirror()) {
        sendFrameEventsToWorkspace(pMonitor, pMonitor->activeWorkspace, &now);
        if (pMonitor->activeSpecialWorkspace)
            sendFrameEventsToWorkspace(pMonitor, pMonitor->activeSpecialWorkspace, &now);
    }

    renderCursor = renderCursor && shouldRenderCursor();

    if (renderCursor) {
        TRACY_GPU_ZONE("RenderCursor");
        g_pPointerManager->renderSoftwareCursorsFor(pMonitor->self.lock(), &now, g_pHyprOpenGL->m_RenderData.damage);
    }

    EMIT_HOOK_EVENT("render", RENDER_LAST_MOMENT);

    endRender();

    TRACY_GPU_COLLECT;

    if (!pMonitor->mirrors.empty()) {
        CRegion    frameDamage{finalDamage};

        const auto TRANSFORM = invertTransform(pMonitor->transform);
        frameDamage.transform(wlTransformToHyprutils(TRANSFORM), pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y);

        if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
            frameDamage.add(0, 0, (int)pMonitor->vecTransformedSize.x, (int)pMonitor->vecTransformedSize.y);

        if (*PDAMAGEBLINK)
            frameDamage.add(damage);

        g_pHyprRenderer->damageMirrorsWith(pMonitor, frameDamage);

        pMonitor->output->state->addDamage(frameDamage);
    }

    pMonitor->renderingActive = false;

    EMIT_HOOK_EVENT("render", RENDER_POST);

    pMonitor->output->state->setPresentationMode(shouldTear ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE :
                                                              Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC);

    commitPendingAndDoExplicitSync(pMonitor);

    if (shouldTear)
        pMonitor->tearingState.busy = true;

    if (*PDAMAGEBLINK || *PVFR == 0 || pMonitor->pendingFrame)
        g_pCompositor->scheduleFrameForMonitor(pMonitor, Aquamarine::IOutput::AQ_SCHEDULE_RENDER_MONITOR);

    pMonitor->pendingFrame = false;

    const float durationUs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000.f;
    g_pDebugOverlay->renderData(pMonitor, durationUs);

    if (*PDEBUGOVERLAY == 1) {
        if (pMonitor == g_pCompositor->m_vMonitors.front()) {
            const float noOverlayUs = durationUs - std::chrono::duration_cast<std::chrono::nanoseconds>(endRenderOverlay - renderStartOverlay).count() / 1000.f;
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, noOverlayUs);
        } else {
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, durationUs);
        }
    }
}

bool CHyprRenderer::commitPendingAndDoExplicitSync(PHLMONITOR pMonitor) {
    // apply timelines for explicit sync
    // save inFD otherwise reset will reset it
    auto inFD = pMonitor->output->state->state().explicitInFence;
    pMonitor->output->state->resetExplicitFences();
    if (inFD >= 0)
        pMonitor->output->state->setExplicitInFence(inFD);
    auto explicitOptions = getExplicitSyncSettings();
    if (explicitOptions.explicitEnabled && explicitOptions.explicitKMSEnabled)
        pMonitor->output->state->enableExplicitOutFenceForNextCommit();

    if (pMonitor->ctmUpdated) {
        pMonitor->ctmUpdated = false;
        pMonitor->output->state->setCTM(pMonitor->ctm);
    }

    bool ok = pMonitor->state.commit();
    if (!ok) {
        if (inFD >= 0) {
            Debug::log(TRACE, "Monitor state commit failed, retrying without a fence");
            pMonitor->output->state->resetExplicitFences();
            ok = pMonitor->state.commit();
        }

        if (!ok) {
            Debug::log(TRACE, "Monitor state commit failed");
            // rollback the buffer to avoid writing to the front buffer that is being
            // displayed
            pMonitor->output->swapchain->rollback();
            pMonitor->damage.damageEntire();
        }
    }

    if (!explicitOptions.explicitEnabled)
        return ok;

    if (inFD >= 0)
        close(inFD);

    if (pMonitor->output->state->state().explicitOutFence >= 0) {
        Debug::log(TRACE, "Aquamarine returned an explicit out fence at {}", pMonitor->output->state->state().explicitOutFence);
        close(pMonitor->output->state->state().explicitOutFence);
    } else
        Debug::log(TRACE, "Aquamarine did not return an explicit out fence");

    Debug::log(TRACE, "Explicit: {} presented", explicitPresented.size());
    auto sync = g_pHyprOpenGL->createEGLSync(-1);

    if (!sync)
        Debug::log(TRACE, "Explicit: can't add sync, EGLSync failed");
    else {
        for (auto const& e : explicitPresented) {
            if (!e->current.buffer || !e->current.buffer->releaser)
                continue;

            e->current.buffer->releaser->addReleaseSync(sync);
        }
    }

    explicitPresented.clear();

    pMonitor->output->state->resetExplicitFences();

    return ok;
}

void CHyprRenderer::renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* now, const CBox& geometry) {
    Vector2D translate = {geometry.x, geometry.y};
    float    scale     = (float)geometry.width / pMonitor->vecPixelSize.x;

    TRACY_GPU_ZONE("RenderWorkspace");

    if (!DELTALESSTHAN((double)geometry.width / (double)geometry.height, pMonitor->vecPixelSize.x / pMonitor->vecPixelSize.y, 0.01)) {
        Debug::log(ERR, "Ignoring geometry in renderWorkspace: aspect ratio mismatch");
        scale     = 1.f;
        translate = Vector2D{};
    }

    g_pHyprOpenGL->m_RenderData.pWorkspace = pWorkspace;
    renderAllClientsForWorkspace(pMonitor, pWorkspace, now, translate, scale);
    g_pHyprOpenGL->m_RenderData.pWorkspace = nullptr;
}

void CHyprRenderer::sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* now) {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped || w->m_bFadingOut || !w->m_pWLSurface->resource())
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        w->m_pWLSurface->resource()->breadthfirst([now](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { r->frame(now); }, nullptr);
    }

    for (auto const& lsl : pMonitor->m_aLayerSurfaceLayers) {
        for (auto const& ls : lsl) {
            if (ls->fadingOut || !ls->surface->resource())
                continue;

            ls->surface->resource()->breadthfirst([now](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { r->frame(now); }, nullptr);
        }
    }
}

void CHyprRenderer::setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor) {
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
            .positive_axis   = NULL,
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
            .positive_axis   = NULL,
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

void CHyprRenderer::arrangeLayerArray(PHLMONITOR pMonitor, const std::vector<PHLLSREF>& layerSurfaces, bool exclusiveZone, CBox* usableArea) {
    CBox full_area = {pMonitor->vecPosition.x, pMonitor->vecPosition.y, pMonitor->vecSize.x, pMonitor->vecSize.y};

    for (auto const& ls : layerSurfaces) {
        if (!ls || ls->fadingOut || ls->readyToDelete || !ls->layerSurface || ls->noProcess)
            continue;

        const auto PLAYER = ls->layerSurface;
        const auto PSTATE = &PLAYER->current;
        if (exclusiveZone != (PSTATE->exclusive > 0))
            continue;

        CBox bounds;
        if (PSTATE->exclusive == -1)
            bounds = full_area;
        else
            bounds = *usableArea;

        const Vector2D OLDSIZE = {ls->geometry.width, ls->geometry.height};

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
            Debug::log(ERR, "LayerSurface {:x} has a negative/zero w/h???", (uintptr_t)ls.get());
            continue;
        }

        box.round(); // fix rounding errors

        ls->geometry = box;

        applyExclusive(*usableArea, PSTATE->anchor, PSTATE->exclusive, PSTATE->exclusiveEdge, PSTATE->margin.top, PSTATE->margin.right, PSTATE->margin.bottom, PSTATE->margin.left);

        if (Vector2D{box.width, box.height} != OLDSIZE)
            ls->layerSurface->configure(box.size());

        ls->realPosition = box.pos();
        ls->realSize     = box.size();
    }
}

void CHyprRenderer::arrangeLayersForMonitor(const MONITORID& monitor) {
    const auto  PMONITOR = g_pCompositor->getMonitorFromID(monitor);

    static auto BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");

    if (!PMONITOR)
        return;

    // Reset the reserved
    PMONITOR->vecReservedBottomRight = Vector2D();
    PMONITOR->vecReservedTopLeft     = Vector2D();

    CBox usableArea = {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    if (g_pHyprError->active() && g_pCompositor->m_pLastMonitor == PMONITOR->self) {
        const auto HEIGHT = g_pHyprError->height();
        if (*BAR_POSITION == 0) {
            PMONITOR->vecReservedTopLeft.y = HEIGHT;
            usableArea.y += HEIGHT;
            usableArea.h -= HEIGHT;
        } else {
            PMONITOR->vecReservedBottomRight.y = HEIGHT;
            usableArea.h -= HEIGHT;
        }
    }

    for (auto& la : PMONITOR->m_aLayerSurfaceLayers) {
        std::stable_sort(la.begin(), la.end(), [](const PHLLSREF& a, const PHLLSREF& b) { return a->order > b->order; });
    }

    for (auto const& la : PMONITOR->m_aLayerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, true, &usableArea);

    for (auto const& la : PMONITOR->m_aLayerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, false, &usableArea);

    PMONITOR->vecReservedTopLeft     = Vector2D(usableArea.x, usableArea.y) - PMONITOR->vecPosition;
    PMONITOR->vecReservedBottomRight = PMONITOR->vecSize - Vector2D(usableArea.width, usableArea.height) - PMONITOR->vecReservedTopLeft;

    auto ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(PMONITOR->szName);
    if (ADDITIONALRESERVED == g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(""); // glob wildcard
    }

    if (ADDITIONALRESERVED != g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        PMONITOR->vecReservedTopLeft     = PMONITOR->vecReservedTopLeft + Vector2D(ADDITIONALRESERVED->second.left, ADDITIONALRESERVED->second.top);
        PMONITOR->vecReservedBottomRight = PMONITOR->vecReservedBottomRight + Vector2D(ADDITIONALRESERVED->second.right, ADDITIONALRESERVED->second.bottom);
    }

    // damage the monitor if can
    damageMonitor(PMONITOR);

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitor);
}

void CHyprRenderer::damageSurface(SP<CWLSurfaceResource> pSurface, double x, double y, double scale) {
    if (!pSurface)
        return; // wut?

    if (g_pCompositor->m_bUnsafeState)
        return;

    const auto WLSURF    = CWLSurface::fromResource(pSurface);
    CRegion    damageBox = WLSURF ? WLSURF->computeDamage() : CRegion{};
    if (!WLSURF) {
        Debug::log(ERR, "BUG THIS: No CWLSurface for surface in damageSurface!!!");
        return;
    }

    if (scale != 1.0)
        damageBox.scale(scale);

    // schedule frame events
    g_pCompositor->scheduleFrameForMonitor(g_pCompositor->getMonitorFromVector(Vector2D(x, y)), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);

    if (damageBox.empty())
        return;

    damageBox.translate({x, y});

    CRegion damageBoxForEach;

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (!m->output)
            continue;

        damageBoxForEach.set(damageBox);
        damageBoxForEach.translate({-m->vecPosition.x, -m->vecPosition.y}).scale(m->scale);

        m->addDamage(&damageBoxForEach);
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Surface (extents): xy: {}, {} wh: {}, {}", damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y1,
                   damageBox.pixman()->extents.x2 - damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y2 - damageBox.pixman()->extents.y1);
}

void CHyprRenderer::damageWindow(PHLWINDOW pWindow, bool forceFull) {
    if (g_pCompositor->m_bUnsafeState)
        return;

    CBox       windowBox        = pWindow->getFullWindowBoundingBox();
    const auto PWINDOWWORKSPACE = pWindow->m_pWorkspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated() && !pWindow->m_bPinned)
        windowBox.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
    windowBox.translate(pWindow->m_vFloatingOffset);

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (forceFull || g_pHyprRenderer->shouldRenderWindow(pWindow, m)) { // only damage if window is rendered on monitor
            CBox fixedDamageBox = {windowBox.x - m->vecPosition.x, windowBox.y - m->vecPosition.y, windowBox.width, windowBox.height};
            fixedDamageBox.scale(m->scale);
            m->addDamage(&fixedDamageBox);
        }
    }

    for (auto const& wd : pWindow->m_dWindowDecorations)
        wd->damageEntire();

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Window ({}): xy: {}, {} wh: {}, {}", pWindow->m_szTitle, windowBox.x, windowBox.y, windowBox.width, windowBox.height);
}

void CHyprRenderer::damageMonitor(PHLMONITOR pMonitor) {
    if (g_pCompositor->m_bUnsafeState || pMonitor->isMirror())
        return;

    CBox damageBox = {0, 0, INT16_MAX, INT16_MAX};
    pMonitor->addDamage(&damageBox);

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Monitor {}", pMonitor->szName);
}

void CHyprRenderer::damageBox(CBox* pBox, bool skipFrameSchedule) {
    if (g_pCompositor->m_bUnsafeState)
        return;

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (m->isMirror())
            continue; // don't damage mirrors traditionally

        if (!skipFrameSchedule) {
            CBox damageBox = {pBox->x - m->vecPosition.x, pBox->y - m->vecPosition.y, pBox->width, pBox->height};
            damageBox.scale(m->scale);
            m->addDamage(&damageBox);
        }
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Box: xy: {}, {} wh: {}, {}", pBox->x, pBox->y, pBox->width, pBox->height);
}

void CHyprRenderer::damageBox(const int& x, const int& y, const int& w, const int& h) {
    CBox box = {x, y, w, h};
    damageBox(&box);
}

void CHyprRenderer::damageRegion(const CRegion& rg) {
    for (auto const& RECT : rg.getRects()) {
        damageBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1);
    }
}

void CHyprRenderer::damageMirrorsWith(PHLMONITOR pMonitor, const CRegion& pRegion) {
    for (auto const& mirror : pMonitor->mirrors) {

        // transform the damage here, so it won't get clipped by the monitor damage ring
        auto    monitor  = mirror;
        auto    mirrored = pMonitor;

        CRegion transformed{pRegion};

        // we want to transform to the same box as in CHyprOpenGLImpl::renderMirrored
        double scale  = std::min(monitor->vecTransformedSize.x / mirrored->vecTransformedSize.x, monitor->vecTransformedSize.y / mirrored->vecTransformedSize.y);
        CBox   monbox = {0, 0, mirrored->vecTransformedSize.x * scale, mirrored->vecTransformedSize.y * scale};
        monbox.x      = (monitor->vecTransformedSize.x - monbox.w) / 2;
        monbox.y      = (monitor->vecTransformedSize.y - monbox.h) / 2;

        transformed.scale(scale);
        transformed.transform(wlTransformToHyprutils(mirrored->transform), mirrored->vecPixelSize.x * scale, mirrored->vecPixelSize.y * scale);
        transformed.translate(Vector2D(monbox.x, monbox.y));

        mirror->addDamage(&transformed);

        g_pCompositor->scheduleFrameForMonitor(mirror.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }
}

void CHyprRenderer::renderDragIcon(PHLMONITOR pMonitor, timespec* time) {
    PROTO::data->renderDND(pMonitor, time);
}

DAMAGETRACKINGMODES CHyprRenderer::damageTrackingModeFromStr(const std::string& mode) {
    if (mode == "full")
        return DAMAGE_TRACKING_FULL;
    if (mode == "monitor")
        return DAMAGE_TRACKING_MONITOR;
    if (mode == "none")
        return DAMAGE_TRACKING_NONE;

    return DAMAGE_TRACKING_INVALID;
}

bool CHyprRenderer::applyMonitorRule(PHLMONITOR pMonitor, SMonitorRule* pMonitorRule, bool force) {

    static auto PDISABLESCALECHECKS = CConfigValue<Hyprlang::INT>("debug:disable_scale_checks");

    Debug::log(LOG, "Applying monitor rule for {}", pMonitor->szName);

    pMonitor->activeMonitorRule = *pMonitorRule;

    if (pMonitor->forceSize.has_value())
        pMonitor->activeMonitorRule.resolution = pMonitor->forceSize.value();

    const auto RULE = &pMonitor->activeMonitorRule;

    // if it's disabled, disable and ignore
    if (RULE->disabled) {
        if (pMonitor->m_bEnabled)
            pMonitor->onDisconnect();

        pMonitor->events.modeChanged.emit();

        return true;
    }

    // don't touch VR headsets
    if (pMonitor->output->nonDesktop)
        return true;

    if (!pMonitor->m_bEnabled) {
        pMonitor->onConnect(true); // enable it.
        Debug::log(LOG, "Monitor {} is disabled but is requested to be enabled", pMonitor->szName);
        force = true;
    }

    // Check if the rule isn't already applied
    // TODO: clean this up lol
    if (!force && DELTALESSTHAN(pMonitor->vecPixelSize.x, RULE->resolution.x, 1) && DELTALESSTHAN(pMonitor->vecPixelSize.y, RULE->resolution.y, 1) &&
        DELTALESSTHAN(pMonitor->refreshRate, RULE->refreshRate, 1) && pMonitor->setScale == RULE->scale &&
        ((DELTALESSTHAN(pMonitor->vecPosition.x, RULE->offset.x, 1) && DELTALESSTHAN(pMonitor->vecPosition.y, RULE->offset.y, 1)) ||
         RULE->offset == Vector2D(-INT32_MAX, -INT32_MAX)) &&
        pMonitor->transform == RULE->transform && RULE->enable10bit == pMonitor->enabled10bit &&
        !memcmp(&pMonitor->customDrmMode, &RULE->drmMode, sizeof(pMonitor->customDrmMode))) {

        Debug::log(LOG, "Not applying a new rule to {} because it's already applied!", pMonitor->szName);

        pMonitor->setMirror(RULE->mirrorOf);

        return true;
    }

    const auto WAS10B = pMonitor->enabled10bit;
    const auto OLDRES = pMonitor->vecPixelSize;

    // Needed in case we are switching from a custom modeline to a standard mode
    pMonitor->customDrmMode = {};
    pMonitor->currentMode   = nullptr;

    pMonitor->output->state->setFormat(DRM_FORMAT_XRGB8888);
    pMonitor->prevDrmFormat = pMonitor->drmFormat;
    pMonitor->drmFormat     = DRM_FORMAT_XRGB8888;
    pMonitor->output->state->resetExplicitFences();

    bool autoScale = false;

    if (RULE->scale > 0.1) {
        pMonitor->scale = RULE->scale;
    } else {
        autoScale               = true;
        const auto DEFAULTSCALE = pMonitor->getDefaultScale();
        pMonitor->scale         = DEFAULTSCALE;
    }

    pMonitor->setScale  = pMonitor->scale;
    pMonitor->transform = RULE->transform;

    const auto WLRREFRESHRATE = pMonitor->output->getBackend()->type() == Aquamarine::eBackendType::AQ_BACKEND_DRM ? RULE->refreshRate * 1000 : 0;

    // loop over modes and choose an appropriate one.
    if (RULE->resolution != Vector2D() && RULE->resolution != Vector2D(-1, -1) && RULE->resolution != Vector2D(-1, -2)) {
        if (!pMonitor->output->modes.empty() && RULE->drmMode.type != DRM_MODE_TYPE_USERDEF) {
            bool found = false;

            for (auto const& mode : pMonitor->output->modes) {
                // if delta of refresh rate, w and h chosen and mode is < 1 we accept it
                if (DELTALESSTHAN(mode->pixelSize.x, RULE->resolution.x, 1) && DELTALESSTHAN(mode->pixelSize.y, RULE->resolution.y, 1) &&
                    DELTALESSTHAN(mode->refreshRate / 1000.f, RULE->refreshRate, 1)) {
                    pMonitor->output->state->setMode(mode);

                    if (!pMonitor->state.test()) {
                        Debug::log(LOG, "Monitor {}: REJECTED available mode: {}x{}@{:2f}!", pMonitor->output->name, mode->pixelSize.x, mode->pixelSize.y,
                                   mode->refreshRate / 1000.f);
                        continue;
                    }

                    Debug::log(LOG, "Monitor {}: requested {:X0}@{:2f}, found available mode: {}x{}@{}mHz, applying.", pMonitor->output->name, RULE->resolution,
                               (float)RULE->refreshRate, mode->pixelSize.x, mode->pixelSize.y, mode->refreshRate);

                    found = true;

                    pMonitor->refreshRate = mode->refreshRate / 1000.f;
                    pMonitor->vecSize     = mode->pixelSize;
                    pMonitor->currentMode = mode;

                    break;
                }
            }

            if (!found) {
                pMonitor->output->state->setCustomMode(makeShared<Aquamarine::SOutputMode>(Aquamarine::SOutputMode{.pixelSize = RULE->resolution, .refreshRate = WLRREFRESHRATE}));
                pMonitor->vecSize     = RULE->resolution;
                pMonitor->refreshRate = RULE->refreshRate;

                if (!pMonitor->state.test()) {
                    Debug::log(ERR, "Custom resolution FAILED, falling back to preferred");

                    const auto PREFERREDMODE = pMonitor->output->preferredMode();

                    if (!PREFERREDMODE) {
                        Debug::log(ERR, "Monitor {} has NO PREFERRED MODE, and an INVALID one was requested: {:X0}@{:2f}", pMonitor->ID, RULE->resolution,
                                   (float)RULE->refreshRate);
                        return true;
                    }

                    // Preferred is valid
                    pMonitor->output->state->setMode(PREFERREDMODE);

                    Debug::log(ERR, "Monitor {} got an invalid requested mode: {:X0}@{:2f}, using the preferred one instead: {}x{}@{:2f}", pMonitor->output->name, RULE->resolution,
                               (float)RULE->refreshRate, PREFERREDMODE->pixelSize.x, PREFERREDMODE->pixelSize.y, PREFERREDMODE->refreshRate / 1000.f);

                    pMonitor->refreshRate = PREFERREDMODE->refreshRate / 1000.f;
                    pMonitor->vecSize     = PREFERREDMODE->pixelSize;
                    pMonitor->currentMode = PREFERREDMODE;
                } else {
                    Debug::log(LOG, "Set a custom mode {:X0}@{:2f} (mode not found in monitor modes)", RULE->resolution, (float)RULE->refreshRate);
                }
            }
        } else {
            // custom resolution
            bool fail = false;

            if (RULE->drmMode.type == DRM_MODE_TYPE_USERDEF) {
                if (pMonitor->output->getBackend()->type() != Aquamarine::eBackendType::AQ_BACKEND_DRM) {
                    Debug::log(ERR, "Tried to set custom modeline on non-DRM output");
                    fail = true;
                } else
                    pMonitor->output->state->setCustomMode(makeShared<Aquamarine::SOutputMode>(
                        Aquamarine::SOutputMode{.pixelSize = {RULE->drmMode.hdisplay, RULE->drmMode.vdisplay}, .refreshRate = RULE->drmMode.vrefresh, .modeInfo = RULE->drmMode}));
            } else
                pMonitor->output->state->setCustomMode(makeShared<Aquamarine::SOutputMode>(Aquamarine::SOutputMode{.pixelSize = RULE->resolution, .refreshRate = WLRREFRESHRATE}));

            pMonitor->vecSize     = RULE->resolution;
            pMonitor->refreshRate = RULE->refreshRate;

            if (fail || !pMonitor->state.test()) {
                Debug::log(ERR, "Custom resolution FAILED, falling back to preferred");

                const auto PREFERREDMODE = pMonitor->output->preferredMode();

                if (!PREFERREDMODE) {
                    Debug::log(ERR, "Monitor {} has NO PREFERRED MODE, and an INVALID one was requested: {:X0}@{:2f}", pMonitor->output->name, RULE->resolution,
                               (float)RULE->refreshRate);
                    return true;
                }

                // Preferred is valid
                pMonitor->output->state->setMode(PREFERREDMODE);

                Debug::log(ERR, "Monitor {} got an invalid requested mode: {:X0}@{:2f}, using the preferred one instead: {}x{}@{:2f}", pMonitor->output->name, RULE->resolution,
                           (float)RULE->refreshRate, PREFERREDMODE->pixelSize.x, PREFERREDMODE->pixelSize.y, PREFERREDMODE->refreshRate / 1000.f);

                pMonitor->refreshRate   = PREFERREDMODE->refreshRate / 1000.f;
                pMonitor->vecSize       = PREFERREDMODE->pixelSize;
                pMonitor->customDrmMode = {};
            } else
                Debug::log(LOG, "Set a custom mode {:X0}@{:2f} (mode not found in monitor modes)", RULE->resolution, (float)RULE->refreshRate);
        }
    } else if (RULE->resolution != Vector2D()) {
        if (!pMonitor->output->modes.empty()) {
            float currentWidth   = 0;
            float currentHeight  = 0;
            float currentRefresh = 0;
            bool  success        = false;

            //(-1,-1) indicates a preference to refreshrate over resolution, (-1,-2) preference to resolution
            if (RULE->resolution == Vector2D(-1, -1)) {
                for (auto const& mode : pMonitor->output->modes) {
                    if ((mode->pixelSize.x >= currentWidth && mode->pixelSize.y >= currentHeight && mode->refreshRate >= (currentRefresh - 1000.f)) ||
                        mode->refreshRate > (currentRefresh + 3000.f)) {
                        pMonitor->output->state->setMode(mode);
                        if (pMonitor->state.test()) {
                            currentWidth   = mode->pixelSize.x;
                            currentHeight  = mode->pixelSize.y;
                            currentRefresh = mode->refreshRate;
                            success        = true;
                        }
                    }
                }
            } else {
                for (auto const& mode : pMonitor->output->modes) {
                    if ((mode->pixelSize.x >= currentWidth && mode->pixelSize.y >= currentHeight && mode->refreshRate >= (currentRefresh - 1000.f)) ||
                        (mode->pixelSize.x > currentWidth && mode->pixelSize.y > currentHeight)) {
                        pMonitor->output->state->setMode(mode);
                        if (pMonitor->state.test()) {
                            currentWidth   = mode->pixelSize.x;
                            currentHeight  = mode->pixelSize.y;
                            currentRefresh = mode->refreshRate;
                            success        = true;
                        }
                    }
                }
            }

            if (!success) {
                if (pMonitor->output->state->state().mode)
                    Debug::log(LOG, "Monitor {}: REJECTED mode: {:X0}@{:2f}! Falling back to preferred: {}x{}@{:2f}", pMonitor->output->name, RULE->resolution,
                               (float)RULE->refreshRate, pMonitor->output->state->state().mode->pixelSize.x, pMonitor->output->state->state().mode->pixelSize.y,
                               pMonitor->output->state->state().mode->refreshRate / 1000.f);

                const auto PREFERREDMODE = pMonitor->output->preferredMode();

                if (!PREFERREDMODE) {
                    Debug::log(ERR, "Monitor {} has NO PREFERRED MODE, and an INVALID one was requested: {:X0}@{:2f}", pMonitor->ID, RULE->resolution, (float)RULE->refreshRate);
                    return true;
                }

                // Preferred is valid
                pMonitor->output->state->setMode(PREFERREDMODE);

                Debug::log(ERR, "Monitor {} got an invalid requested mode: {:X0}@{:2f}, using the preferred one instead: {}x{}@{:2f}", pMonitor->output->name, RULE->resolution,
                           (float)RULE->refreshRate, PREFERREDMODE->pixelSize.x, PREFERREDMODE->pixelSize.y, PREFERREDMODE->refreshRate / 1000.f);

                pMonitor->refreshRate = PREFERREDMODE->refreshRate / 1000.f;
                pMonitor->vecSize     = PREFERREDMODE->pixelSize;
                pMonitor->currentMode = PREFERREDMODE;
            } else {

                Debug::log(LOG, "Monitor {}: Applying highest mode {}x{}@{:2f}.", pMonitor->output->name, (int)currentWidth, (int)currentHeight, (int)currentRefresh / 1000.f);

                pMonitor->refreshRate = currentRefresh / 1000.f;
                pMonitor->vecSize     = Vector2D(currentWidth, currentHeight);
            }
        }
    } else {
        const auto PREFERREDMODE = pMonitor->output->preferredMode();

        if (!PREFERREDMODE) {
            Debug::log(ERR, "Monitor {} has NO PREFERRED MODE", pMonitor->output->name);

            if (!pMonitor->output->modes.empty()) {
                for (auto const& mode : pMonitor->output->modes) {
                    pMonitor->output->state->setMode(mode);

                    if (!pMonitor->state.test()) {
                        Debug::log(LOG, "Monitor {}: REJECTED available mode: {}x{}@{:2f}!", pMonitor->output->name, mode->pixelSize.x, mode->pixelSize.y,
                                   mode->refreshRate / 1000.f);
                        continue;
                    }

                    Debug::log(LOG, "Monitor {}: requested {:X0}@{:2f}, found available mode: {}x{}@{}mHz, applying.", pMonitor->output->name, RULE->resolution,
                               (float)RULE->refreshRate, mode->pixelSize.x, mode->pixelSize.y, mode->refreshRate);

                    pMonitor->refreshRate = mode->refreshRate / 1000.f;
                    pMonitor->vecSize     = mode->pixelSize;
                    pMonitor->currentMode = mode;

                    break;
                }
            }
        } else {
            // Preferred is valid
            pMonitor->output->state->setMode(PREFERREDMODE);

            pMonitor->vecSize     = PREFERREDMODE->pixelSize;
            pMonitor->refreshRate = PREFERREDMODE->refreshRate / 1000.f;
            pMonitor->currentMode = PREFERREDMODE;

            Debug::log(LOG, "Setting preferred mode for {}", pMonitor->output->name);
        }
    }

    pMonitor->vrrActive = pMonitor->output->state->state().adaptiveSync // disabled here, will be tested in CConfigManager::ensureVRR()
        || pMonitor->createdByUser;                                     // wayland backend doesn't allow for disabling adaptive_sync

    pMonitor->vecPixelSize = pMonitor->vecSize;

    // clang-format off
    static const std::array<std::vector<std::pair<std::string, uint32_t>>, 2> formats{
        std::vector<std::pair<std::string, uint32_t>>{ /* 10-bit */
            {"DRM_FORMAT_XRGB2101010", DRM_FORMAT_XRGB2101010}, {"DRM_FORMAT_XBGR2101010", DRM_FORMAT_XBGR2101010}, {"DRM_FORMAT_XRGB8888", DRM_FORMAT_XRGB8888}, {"DRM_FORMAT_XBGR8888", DRM_FORMAT_XBGR8888}, {"DRM_FORMAT_INVALID", DRM_FORMAT_INVALID}
        },
        std::vector<std::pair<std::string, uint32_t>>{ /* 8-bit */
            {"DRM_FORMAT_XRGB8888", DRM_FORMAT_XRGB8888}, {"DRM_FORMAT_XBGR8888", DRM_FORMAT_XBGR8888}, {"DRM_FORMAT_INVALID", DRM_FORMAT_INVALID}
        }
    };
    // clang-format on

    bool set10bit = false;

    for (auto const& fmt : formats[(int)!RULE->enable10bit]) {
        pMonitor->output->state->setFormat(fmt.second);
        pMonitor->prevDrmFormat = pMonitor->drmFormat;
        pMonitor->drmFormat     = fmt.second;

        if (!pMonitor->state.test()) {
            Debug::log(ERR, "output {} failed basic test on format {}", pMonitor->szName, fmt.first);
        } else {
            Debug::log(LOG, "output {} succeeded basic test on format {}", pMonitor->szName, fmt.first);
            if (RULE->enable10bit && fmt.first.contains("101010"))
                set10bit = true;
            break;
        }
    }

    pMonitor->enabled10bit = set10bit;

    Vector2D logicalSize = pMonitor->vecPixelSize / pMonitor->scale;
    if (!*PDISABLESCALECHECKS && (logicalSize.x != std::round(logicalSize.x) || logicalSize.y != std::round(logicalSize.y))) {
        // invalid scale, will produce fractional pixels.
        // find the nearest valid.

        float    searchScale = std::round(pMonitor->scale * 120.0);
        bool     found       = false;

        double   scaleZero = searchScale / 120.0;

        Vector2D logicalZero = pMonitor->vecPixelSize / scaleZero;
        if (logicalZero == logicalZero.round())
            pMonitor->scale = scaleZero;
        else {
            for (size_t i = 1; i < 90; ++i) {
                double   scaleUp   = (searchScale + i) / 120.0;
                double   scaleDown = (searchScale - i) / 120.0;

                Vector2D logicalUp   = pMonitor->vecPixelSize / scaleUp;
                Vector2D logicalDown = pMonitor->vecPixelSize / scaleDown;

                if (logicalUp == logicalUp.round()) {
                    found       = true;
                    searchScale = scaleUp;
                    break;
                }
                if (logicalDown == logicalDown.round()) {
                    found       = true;
                    searchScale = scaleDown;
                    break;
                }
            }

            if (!found) {
                if (autoScale)
                    pMonitor->scale = std::round(scaleZero);
                else {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} failed to find a clean divisor", pMonitor->scale);
                    g_pConfigManager->addParseError("Invalid scale passed to monitor " + pMonitor->szName + ", failed to find a clean divisor");
                    pMonitor->scale = pMonitor->getDefaultScale();
                }
            } else {
                if (!autoScale) {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} found suggestion {}", pMonitor->scale, searchScale);
                    g_pConfigManager->addParseError(
                        std::format("Invalid scale passed to monitor {}, failed to find a clean divisor. Suggested nearest scale: {:5f}", pMonitor->szName, searchScale));
                    pMonitor->scale = pMonitor->getDefaultScale();
                } else
                    pMonitor->scale = searchScale;
            }
        }
    }

    pMonitor->output->scheduleFrame();

    if (!pMonitor->state.commit())
        Debug::log(ERR, "Couldn't commit output named {}", pMonitor->output->name);

    Vector2D xfmd                = pMonitor->transform % 2 == 1 ? Vector2D{pMonitor->vecPixelSize.y, pMonitor->vecPixelSize.x} : pMonitor->vecPixelSize;
    pMonitor->vecSize            = (xfmd / pMonitor->scale).round();
    pMonitor->vecTransformedSize = xfmd;

    if (pMonitor->createdByUser) {
        CBox transformedBox = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
        transformedBox.transform(wlTransformToHyprutils(invertTransform(pMonitor->transform)), pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y);

        pMonitor->vecPixelSize = Vector2D(transformedBox.width, transformedBox.height);
    }

    pMonitor->updateMatrix();

    if (WAS10B != pMonitor->enabled10bit || OLDRES != pMonitor->vecPixelSize)
        g_pHyprOpenGL->destroyMonitorResources(pMonitor);

    g_pCompositor->arrangeMonitors();

    pMonitor->damage.setSize(pMonitor->vecTransformedSize);

    // Set scale for all surfaces on this monitor, needed for some clients
    // but not on unsafe state to avoid crashes
    if (!g_pCompositor->m_bUnsafeState) {
        for (auto const& w : g_pCompositor->m_vWindows) {
            w->updateSurfaceScaleTransformDetails();
        }
    }
    // updato us
    arrangeLayersForMonitor(pMonitor->ID);

    // reload to fix mirrors
    g_pConfigManager->m_bWantsMonitorReload = true;

    Debug::log(LOG, "Monitor {} data dump: res {:X}@{:.2f}Hz, scale {:.2f}, transform {}, pos {:X}, 10b {}", pMonitor->szName, pMonitor->vecPixelSize, pMonitor->refreshRate,
               pMonitor->scale, (int)pMonitor->transform, pMonitor->vecPosition, (int)pMonitor->enabled10bit);

    EMIT_HOOK_EVENT("monitorLayoutChanged", nullptr);

    pMonitor->events.modeChanged.emit();

    return true;
}

void CHyprRenderer::setCursorSurface(SP<CWLSurface> surf, int hotspotX, int hotspotY, bool force) {
    m_bCursorHasSurface = surf;

    m_sLastCursorData.name     = "";
    m_sLastCursorData.surf     = surf;
    m_sLastCursorData.hotspotX = hotspotX;
    m_sLastCursorData.hotspotY = hotspotY;

    if (m_bCursorHidden && !force)
        return;

    g_pCursorManager->setCursorSurface(surf, {hotspotX, hotspotY});
}

void CHyprRenderer::setCursorFromName(const std::string& name, bool force) {
    m_bCursorHasSurface = true;

    if (name == m_sLastCursorData.name && !force)
        return;

    m_sLastCursorData.name = name;
    m_sLastCursorData.surf.reset();

    if (m_bCursorHidden && !force)
        return;

    g_pCursorManager->setCursorFromName(name);
}

void CHyprRenderer::ensureCursorRenderingMode() {
    static auto PCURSORTIMEOUT = CConfigValue<Hyprlang::FLOAT>("cursor:inactive_timeout");
    static auto PHIDEONTOUCH   = CConfigValue<Hyprlang::INT>("cursor:hide_on_touch");
    static auto PHIDEONKEY     = CConfigValue<Hyprlang::INT>("cursor:hide_on_key_press");

    if (*PCURSORTIMEOUT <= 0)
        m_sCursorHiddenConditions.hiddenOnTimeout = false;
    if (*PHIDEONTOUCH == 0)
        m_sCursorHiddenConditions.hiddenOnTouch = false;
    if (*PHIDEONKEY == 0)
        m_sCursorHiddenConditions.hiddenOnKeyboard = false;

    if (*PCURSORTIMEOUT > 0)
        m_sCursorHiddenConditions.hiddenOnTimeout = *PCURSORTIMEOUT < g_pInputManager->m_tmrLastCursorMovement.getSeconds();

    const bool HIDE = m_sCursorHiddenConditions.hiddenOnTimeout || m_sCursorHiddenConditions.hiddenOnTouch || m_sCursorHiddenConditions.hiddenOnKeyboard;

    if (HIDE == m_bCursorHidden)
        return;

    if (HIDE) {
        Debug::log(LOG, "Hiding the cursor (hl-mandated)");

        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (!g_pPointerManager->softwareLockedFor(m))
                continue;

            g_pHyprRenderer->damageMonitor(m); // TODO: maybe just damage the cursor area?
        }

        setCursorHidden(true);

    } else {
        Debug::log(LOG, "Showing the cursor (hl-mandated)");

        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (!g_pPointerManager->softwareLockedFor(m))
                continue;

            g_pHyprRenderer->damageMonitor(m); // TODO: maybe just damage the cursor area?
        }

        setCursorHidden(false);
    }
}

void CHyprRenderer::setCursorHidden(bool hide) {

    if (hide == m_bCursorHidden)
        return;

    m_bCursorHidden = hide;

    if (hide) {
        g_pPointerManager->resetCursorImage();
        return;
    }

    if (m_sLastCursorData.surf.has_value())
        setCursorSurface(m_sLastCursorData.surf.value(), m_sLastCursorData.hotspotX, m_sLastCursorData.hotspotY, true);
    else if (!m_sLastCursorData.name.empty())
        setCursorFromName(m_sLastCursorData.name, true);
    else
        setCursorFromName("left_ptr", true);
}

bool CHyprRenderer::shouldRenderCursor() {
    return !m_bCursorHidden && m_bCursorHasSurface;
}

std::tuple<float, float, float> CHyprRenderer::getRenderTimes(PHLMONITOR pMonitor) {
    const auto POVERLAY = &g_pDebugOverlay->m_mMonitorOverlays[pMonitor];

    float      avgRenderTime = 0;
    float      maxRenderTime = 0;
    float      minRenderTime = 9999;
    for (auto const& rt : POVERLAY->m_dLastRenderTimes) {
        if (rt > maxRenderTime)
            maxRenderTime = rt;
        if (rt < minRenderTime)
            minRenderTime = rt;
        avgRenderTime += rt;
    }
    avgRenderTime /= POVERLAY->m_dLastRenderTimes.size() == 0 ? 1 : POVERLAY->m_dLastRenderTimes.size();

    return std::make_tuple<>(avgRenderTime, maxRenderTime, minRenderTime);
}

static int handleCrashLoop(void* data) {

    g_pHyprNotificationOverlay->addNotification("Hyprland will crash in " + std::to_string(10 - (int)(g_pHyprRenderer->m_fCrashingDistort * 2.f)) + "s.", CColor(0), 5000,
                                                ICON_INFO);

    g_pHyprRenderer->m_fCrashingDistort += 0.5f;

    if (g_pHyprRenderer->m_fCrashingDistort >= 5.5f)
        raise(SIGABRT);

    wl_event_source_timer_update(g_pHyprRenderer->m_pCrashingLoop, 1000);

    return 1;
}

void CHyprRenderer::initiateManualCrash() {
    g_pHyprNotificationOverlay->addNotification("Manual crash initiated. Farewell...", CColor(0), 5000, ICON_INFO);

    m_pCrashingLoop = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_pCrashingLoop, 1000);

    m_bCrashingInProgress = true;
    m_fCrashingDistort    = 0.5;

    g_pHyprOpenGL->m_tGlobalTimer.reset();

    static auto PDT = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("debug:damage_tracking"));

    **PDT = 0;
}

void CHyprRenderer::setOccludedForMainWorkspace(CRegion& region, PHLWORKSPACE pWorkspace) {
    CRegion    rg;

    const auto PMONITOR = pWorkspace->m_pMonitor.lock();

    if (!PMONITOR->activeSpecialWorkspace)
        return;

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || w->m_pWorkspace != PMONITOR->activeSpecialWorkspace)
            continue;

        if (!w->opaque())
            continue;

        const auto     ROUNDING = w->rounding() * PMONITOR->scale;
        const Vector2D POS  = w->m_vRealPosition.value() + Vector2D{ROUNDING, ROUNDING} - PMONITOR->vecPosition + (w->m_bPinned ? Vector2D{} : pWorkspace->m_vRenderOffset.value());
        const Vector2D SIZE = w->m_vRealSize.value() - Vector2D{ROUNDING * 2, ROUNDING * 2};

        CBox           box = {POS.x, POS.y, SIZE.x, SIZE.y};

        box.scale(PMONITOR->scale);

        rg.add(box);
    }

    region.subtract(rg);
}

void CHyprRenderer::setOccludedForBackLayers(CRegion& region, PHLWORKSPACE pWorkspace) {
    CRegion     rg;

    const auto  PMONITOR = pWorkspace->m_pMonitor.lock();

    static auto PBLUR       = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    const auto  BLURRADIUS  = *PBLUR ? (*PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES)) : 0;

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || w->m_pWorkspace != pWorkspace)
            continue;

        if (!w->opaque())
            continue;

        const auto     ROUNDING = w->rounding() * PMONITOR->scale;
        const Vector2D POS  = w->m_vRealPosition.value() + Vector2D{ROUNDING, ROUNDING} - PMONITOR->vecPosition + (w->m_bPinned ? Vector2D{} : pWorkspace->m_vRenderOffset.value());
        const Vector2D SIZE = w->m_vRealSize.value() - Vector2D{ROUNDING * 2, ROUNDING * 2};

        CBox           box = {POS.x, POS.y, SIZE.x, SIZE.y};

        box.scale(PMONITOR->scale).expand(-BLURRADIUS);

        g_pHyprOpenGL->m_RenderData.renderModif.applyToBox(box);

        rg.add(box);
    }

    region.subtract(rg);
}

bool CHyprRenderer::canSkipBackBufferClear(PHLMONITOR pMonitor) {
    for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        if (!ls->layerSurface)
            continue;

        if (ls->alpha.value() < 1.f)
            continue;

        if (ls->geometry.x != pMonitor->vecPosition.x || ls->geometry.y != pMonitor->vecPosition.y || ls->geometry.width != pMonitor->vecSize.x ||
            ls->geometry.height != pMonitor->vecSize.y)
            continue;

        // TODO: cache maybe?
        CRegion opaque = ls->layerSurface->surface->current.opaque;
        CBox    lsbox  = {{}, ls->layerSurface->surface->current.size};
        opaque.invert(lsbox);

        if (!opaque.empty())
            continue;

        return true;
    }

    return false;
}

void CHyprRenderer::recheckSolitaryForMonitor(PHLMONITOR pMonitor) {
    pMonitor->solitaryClient.reset(); // reset it, if we find one it will be set.

    if (g_pHyprNotificationOverlay->hasAny() || g_pSessionLockManager->isSessionLocked())
        return;

    const auto PWORKSPACE = pMonitor->activeWorkspace;

    if (!PWORKSPACE || !PWORKSPACE->m_bHasFullscreenWindow || PROTO::data->dndActive() || pMonitor->activeSpecialWorkspace || PWORKSPACE->m_fAlpha.value() != 1.f ||
        PWORKSPACE->m_vRenderOffset.value() != Vector2D{})
        return;

    const auto PCANDIDATE = PWORKSPACE->getFullscreenWindow();

    if (!PCANDIDATE)
        return; // ????

    if (!PCANDIDATE->opaque())
        return;

    if (PCANDIDATE->m_vRealSize.value() != pMonitor->vecSize || PCANDIDATE->m_vRealPosition.value() != pMonitor->vecPosition || PCANDIDATE->m_vRealPosition.isBeingAnimated() ||
        PCANDIDATE->m_vRealSize.isBeingAnimated())
        return;

    if (!pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty())
        return;

    for (auto const& topls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (topls->alpha.value() != 0.f)
            return;
    }

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w == PCANDIDATE || (!w->m_bIsMapped && !w->m_bFadingOut) || w->isHidden())
            continue;

        if (w->m_pWorkspace == PCANDIDATE->m_pWorkspace && w->m_bIsFloating && w->m_bCreatedOverFullscreen && w->visibleOnMonitor(pMonitor))
            return;
    }

    if (pMonitor->activeSpecialWorkspace)
        return;

    // check if it did not open any subsurfaces or shit
    int surfaceCount = 0;
    if (PCANDIDATE->m_bIsX11) {
        surfaceCount = 1;
    } else {
        surfaceCount = PCANDIDATE->popupsCount() + PCANDIDATE->surfacesCount();
    }

    if (surfaceCount > 1)
        return;

    // found one!
    pMonitor->solitaryClient = PCANDIDATE;
}

SP<CRenderbuffer> CHyprRenderer::getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto it = std::find_if(m_vRenderbuffers.begin(), m_vRenderbuffers.end(), [&](const auto& other) { return other->m_pHLBuffer == buffer; });

    if (it != m_vRenderbuffers.end())
        return *it;

    auto buf = makeShared<CRenderbuffer>(buffer, fmt);

    if (!buf->good())
        return nullptr;

    m_vRenderbuffers.emplace_back(buf);
    return buf;
}

void CHyprRenderer::makeEGLCurrent() {
    if (!g_pCompositor || !g_pHyprOpenGL)
        return;

    if (eglGetCurrentContext() != g_pHyprOpenGL->m_pEglContext)
        eglMakeCurrent(g_pHyprOpenGL->m_pEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, g_pHyprOpenGL->m_pEglContext);
}

void CHyprRenderer::unsetEGL() {
    if (!g_pHyprOpenGL)
        return;

    eglMakeCurrent(g_pHyprOpenGL->m_pEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool CHyprRenderer::beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, CFramebuffer* fb, bool simple) {

    makeEGLCurrent();

    m_eRenderMode = mode;

    g_pHyprOpenGL->m_RenderData.pMonitor = pMonitor; // has to be set cuz allocs

    if (mode == RENDER_MODE_FULL_FAKE) {
        RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
        fb->bind();
        if (simple)
            g_pHyprOpenGL->beginSimple(pMonitor, damage, nullptr, fb);
        else
            g_pHyprOpenGL->begin(pMonitor, damage, fb);
        return true;
    }

    /* This is a constant expression, as we always use double-buffering in our swapchain
        TODO: Rewrite the CDamageRing to take advantage of that maybe? It's made to support longer swapchains atm because we used to do wlroots */
    static constexpr const int HL_BUFFER_AGE = 2;

    if (!buffer) {
        m_pCurrentBuffer = pMonitor->output->swapchain->next(nullptr);
        if (!m_pCurrentBuffer) {
            Debug::log(ERR, "Failed to acquire swapchain buffer for {}", pMonitor->szName);
            return false;
        }
    } else
        m_pCurrentBuffer = buffer;

    try {
        m_pCurrentRenderbuffer = getOrCreateRenderbuffer(m_pCurrentBuffer, pMonitor->output->state->state().drmFormat);
    } catch (std::exception& e) {
        Debug::log(ERR, "getOrCreateRenderbuffer failed for {}", pMonitor->szName);
        return false;
    }

    if (!m_pCurrentRenderbuffer) {
        Debug::log(ERR, "failed to start a render pass for output {}, no RBO could be obtained", pMonitor->szName);
        return false;
    }

    if (mode == RENDER_MODE_NORMAL) {
        damage = pMonitor->damage.getBufferDamage(HL_BUFFER_AGE);
        pMonitor->damage.rotate();
    }

    m_pCurrentRenderbuffer->bind();
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, m_pCurrentRenderbuffer);
    else
        g_pHyprOpenGL->begin(pMonitor, damage);

    return true;
}

void CHyprRenderer::endRender() {
    const auto  PMONITOR           = g_pHyprOpenGL->m_RenderData.pMonitor;
    static auto PNVIDIAANTIFLICKER = CConfigValue<Hyprlang::INT>("opengl:nvidia_anti_flicker");

    PMONITOR->commitSeq++;

    auto cleanup = CScopeGuard([this]() {
        if (m_pCurrentRenderbuffer)
            m_pCurrentRenderbuffer->unbind();
        m_pCurrentRenderbuffer = nullptr;
        m_pCurrentBuffer       = nullptr;
    });

    if (m_eRenderMode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pHyprOpenGL->end();
    else {
        g_pHyprOpenGL->m_RenderData.pMonitor.reset();
        g_pHyprOpenGL->m_RenderData.mouseZoomFactor   = 1.f;
        g_pHyprOpenGL->m_RenderData.mouseZoomUseMouse = true;
    }

    if (m_eRenderMode == RENDER_MODE_FULL_FAKE)
        return;

    if (m_eRenderMode == RENDER_MODE_NORMAL) {
        PMONITOR->output->state->setBuffer(m_pCurrentBuffer);

        auto explicitOptions = getExplicitSyncSettings();

        if (PMONITOR->inTimeline && explicitOptions.explicitEnabled && explicitOptions.explicitKMSEnabled) {
            auto sync = g_pHyprOpenGL->createEGLSync(-1);
            if (!sync) {
                Debug::log(ERR, "renderer: couldn't create an EGLSync for out in endRender");
                return;
            }

            bool ok = PMONITOR->inTimeline->importFromSyncFileFD(PMONITOR->commitSeq, sync->fd());
            if (!ok) {
                Debug::log(ERR, "renderer: couldn't import from sync file fd in endRender");
                return;
            }

            auto fd = PMONITOR->inTimeline->exportAsSyncFileFD(PMONITOR->commitSeq);
            if (fd <= 0) {
                Debug::log(ERR, "renderer: couldn't export from sync timeline in endRender");
                return;
            }

            PMONITOR->output->state->setExplicitInFence(fd);
        } else {
            if (isNvidia() && *PNVIDIAANTIFLICKER)
                glFinish();
            else
                glFlush();
        }
    }
}

void CHyprRenderer::onRenderbufferDestroy(CRenderbuffer* rb) {
    std::erase_if(m_vRenderbuffers, [&](const auto& rbo) { return rbo.get() == rb; });
}

SP<CRenderbuffer> CHyprRenderer::getCurrentRBO() {
    return m_pCurrentRenderbuffer;
}

bool CHyprRenderer::isNvidia() {
    return m_bNvidia;
}

SExplicitSyncSettings CHyprRenderer::getExplicitSyncSettings() {
    static auto           PENABLEEXPLICIT    = CConfigValue<Hyprlang::INT>("render:explicit_sync");
    static auto           PENABLEEXPLICITKMS = CConfigValue<Hyprlang::INT>("render:explicit_sync_kms");

    SExplicitSyncSettings settings;
    settings.explicitEnabled    = *PENABLEEXPLICIT;
    settings.explicitKMSEnabled = *PENABLEEXPLICITKMS;

    if (*PENABLEEXPLICIT == 2 /* auto */)
        settings.explicitEnabled = true;
    if (*PENABLEEXPLICITKMS == 2 /* auto */) {
        if (!m_bNvidia)
            settings.explicitKMSEnabled = true;
        else {

            // check nvidia version. Explicit KMS is supported in >=560
            // in the case of an error, driverMajor will stay 0 and explicit KMS will be disabled
            static int  driverMajor = 0;

            static bool once = true;
            if (once) {
                once = false;

                Debug::log(LOG, "Renderer: checking for explicit KMS support for nvidia");

                if (std::filesystem::exists("/sys/module/nvidia_drm/version")) {
                    Debug::log(LOG, "Renderer: Nvidia version file exists");

                    std::ifstream ifs("/sys/module/nvidia_drm/version");
                    if (ifs.good()) {
                        try {
                            std::string driverInfo((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

                            Debug::log(LOG, "Renderer: Read nvidia version {}", driverInfo);

                            CVarList ver(driverInfo, 0, '.', true);
                            driverMajor = std::stoi(ver[0]);

                            Debug::log(LOG, "Renderer: Parsed nvidia major version: {}", driverMajor);

                        } catch (std::exception& e) { settings.explicitKMSEnabled = false; }

                        ifs.close();
                    }
                }
            }

            settings.explicitKMSEnabled = driverMajor >= 560;
        }
    }

    return settings;
}

void CHyprRenderer::addWindowToRenderUnfocused(PHLWINDOW window) {
    static auto PFPS = CConfigValue<Hyprlang::INT>("misc:render_unfocused_fps");

    if (std::find(m_vRenderUnfocused.begin(), m_vRenderUnfocused.end(), window) != m_vRenderUnfocused.end())
        return;

    m_vRenderUnfocused.emplace_back(window);

    if (!m_tRenderUnfocusedTimer->armed())
        m_tRenderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
}
