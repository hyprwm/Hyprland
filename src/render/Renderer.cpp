#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/sync/SyncReleaser.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../managers/CursorManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/AnimationManager.hpp"
#include "../managers/LayoutManager.hpp"
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
#include "../hyprerror/HyprError.hpp"
#include "../debug/HyprDebugOverlay.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/RendererHintsPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "debug/Log.hpp"
#include "protocols/ColorManagement.hpp"
#if AQUAMARINE_VERSION_NUMBER > 702 // >0.7.2
#include "protocols/types/ContentType.hpp"
#endif

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
    if (pWindow->m_iMonitorMovedFrom != -1 && pWindow->m_fMovingToWorkspaceAlpha->isBeingAnimated() && pWindow->m_fMovingToWorkspaceAlpha->value() > 0.F && pWindow->m_pWorkspace &&
        !pWindow->m_pWorkspace->isVisible())
        return true;

    const auto PWINDOWWORKSPACE = pWindow->m_pWorkspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_pMonitor == pMonitor) {
        if (PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated() || PWINDOWWORKSPACE->m_fAlpha->isBeingAnimated() || PWINDOWWORKSPACE->m_bForceRendering)
            return true;

        // if hidden behind fullscreen
        if (PWINDOWWORKSPACE->m_bHasFullscreenWindow && !pWindow->isFullscreen() && (!pWindow->m_bIsFloating || !pWindow->m_bCreatedOverFullscreen) &&
            pWindow->m_fAlpha->value() == 0)
            return false;

        if (!PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated() && !PWINDOWWORKSPACE->m_fAlpha->isBeingAnimated() && !PWINDOWWORKSPACE->isVisible())
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
    if (!pWindow->m_bIsFloating && pWindow->m_vRealPosition->isBeingAnimated() && pWindow->m_bAnimatingIn && pWindow->m_pMonitor != pMonitor)
        return false;

    if (pWindow->m_vRealPosition->isBeingAnimated()) {
        if (PWINDOWWORKSPACE && !PWINDOWWORKSPACE->m_bIsSpecialWorkspace && PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated())
            return false;
        // render window if window and monitor intersect
        // (when moving out of or through a monitor)
        CBox windowBox = pWindow->getFullWindowBoundingBox();
        if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated())
            windowBox.translate(PWINDOWWORKSPACE->m_vRenderOffset->value());
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
        if (PWORKSPACE && PWORKSPACE->m_pMonitor == m && (PWORKSPACE->m_vRenderOffset->isBeingAnimated() || PWORKSPACE->m_fAlpha->isBeingAnimated()))
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

        if (w->m_fAlpha->value() == 0.f)
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

        if (w->m_fAlpha->value() == 0.f)
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
            if (!(PWORKSPACE && (PWORKSPACE->m_vRenderOffset->isBeingAnimated() || PWORKSPACE->m_fAlpha->isBeingAnimated() || PWORKSPACE->m_bForceRendering)))
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
        if (w->m_pWorkspace != pWorkspaceWindow->m_pWorkspace || !w->m_bIsFloating || (!w->m_bCreatedOverFullscreen && !w->m_bPinned) || (!w->m_bIsMapped && !w->m_bFadingOut) ||
            w->isFullscreen())
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

        windows.emplace_back(w);
    }

    // Non-floating main
    for (auto& w : windows) {
        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_iMonitorMovedFrom != -1 && (w->m_pWorkspace && !w->m_pWorkspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render active window after all others of this pass
        if (w == g_pCompositor->m_pLastWindow) {
            lastWindow = w.lock();
            continue;
        }

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_MAIN);
        w.reset();
    }

    if (lastWindow)
        renderWindow(lastWindow, pMonitor, time, true, RENDER_PASS_MAIN);

    lastWindow.reset();

    // Non-floating popup
    for (auto& w : windows) {
        if (!w)
            continue;

        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_iMonitorMovedFrom != -1 && (w->m_pWorkspace && !w->m_pWorkspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
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

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_iMonitorMovedFrom != -1 && (w->m_pWorkspace && !w->m_pWorkspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_bIsSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_bIsSpecialWorkspace && w->m_pMonitor != pWorkspace->m_pMonitor)
            continue; // special on another are rendered as a part of the base pass

        // render the bad boy
        renderWindow(w.lock(), pMonitor, time, true, RENDER_PASS_ALL);
    }
}

void CHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, timespec* time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool standalone) {
    if (pWindow->isHidden() && !standalone)
        return;

    if (pWindow->m_bFadingOut) {
        if (pMonitor == pWindow->m_pMonitor) // TODO: fix this
            renderSnapshot(pWindow);
        return;
    }

    if (!pWindow->m_bIsMapped)
        return;

    TRACY_GPU_ZONE("RenderWindow");

    const auto                       PWORKSPACE = pWindow->m_pWorkspace;
    const auto                       REALPOS    = pWindow->m_vRealPosition->value() + (pWindow->m_bPinned ? Vector2D{} : PWORKSPACE->m_vRenderOffset->value());
    static auto                      PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");
    static auto                      PBLUR      = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time};
    CBox                             textureBox = {REALPOS.x, REALPOS.y, std::max(pWindow->m_vRealSize->value().x, 5.0), std::max(pWindow->m_vRealSize->value().y, 5.0)};

    renderdata.pos.x = textureBox.x;
    renderdata.pos.y = textureBox.y;
    renderdata.w     = textureBox.w;
    renderdata.h     = textureBox.h;

    if (ignorePosition) {
        renderdata.pos.x = pMonitor->vecPosition.x;
        renderdata.pos.y = pMonitor->vecPosition.y;
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
    const bool USE_WORKSPACE_FADE_ALPHA = pWindow->m_iMonitorMovedFrom != -1 && (!PWORKSPACE || !PWORKSPACE->isVisible());
    const bool DONT_BLUR                = pWindow->m_sWindowData.noBlur.valueOrDefault() || pWindow->m_sWindowData.RGBX.valueOrDefault() || pWindow->opaque();

    renderdata.surface   = pWindow->m_pWLSurface->resource();
    renderdata.dontRound = pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN) || pWindow->m_sWindowData.noRounding.valueOrDefault();
    renderdata.fadeAlpha = pWindow->m_fAlpha->value() * (pWindow->m_bPinned || USE_WORKSPACE_FADE_ALPHA ? 1.f : PWORKSPACE->m_fAlpha->value()) *
        (USE_WORKSPACE_FADE_ALPHA ? pWindow->m_fMovingToWorkspaceAlpha->value() : 1.F) * pWindow->m_fMovingFromWorkspaceAlpha->value();
    renderdata.alpha         = pWindow->m_fActiveInactiveAlpha->value();
    renderdata.decorate      = decorate && !pWindow->m_bX11DoesntWantBorders && !pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderdata.rounding      = standalone || renderdata.dontRound ? 0 : pWindow->rounding() * pMonitor->scale;
    renderdata.roundingPower = standalone || renderdata.dontRound ? 2.0f : pWindow->roundingPower();
    renderdata.blur          = !standalone && *PBLUR && !DONT_BLUR;
    renderdata.pWindow       = pWindow;

    if (standalone) {
        renderdata.alpha     = 1.f;
        renderdata.fadeAlpha = 1.f;
    }

    // apply opaque
    if (pWindow->m_sWindowData.opaque.valueOrDefault())
        renderdata.alpha = 1.f;

    renderdata.pWindow = pWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOW);

    if (*PDIMAROUND && pWindow->m_sWindowData.dimAround.valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {
        CBox                        monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};
        CRectPassElement::SRectData data;
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * renderdata.alpha * renderdata.fadeAlpha);
        data.box   = monbox;
        m_sRenderPass.add(makeShared<CRectPassElement>(data));
    }

    renderdata.pos.x += pWindow->m_vFloatingOffset.x;
    renderdata.pos.y += pWindow->m_vFloatingOffset.y;

    // if window is floating and we have a slide animation, clip it to its full bb
    if (!ignorePosition && pWindow->m_bIsFloating && !pWindow->isFullscreen() && PWORKSPACE->m_vRenderOffset->isBeingAnimated() && !pWindow->m_bPinned) {
        CRegion rg =
            pWindow->getFullWindowBoundingBox().translate(-pMonitor->vecPosition + PWORKSPACE->m_vRenderOffset->value() + pWindow->m_vFloatingOffset).scale(pMonitor->scale);
        renderdata.clipBox = rg.getExtents();
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
            renderdata.useNearestNeighbor = true;

        if (!pWindow->m_sWindowData.noBlur.valueOrDefault() && pWindow->m_pWLSurface->small() && !pWindow->m_pWLSurface->m_bFillIgnoreSmall && renderdata.blur && *PBLUR) {
            CBox wb = {renderdata.pos.x - pMonitor->vecPosition.x, renderdata.pos.y - pMonitor->vecPosition.y, renderdata.w, renderdata.h};
            wb.scale(pMonitor->scale).round();
            CRectPassElement::SRectData data;
            data.color = CHyprColor(0, 0, 0, 0);
            data.box   = wb;
            data.round = renderdata.dontRound ? 0 : renderdata.rounding - 1;
            data.blur  = true;
            data.blurA = renderdata.fadeAlpha;
            data.xray  = g_pHyprOpenGL->shouldUseNewBlurOptimizations(nullptr, pWindow);
            m_sRenderPass.add(makeShared<CRectPassElement>(data));
            renderdata.blur = false;
        }

        renderdata.surfaceCounter = 0;
        pWindow->m_pWLSurface->resource()->breadthfirst(
            [this, &renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                renderdata.localPos    = offset;
                renderdata.texture     = s->current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pWindow->m_pWLSurface->resource();
                m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            nullptr);

        renderdata.useNearestNeighbor = false;

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

            renderdata.pos -= geom.pos();
            renderdata.dontRound       = true; // don't round popups
            renderdata.pMonitor        = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            renderdata.popup           = true;

            static CConfigValue PBLURPOPUPS  = CConfigValue<Hyprlang::INT>("decoration:blur:popups");
            static CConfigValue PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:popups_ignorealpha");

            renderdata.blur = *PBLURPOPUPS && *PBLUR;

            if (renderdata.blur) {
                renderdata.discardMode |= DISCARD_ALPHA;
                renderdata.discardOpacity = *PBLURIGNOREA;
            }

            if (pWindow->m_sWindowData.nearestNeighbor.valueOrDefault())
                renderdata.useNearestNeighbor = true;

            renderdata.surfaceCounter = 0;

            pWindow->m_pPopupHead->breadthfirst(
                [this, &renderdata](WP<CPopup> popup, void* data) {
                    if (!popup->m_pWLSurface || !popup->m_pWLSurface->resource() || !popup->m_bMapped)
                        return;
                    const auto     pos    = popup->coordsRelativeToParent();
                    const Vector2D oldPos = renderdata.pos;
                    renderdata.pos += pos;

                    popup->m_pWLSurface->resource()->breadthfirst(
                        [this, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                            renderdata.localPos    = offset;
                            renderdata.texture     = s->current.texture;
                            renderdata.surface     = s;
                            renderdata.mainSurface = false;
                            m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
                            renderdata.surfaceCounter++;
                        },
                        data);

                    renderdata.pos = oldPos;
                },
                &renderdata);
        }

        if (decorate) {
            for (auto const& wd : pWindow->m_dWindowDecorations) {
                if (wd->getDecorationLayer() != DECORATION_LAYER_OVERLAY)
                    continue;

                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha);
            }
        }
    }

    // for plugins
    g_pHyprOpenGL->m_RenderData.currentWindow = pWindow;

    EMIT_HOOK_EVENT("render", RENDER_POST_WINDOW);

    g_pHyprOpenGL->m_RenderData.currentWindow.reset();
}

void CHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, timespec* time, bool popups) {
    if (!pLayer)
        return;

    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    if (*PDIMAROUND && pLayer->dimAround && !m_bRenderingSnapshot && !popups) {
        CRectPassElement::SRectData data;
        data.box   = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pLayer->alpha->value());
        m_sRenderPass.add(makeShared<CRectPassElement>(data));
    }

    if (pLayer->fadingOut) {
        if (!popups)
            renderSnapshot(pLayer);
        return;
    }

    static auto PBLUR = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    TRACY_GPU_ZONE("RenderLayer");

    const auto                       REALPOS = pLayer->realPosition->value();
    const auto                       REALSIZ = pLayer->realSize->value();

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, REALPOS};
    renderdata.fadeAlpha                        = pLayer->alpha->value();
    renderdata.blur                             = pLayer->forceBlur && *PBLUR;
    renderdata.surface                          = pLayer->surface->resource();
    renderdata.decorate                         = false;
    renderdata.w                                = REALSIZ.x;
    renderdata.h                                = REALSIZ.y;
    renderdata.pLS                              = pLayer;
    renderdata.blockBlurOptimization            = pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    renderdata.clipBox = CBox{0, 0, pMonitor->vecSize.x, pMonitor->vecSize.y}.scale(pMonitor->scale);

    if (renderdata.blur && pLayer->ignoreAlpha) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = pLayer->ignoreAlphaValue;
    }

    if (!popups)
        pLayer->surface->resource()->breadthfirst(
            [this, &renderdata, &pLayer](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                renderdata.localPos    = offset;
                renderdata.texture     = s->current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pLayer->surface->resource();
                m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    renderdata.popup           = true;
    renderdata.blur            = pLayer->forceBlurPopups;
    renderdata.surfaceCounter  = 0;
    if (popups) {
        pLayer->popupHead->breadthfirst(
            [this, &renderdata](WP<CPopup> popup, void* data) {
                if (!popup->m_pWLSurface || !popup->m_pWLSurface->resource() || !popup->m_bMapped)
                    return;

                Vector2D pos           = popup->coordsRelativeToParent();
                renderdata.localPos    = pos;
                renderdata.texture     = popup->m_pWLSurface->resource()->current.texture;
                renderdata.surface     = popup->m_pWLSurface->resource();
                renderdata.mainSurface = false;
                m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);
    }
}

void CHyprRenderer::renderIMEPopup(CInputPopup* pPopup, PHLMONITOR pMonitor, timespec* time) {
    const auto                       POS = pPopup->globalBox().pos();

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, POS};

    const auto                       SURF = pPopup->getSurface();

    renderdata.surface  = SURF;
    renderdata.decorate = false;
    renderdata.w        = SURF->current.size.x;
    renderdata.h        = SURF->current.size.y;

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
            renderdata.localPos    = offset;
            renderdata.texture     = s->current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == SURF;
            m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void CHyprRenderer::renderSessionLockSurface(WP<SSessionLockSurface> pSurface, PHLMONITOR pMonitor, timespec* time) {
    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->vecPosition, pMonitor->vecPosition};

    renderdata.blur     = false;
    renderdata.surface  = pSurface->surface->surface();
    renderdata.decorate = false;
    renderdata.w        = pMonitor->vecSize.x;
    renderdata.h        = pMonitor->vecSize.y;

    renderdata.surface->breadthfirst(
        [this, &renderdata, &pSurface](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            renderdata.localPos    = offset;
            renderdata.texture     = s->current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pSurface->surface->surface();
            m_sRenderPass.add(makeShared<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void CHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* time, const Vector2D& translate, const float& scale) {
    static auto PDIMSPECIAL      = CConfigValue<Hyprlang::FLOAT>("decoration:dim_special");
    static auto PBLURSPECIAL     = CConfigValue<Hyprlang::INT>("decoration:blur:special");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PRENDERTEX       = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");
    static auto PXPMODE          = CConfigValue<Hyprlang::INT>("render:xp_mode");

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

    SRenderModifData RENDERMODIFDATA;
    if (translate != Vector2D{0, 0})
        RENDERMODIFDATA.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate));
    if (scale != 1.f)
        RENDERMODIFDATA.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale));

    if (!RENDERMODIFDATA.modifs.empty()) {
        g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RENDERMODIFDATA}));
    }

    CScopeGuard x([&RENDERMODIFDATA] {
        if (!RENDERMODIFDATA.modifs.empty()) {
            g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        }
    });

    if (!pWorkspace) {
        // allow rendering without a workspace. In this case, just render layers.

        if (*PRENDERTEX /* inverted cfg flag */)
            m_sRenderPass.add(makeShared<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));
        else
            g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

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

        return;
    }

    if (!*PXPMODE) {
        if (*PRENDERTEX /* inverted cfg flag */)
            m_sRenderPass.add(makeShared<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));
        else
            g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
        for (auto const& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            renderLayer(ls.lock(), pMonitor, time);
        }
    }

    // pre window pass
    g_pHyprOpenGL->preWindowPass();

    if (pWorkspace->m_bHasFullscreenWindow)
        renderWorkspaceWindowsFullscreen(pMonitor, pWorkspace, time);
    else
        renderWorkspaceWindows(pMonitor, pWorkspace, time);

    // and then special
    for (auto const& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_pMonitor == pMonitor && ws->m_fAlpha->value() > 0.f && ws->m_bIsSpecialWorkspace) {
            const auto SPECIALANIMPROGRS = ws->m_vRenderOffset->isBeingAnimated() ? ws->m_vRenderOffset->getCurveValue() : ws->m_fAlpha->getCurveValue();
            const bool ANIMOUT           = !pMonitor->activeSpecialWorkspace;

            if (*PDIMSPECIAL != 0.f) {
                CRectPassElement::SRectData data;
                data.box   = {translate.x, translate.y, pMonitor->vecTransformedSize.x * scale, pMonitor->vecTransformedSize.y * scale};
                data.color = CHyprColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS));

                g_pHyprRenderer->m_sRenderPass.add(makeShared<CRectPassElement>(data));
            }

            if (*PBLURSPECIAL && *PBLUR) {
                CRectPassElement::SRectData data;
                data.box   = {translate.x, translate.y, pMonitor->vecTransformedSize.x * scale, pMonitor->vecTransformedSize.y * scale};
                data.color = CHyprColor(0, 0, 0, 0);
                data.blur  = true;
                data.blurA = (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS);

                g_pHyprRenderer->m_sRenderPass.add(makeShared<CRectPassElement>(data));
            }

            break;
        }
    }

    // special
    for (auto const& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_fAlpha->value() > 0.f && ws->m_bIsSpecialWorkspace) {
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
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockDead2Texture, monbox, ALPHA);
    } else {
        // render image, with instructions. Lock is gone.
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockDeadTexture, monbox, ALPHA);

        // also render text for the tty number
        if (g_pHyprOpenGL->m_pLockTtyTextTexture) {
            CBox texbox = {{}, g_pHyprOpenGL->m_pLockTtyTextTexture->m_vSize};
            g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pLockTtyTextTexture, texbox, 1.F);
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

    if (!g_pCompositor->m_bSessionActive)
        return;

    if (pMonitor->ID == m_pMostHzMonitor->ID ||
        *PVFR == 1) { // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that
        g_pCompositor->sanityCheckWorkspaces();

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();
    }

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

    if ((*PDIRECTSCANOUT == 1 ||
         (*PDIRECTSCANOUT == 2 && pMonitor->activeWorkspace->getFullscreenWindow() && pMonitor->activeWorkspace->getFullscreenWindow()->getContentType() == CONTENT_TYPE_GAME)) &&
        !shouldTear) {
        if (pMonitor->attemptDirectScanout()) {
            return;
        } else if (!pMonitor->lastScanout.expired()) {
            Debug::log(LOG, "Left a direct scanout.");
            pMonitor->lastScanout.reset();

            // reset DRM format, but only if needed since it might modeset
            if (pMonitor->output->state->state().drmFormat != pMonitor->prevDrmFormat)
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
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->forceFullFrames > 0 || damageBlinkCleanup > 0)
        damage = {0, 0, (int)pMonitor->vecTransformedSize.x * 10, (int)pMonitor->vecTransformedSize.y * 10};

    finalDamage = damage;

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
                    CRectPassElement::SRectData data;
                    data.box   = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
                    data.color = CHyprColor(1.0, 0.0, 1.0, 100.0 / 255.0);
                    m_sRenderPass.add(makeShared<CRectPassElement>(data));
                    damageBlinkCleanup = 1;
                } else if (*PDAMAGEBLINK) {
                    damageBlinkCleanup++;
                    if (damageBlinkCleanup > 3)
                        damageBlinkCleanup = 0;
                }
            }
        } else
            renderWindow(pMonitor->solitaryClient.lock(), pMonitor, &now, false, RENDER_PASS_MAIN /* solitary = no popups */);
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

    CRegion    frameDamage{g_pHyprOpenGL->m_RenderData.damage};

    const auto TRANSFORM = invertTransform(pMonitor->transform);
    frameDamage.transform(wlTransformToHyprutils(TRANSFORM), pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y);

    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
        frameDamage.add(0, 0, (int)pMonitor->vecTransformedSize.x, (int)pMonitor->vecTransformedSize.y);

    if (*PDAMAGEBLINK)
        frameDamage.add(damage);

    if (!pMonitor->mirrors.empty())
        damageMirrorsWith(pMonitor, frameDamage);

    pMonitor->renderingActive = false;

    EMIT_HOOK_EVENT("render", RENDER_POST);

    pMonitor->output->state->addDamage(frameDamage);
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
        } else
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, durationUs);
    }
}

static const auto BT709 = Aquamarine::IOutput::SChromaticityCoords{
    .red   = Aquamarine::IOutput::xy{.x = 0.64, .y = 0.33},
    .green = Aquamarine::IOutput::xy{.x = 0.30, .y = 0.60},
    .blue  = Aquamarine::IOutput::xy{.x = 0.15, .y = 0.06},
    .white = Aquamarine::IOutput::xy{.x = 0.3127, .y = 0.3290},
};

static hdr_output_metadata createHDRMetadata(uint8_t eotf, Aquamarine::IOutput::SParsedEDID edid) {
    if (eotf == 0)
        return hdr_output_metadata{.hdmi_metadata_type1 = hdr_metadata_infoframe{.eotf = 0}}; // empty metadata for SDR

    const auto toNits      = [](float value) { return uint16_t(std::round(value)); };
    const auto to16Bit     = [](float value) { return uint16_t(std::round(value * 50000)); };
    const auto colorimetry = edid.chromaticityCoords.value_or(BT709);

    Debug::log(TRACE, "ColorManagement primaries {},{} {},{} {},{} {},{}", colorimetry.red.x, colorimetry.red.y, colorimetry.green.x, colorimetry.green.y, colorimetry.blue.x,
               colorimetry.blue.y, colorimetry.white.x, colorimetry.white.y);
    Debug::log(TRACE, "ColorManagement max avg {}, min {}, max {}", edid.hdrMetadata->desiredMaxFrameAverageLuminance, edid.hdrMetadata->desiredContentMinLuminance,
               edid.hdrMetadata->desiredContentMaxLuminance);
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
                .max_display_mastering_luminance = toNits(edid.hdrMetadata->desiredMaxFrameAverageLuminance),
                .min_display_mastering_luminance = toNits(edid.hdrMetadata->desiredContentMinLuminance * 10000),
                .max_cll                         = toNits(edid.hdrMetadata->desiredMaxFrameAverageLuminance),
                .max_fall                        = toNits(edid.hdrMetadata->desiredMaxFrameAverageLuminance),
            },
    };
}

static hdr_output_metadata createHDRMetadata(SImageDescription settings, Aquamarine::IOutput::SParsedEDID edid) {
    if (settings.transferFunction != CM_TRANSFER_FUNCTION_ST2084_PQ)
        return hdr_output_metadata{.hdmi_metadata_type1 = hdr_metadata_infoframe{.eotf = 0}}; // empty metadata for SDR

    const auto toNits  = [](uint32_t value) { return uint16_t(std::round(value)); };
    const auto to16Bit = [](uint32_t value) { return uint16_t(std::round(value * 50000)); };

    auto       colorimetry = settings.primaries;
    auto       luminances  = settings.masteringLuminances.max > 0 ?
               settings.masteringLuminances :
               SImageDescription::SPCMasteringLuminances{.min = edid.hdrMetadata->desiredContentMinLuminance, .max = edid.hdrMetadata->desiredContentMaxLuminance};

    Debug::log(TRACE, "ColorManagement primaries {},{} {},{} {},{} {},{}", colorimetry.red.x, colorimetry.red.y, colorimetry.green.x, colorimetry.green.y, colorimetry.blue.x,
               colorimetry.blue.y, colorimetry.white.x, colorimetry.white.y);
    Debug::log(TRACE, "ColorManagement min {}, max {}, cll {}, fall {}", luminances.min, luminances.max, settings.maxCLL, settings.maxFALL);
    return hdr_output_metadata{
        .metadata_type = 0,
        .hdmi_metadata_type1 =
            hdr_metadata_infoframe{
                .eotf          = 2,
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
                .max_cll                         = toNits(settings.maxCLL),
                .max_fall                        = toNits(settings.maxFALL),
            },
    };
}

bool CHyprRenderer::commitPendingAndDoExplicitSync(PHLMONITOR pMonitor) {
    // apply timelines for explicit sync
    // save inFD otherwise reset will reset it
    CFileDescriptor inFD{pMonitor->output->state->state().explicitInFence};
    pMonitor->output->state->resetExplicitFences();
    if (inFD.isValid())
        pMonitor->output->state->setExplicitInFence(inFD.get());

    static auto PHDR = CConfigValue<Hyprlang::INT>("experimental:hdr");

    const bool  SUPPORTSPQ = pMonitor->output->parsedEDID.hdrMetadata.has_value() ? pMonitor->output->parsedEDID.hdrMetadata->supportsPQ : false;
    Debug::log(TRACE, "ColorManagement supportsBT2020 {}, supportsPQ {}", pMonitor->output->parsedEDID.supportsBT2020, SUPPORTSPQ);
    if (pMonitor->output->parsedEDID.supportsBT2020 && SUPPORTSPQ) {
        if (*PHDR != (pMonitor->imageDescription.transferFunction == NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ)) {
            if (*PHDR) {
                pMonitor->imageDescription.transferFunction     = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ;
                pMonitor->imageDescription.primariesNameSet     = true;
                pMonitor->imageDescription.primariesNamed       = NColorManagement::CM_PRIMARIES_BT2020;
                pMonitor->imageDescription.luminances.min       = pMonitor->output->parsedEDID.hdrMetadata->desiredContentMinLuminance;
                pMonitor->imageDescription.luminances.max       = pMonitor->output->parsedEDID.hdrMetadata->desiredContentMaxLuminance;
                pMonitor->imageDescription.luminances.reference = pMonitor->output->parsedEDID.hdrMetadata->desiredMaxFrameAverageLuminance;
            } else {
                pMonitor->imageDescription = {};
            }
        }
        if (pMonitor->activeWorkspace && pMonitor->activeWorkspace->m_bHasFullscreenWindow && pMonitor->activeWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN) {
            const auto WINDOW    = pMonitor->activeWorkspace->getFullscreenWindow();
            const auto ROOT_SURF = WINDOW->m_pWLSurface->resource();
            const auto SURF =
                ROOT_SURF->findFirstPreorder([ROOT_SURF](SP<CWLSurfaceResource> surf) { return surf->colorManagement.valid() && surf->extends() == ROOT_SURF->extends(); });

            if (SURF && SURF->colorManagement.valid() && SURF->colorManagement->hasImageDescription()) {
                bool needsHdrMetadataUpdate = SURF->colorManagement->needsHdrMetadataUpdate() || pMonitor->m_previousFSWindow != WINDOW;
                if (SURF->colorManagement->needsHdrMetadataUpdate())
                    SURF->colorManagement->setHDRMetadata(createHDRMetadata(SURF->colorManagement->imageDescription(), pMonitor->output->parsedEDID));
                if (needsHdrMetadataUpdate)
                    pMonitor->output->state->setHDRMetadata(SURF->colorManagement->hdrMetadata());
            } else if ((pMonitor->output->state->state().hdrMetadata.hdmi_metadata_type1.eotf == 2) != *PHDR)
                pMonitor->output->state->setHDRMetadata(*PHDR ? createHDRMetadata(2, pMonitor->output->parsedEDID) : createHDRMetadata(0, pMonitor->output->parsedEDID));
            pMonitor->m_previousFSWindow = WINDOW;
        } else {
            if ((pMonitor->output->state->state().hdrMetadata.hdmi_metadata_type1.eotf == 2) != *PHDR)
                pMonitor->output->state->setHDRMetadata(*PHDR ? createHDRMetadata(2, pMonitor->output->parsedEDID) : createHDRMetadata(0, pMonitor->output->parsedEDID));
            pMonitor->m_previousFSWindow.reset();
        }
    }

    static auto PWIDE    = CConfigValue<Hyprlang::INT>("experimental:wide_color_gamut");
    const bool  needsWCG = *PWIDE || pMonitor->output->state->state().hdrMetadata.hdmi_metadata_type1.eotf == 2;
    if (pMonitor->output->state->state().wideColorGamut != needsWCG) {
        Debug::log(TRACE, "Setting wide color gamut {}", needsWCG ? "on" : "off");
        pMonitor->output->state->setWideColorGamut(needsWCG);

        // FIXME do not trust enabled10bit, auto switch to 10bit and back if needed
        if (needsWCG && !pMonitor->enabled10bit) {
            Debug::log(WARN, "Wide color gamut is enabled but the display is not in 10bit mode");
            static bool shown = false;
            if (!shown) {
                g_pHyprNotificationOverlay->addNotification("Wide color gamut is enabled but the display is not in 10bit mode", CHyprColor{}, 15000, ICON_WARNING);
                shown = true;
            }
        }
    }

#if AQUAMARINE_VERSION_NUMBER > 702 // >0.7.2
    if (pMonitor->activeWorkspace && pMonitor->activeWorkspace->m_bHasFullscreenWindow && pMonitor->activeWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN) {
        const auto WINDOW = pMonitor->activeWorkspace->getFullscreenWindow();
        pMonitor->output->state->setContentType(NContentType::toDRM(WINDOW->getContentType()));
    } else
        pMonitor->output->state->setContentType(NContentType::toDRM(CONTENT_TYPE_NONE));
#endif

    if (pMonitor->ctmUpdated) {
        pMonitor->ctmUpdated = false;
        pMonitor->output->state->setCTM(pMonitor->ctm);
    }

    bool ok = pMonitor->state.commit();
    if (!ok) {
        if (inFD.isValid()) {
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

    auto explicitOptions = getExplicitSyncSettings(pMonitor->output);
    if (!explicitOptions.explicitEnabled)
        return ok;

    Debug::log(TRACE, "Explicit: {} presented", explicitPresented.size());
    auto sync = g_pHyprOpenGL->createEGLSync({});

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

    renderAllClientsForWorkspace(pMonitor, pWorkspace, now, translate, scale);
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

        *ls->realPosition = box.pos();
        *ls->realSize     = box.size();
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

        m->addDamage(damageBoxForEach);
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
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset->isBeingAnimated() && !pWindow->m_bPinned)
        windowBox.translate(PWINDOWWORKSPACE->m_vRenderOffset->value());
    windowBox.translate(pWindow->m_vFloatingOffset);

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (forceFull || shouldRenderWindow(pWindow, m)) { // only damage if window is rendered on monitor
            CBox fixedDamageBox = {windowBox.x - m->vecPosition.x, windowBox.y - m->vecPosition.y, windowBox.width, windowBox.height};
            fixedDamageBox.scale(m->scale);
            m->addDamage(fixedDamageBox);
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
    pMonitor->addDamage(damageBox);

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Monitor {}", pMonitor->szName);
}

void CHyprRenderer::damageBox(const CBox& box, bool skipFrameSchedule) {
    if (g_pCompositor->m_bUnsafeState)
        return;

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (m->isMirror())
            continue; // don't damage mirrors traditionally

        if (!skipFrameSchedule) {
            CBox damageBox = box.copy().translate(-m->vecPosition).scale(m->scale);
            m->addDamage(damageBox);
        }
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Box: xy: {}, {} wh: {}, {}", box.x, box.y, box.w, box.h);
}

void CHyprRenderer::damageBox(const int& x, const int& y, const int& w, const int& h) {
    CBox box = {x, y, w, h};
    damageBox(box);
}

void CHyprRenderer::damageRegion(const CRegion& rg) {
    for (auto const& RECT : rg.getRects()) {
        damageBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1);
    }
}

void CHyprRenderer::damageMirrorsWith(PHLMONITOR pMonitor, const CRegion& pRegion) {
    for (auto const& mirror : pMonitor->mirrors) {

        // transform the damage here, so it won't get clipped by the monitor damage ring
        auto    monitor = mirror;

        CRegion transformed{pRegion};

        // we want to transform to the same box as in CHyprOpenGLImpl::renderMirrored
        double scale  = std::min(monitor->vecTransformedSize.x / pMonitor->vecTransformedSize.x, monitor->vecTransformedSize.y / pMonitor->vecTransformedSize.y);
        CBox   monbox = {0, 0, pMonitor->vecTransformedSize.x * scale, pMonitor->vecTransformedSize.y * scale};
        monbox.x      = (monitor->vecTransformedSize.x - monbox.w) / 2;
        monbox.y      = (monitor->vecTransformedSize.y - monbox.h) / 2;

        transformed.scale(scale);
        transformed.transform(wlTransformToHyprutils(pMonitor->transform), pMonitor->vecPixelSize.x * scale, pMonitor->vecPixelSize.y * scale);
        transformed.translate(Vector2D(monbox.x, monbox.y));

        mirror->addDamage(transformed);

        g_pCompositor->scheduleFrameForMonitor(mirror.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }
}

void CHyprRenderer::renderDragIcon(PHLMONITOR pMonitor, timespec* time) {
    PROTO::data->renderDND(pMonitor, time);
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

            damageMonitor(m); // TODO: maybe just damage the cursor area?
        }

        setCursorHidden(true);

    } else {
        Debug::log(LOG, "Showing the cursor (hl-mandated)");

        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (!g_pPointerManager->softwareLockedFor(m))
                continue;

            damageMonitor(m); // TODO: maybe just damage the cursor area?
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

    g_pHyprNotificationOverlay->addNotification("Hyprland will crash in " + std::to_string(10 - (int)(g_pHyprRenderer->m_fCrashingDistort * 2.f)) + "s.", CHyprColor(0), 5000,
                                                ICON_INFO);

    g_pHyprRenderer->m_fCrashingDistort += 0.5f;

    if (g_pHyprRenderer->m_fCrashingDistort >= 5.5f)
        raise(SIGABRT);

    wl_event_source_timer_update(g_pHyprRenderer->m_pCrashingLoop, 1000);

    return 1;
}

void CHyprRenderer::initiateManualCrash() {
    g_pHyprNotificationOverlay->addNotification("Manual crash initiated. Farewell...", CHyprColor(0), 5000, ICON_INFO);

    m_pCrashingLoop = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_pCrashingLoop, 1000);

    m_bCrashingInProgress = true;
    m_fCrashingDistort    = 0.5;

    g_pHyprOpenGL->m_tGlobalTimer.reset();

    static auto PDT = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("debug:damage_tracking"));

    **PDT = 0;
}

void CHyprRenderer::recheckSolitaryForMonitor(PHLMONITOR pMonitor) {
    pMonitor->solitaryClient.reset(); // reset it, if we find one it will be set.

    if (g_pHyprNotificationOverlay->hasAny() || g_pSessionLockManager->isSessionLocked())
        return;

    const auto PWORKSPACE = pMonitor->activeWorkspace;

    if (!PWORKSPACE || !PWORKSPACE->m_bHasFullscreenWindow || PROTO::data->dndActive() || pMonitor->activeSpecialWorkspace || PWORKSPACE->m_fAlpha->value() != 1.f ||
        PWORKSPACE->m_vRenderOffset->value() != Vector2D{})
        return;

    const auto PCANDIDATE = PWORKSPACE->getFullscreenWindow();

    if (!PCANDIDATE)
        return; // ????

    if (!PCANDIDATE->opaque())
        return;

    if (PCANDIDATE->m_vRealSize->value() != pMonitor->vecSize || PCANDIDATE->m_vRealPosition->value() != pMonitor->vecPosition || PCANDIDATE->m_vRealPosition->isBeingAnimated() ||
        PCANDIDATE->m_vRealSize->isBeingAnimated())
        return;

    if (!pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty())
        return;

    for (auto const& topls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (topls->alpha->value() != 0.f)
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
    if (PCANDIDATE->m_bIsX11)
        surfaceCount = 1;
    else
        surfaceCount = PCANDIDATE->popupsCount() + PCANDIDATE->surfacesCount();

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

    m_sRenderPass.clear();

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

    g_pHyprOpenGL->m_RenderData.damage = m_sRenderPass.render(g_pHyprOpenGL->m_RenderData.damage);

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

        auto explicitOptions = getExplicitSyncSettings(PMONITOR->output);

        if (PMONITOR->inTimeline && explicitOptions.explicitEnabled && explicitOptions.explicitKMSEnabled) {
            auto sync = g_pHyprOpenGL->createEGLSync({});
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
            if (!fd.isValid()) {
                Debug::log(ERR, "renderer: couldn't export from sync timeline in endRender");
                return;
            }

            PMONITOR->output->state->setExplicitInFence(fd.take());
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

SExplicitSyncSettings CHyprRenderer::getExplicitSyncSettings(SP<Aquamarine::IOutput> output) {
    static auto           PENABLEEXPLICIT    = CConfigValue<Hyprlang::INT>("render:explicit_sync");
    static auto           PENABLEEXPLICITKMS = CConfigValue<Hyprlang::INT>("render:explicit_sync_kms");

    SExplicitSyncSettings settings;
    settings.explicitEnabled    = *PENABLEEXPLICIT;
    settings.explicitKMSEnabled = *PENABLEEXPLICITKMS;

    if (!output->supportsExplicit) {
        settings.explicitEnabled    = false;
        settings.explicitKMSEnabled = false;

        return settings;
    }

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

void CHyprRenderer::makeRawWindowSnapshot(PHLWINDOW pWindow, CFramebuffer* pFramebuffer) {
    // we trust the window is valid.
    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    makeEGLCurrent();

    pFramebuffer->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->output->state->state().drmFormat);
    pFramebuffer->addStencil(g_pHyprOpenGL->m_RenderData.pCurrentMonData->stencilTex);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, pFramebuffer);

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    static auto* const PBLUR   = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("decoration:blur:enabled"));
    const auto         BLURVAL = **PBLUR;
    **PBLUR                    = 0;

    // TODO: how can we make this the size of the window? setting it to window's size makes the entire screen render with the wrong res forever more. odd.
    glViewport(0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    g_pHyprOpenGL->m_RenderData.currentFB = pFramebuffer;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    renderWindow(pWindow, PMONITOR, &now, false, RENDER_PASS_ALL, true);

    **PBLUR = BLURVAL;

    endRender();
}

void CHyprRenderer::makeWindowSnapshot(PHLWINDOW pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    if (!shouldRenderWindow(pWindow))
        return; // ignore, window is not being rendered

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual damage
    CRegion      fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    PHLWINDOWREF ref{pWindow};

    makeEGLCurrent();

    const auto PFRAMEBUFFER = &g_pHyprOpenGL->m_mWindowFramebuffers[ref];

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->output->state->state().drmFormat);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    static auto* const PBLUR   = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("decoration:blur:enabled"));
    const auto         BLURVAL = **PBLUR;
    **PBLUR                    = 0;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders, RENDER_PASS_ALL);

    **PBLUR = BLURVAL;

    endRender();

    m_bRenderingSnapshot = false;
}

void CHyprRenderer::makeLayerSnapshot(PHLLS pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = pLayer->monitor.lock();

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    makeEGLCurrent();

    const auto PFRAMEBUFFER = &g_pHyprOpenGL->m_mLayerFramebuffers[pLayer];

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->output->state->state().drmFormat);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto BLURLSSTATUS = pLayer->forceBlur;
    pLayer->forceBlur       = false;

    // draw the layer
    renderLayer(pLayer, PMONITOR, &now);

    pLayer->forceBlur = BLURLSSTATUS;

    endRender();

    m_bRenderingSnapshot = false;
}

void CHyprRenderer::renderSnapshot(PHLWINDOW pWindow) {
    static auto  PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    PHLWINDOWREF ref{pWindow};

    if (!g_pHyprOpenGL->m_mWindowFramebuffers.contains(ref))
        return;

    const auto FBDATA = &g_pHyprOpenGL->m_mWindowFramebuffers.at(ref);

    if (!FBDATA->getTexture())
        return;

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    CBox       windowBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * pWindow->m_vRealSize->value().x / (pWindow->m_vOriginalClosedSize.x * PMONITOR->scale)),
                                (PMONITOR->scale * pWindow->m_vRealSize->value().y / (pWindow->m_vOriginalClosedSize.y * PMONITOR->scale)));

    windowBox.width  = PMONITOR->vecTransformedSize.x * scaleXY.x;
    windowBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    windowBox.x      = ((pWindow->m_vRealPosition->value().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - ((pWindow->m_vOriginalClosedPos.x * PMONITOR->scale) * scaleXY.x);
    windowBox.y      = ((pWindow->m_vRealPosition->value().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - ((pWindow->m_vOriginalClosedPos.y * PMONITOR->scale) * scaleXY.y);

    CRegion fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    if (*PDIMAROUND && pWindow->m_sWindowData.dimAround.valueOrDefault()) {

        CRectPassElement::SRectData data;

        data.box   = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y};
        data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pWindow->m_fAlpha->value());

        m_sRenderPass.add(makeShared<CRectPassElement>(data));
        damageMonitor(PMONITOR);
    }

    CTexPassElement::SRenderData data;
    data.flipEndFrame = true;
    data.tex          = FBDATA->getTexture();
    data.box          = windowBox;
    data.a            = pWindow->m_fAlpha->value();
    data.damage       = fakeDamage;

    m_sRenderPass.add(makeShared<CTexPassElement>(data));
}

void CHyprRenderer::renderSnapshot(PHLLS pLayer) {
    if (!g_pHyprOpenGL->m_mLayerFramebuffers.contains(pLayer))
        return;

    const auto FBDATA = &g_pHyprOpenGL->m_mLayerFramebuffers.at(pLayer);

    if (!FBDATA->getTexture())
        return;

    const auto PMONITOR = pLayer->monitor.lock();

    CBox       layerBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * pLayer->realSize->value().x / (pLayer->geometry.w * PMONITOR->scale)),
                                (PMONITOR->scale * pLayer->realSize->value().y / (pLayer->geometry.h * PMONITOR->scale)));

    layerBox.width  = PMONITOR->vecTransformedSize.x * scaleXY.x;
    layerBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    layerBox.x = ((pLayer->realPosition->value().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - (((pLayer->geometry.x - PMONITOR->vecPosition.x) * PMONITOR->scale) * scaleXY.x);
    layerBox.y = ((pLayer->realPosition->value().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - (((pLayer->geometry.y - PMONITOR->vecPosition.y) * PMONITOR->scale) * scaleXY.y);

    CRegion                      fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    CTexPassElement::SRenderData data;
    data.flipEndFrame = true;
    data.tex          = FBDATA->getTexture();
    data.box          = layerBox;
    data.a            = pLayer->alpha->value();
    data.damage       = fakeDamage;

    m_sRenderPass.add(makeShared<CTexPassElement>(data));
}
