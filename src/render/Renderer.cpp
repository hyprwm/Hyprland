#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../managers/CursorManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/animation/AnimationManager.hpp"
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
#include "helpers/CursorShapes.hpp"
#include "helpers/Monitor.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/RendererHintsPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "debug/Log.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "render/OpenGL.hpp"

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

CHyprRenderer::CHyprRenderer() {
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

            Debug::log(LOG, "DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor, DRMV->version_patchlevel,
                       std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});

            drmFreeVersion(DRMV);
        }
        m_mgpu = drmDevices > 1;
    } else {
        Debug::log(LOG, "Aq backend has no session, omitting full DRM node checks");

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

            Debug::log(LOG, "Primary DRM driver information: {} v{}.{}.{} from {} description {}", name, DRMV->version_major, DRMV->version_minor, DRMV->version_patchlevel,
                       std::string{DRMV->date, DRMV->date_len}, std::string{DRMV->desc, DRMV->desc_len});
        } else {
            Debug::log(LOG, "No primary DRM driver information found");
        }

        drmFreeVersion(DRMV);
    }

    if (m_nvidia)
        Debug::log(WARN, "NVIDIA detected, please remember to follow nvidia instructions on the wiki");

    // cursor hiding stuff

    static auto P = g_pHookSystem->hookDynamic("keyPress", [&](void* self, SCallbackInfo& info, std::any param) {
        if (m_cursorHiddenConditions.hiddenOnKeyboard)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = true;
        ensureCursorRenderingMode();
    });

    static auto P2 = g_pHookSystem->hookDynamic("mouseMove", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_cursorHiddenConditions.hiddenOnKeyboard && m_cursorHiddenConditions.hiddenOnTouch == g_pInputManager->m_lastInputTouch && !m_cursorHiddenConditions.hiddenOnTimeout)
            return;

        m_cursorHiddenConditions.hiddenOnKeyboard = false;
        m_cursorHiddenConditions.hiddenOnTimeout  = false;
        m_cursorHiddenConditions.hiddenOnTouch    = g_pInputManager->m_lastInputTouch;
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

                if (!w->m_wlSurface || !w->m_wlSurface->resource() || shouldRenderWindow(w.lock()))
                    continue;

                w->m_wlSurface->resource()->frame(Time::steadyNow());
                auto FEEDBACK = makeUnique<CQueuedPresentationData>(w->m_wlSurface->resource());
                FEEDBACK->attachMonitor(g_pCompositor->m_lastMonitor.lock());
                FEEDBACK->discarded();
                PROTO::presentation->queueData(std::move(FEEDBACK));
            }

            if (dirty)
                std::erase_if(m_renderUnfocused, [](const auto& e) { return !e || !e->m_windowData.renderUnfocused.valueOr(false); });

            if (!m_renderUnfocused.empty())
                m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
        },
        nullptr);

    g_pEventLoopManager->addTimer(m_renderUnfocusedTimer);
}

CHyprRenderer::~CHyprRenderer() {
    if (m_cursorTicker)
        wl_event_source_remove(m_cursorTicker);
}

bool CHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
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

bool CHyprRenderer::shouldRenderWindow(PHLWINDOW pWindow) {

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

void CHyprRenderer::renderWorkspaceWindowsFullscreen(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
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

void CHyprRenderer::renderWorkspaceWindows(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
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
        if (w == g_pCompositor->m_lastWindow) {
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

void CHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, const Time::steady_tp& time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool standalone) {
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

    renderdata.surface   = pWindow->m_wlSurface->resource();
    renderdata.dontRound = pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN) || pWindow->m_windowData.noRounding.valueOrDefault();
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
    if (pWindow->m_windowData.opaque.valueOrDefault())
        renderdata.alpha = 1.f;

    renderdata.pWindow = pWindow;

    // for plugins
    g_pHyprOpenGL->m_renderData.currentWindow = pWindow;

    EMIT_HOOK_EVENT("render", RENDER_PRE_WINDOW);

    const auto fullAlpha = renderdata.alpha * renderdata.fadeAlpha;

    if (*PDIMAROUND && pWindow->m_windowData.dimAround.valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {
        CBox                        monbox = {0, 0, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y};
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
        if ((pWindow->m_isX11 && *PXWLUSENN) || pWindow->m_windowData.nearestNeighbor.valueOrDefault())
            renderdata.useNearestNeighbor = true;

        if (pWindow->m_wlSurface->small() && !pWindow->m_wlSurface->m_fillIgnoreSmall && renderdata.blur) {
            CBox wb = {renderdata.pos.x - pMonitor->m_position.x, renderdata.pos.y - pMonitor->m_position.y, renderdata.w, renderdata.h};
            wb.scale(pMonitor->m_scale).round();
            CRectPassElement::SRectData data;
            data.color = CHyprColor(0, 0, 0, 0);
            data.box   = wb;
            data.round = renderdata.dontRound ? 0 : renderdata.rounding - 1;
            data.blur  = true;
            data.blurA = renderdata.fadeAlpha;
            data.xray  = g_pHyprOpenGL->shouldUseNewBlurOptimizations(nullptr, pWindow);
            m_renderPass.add(makeUnique<CRectPassElement>(data));
            renderdata.blur = false;
        }

        renderdata.surfaceCounter = 0;
        pWindow->m_wlSurface->resource()->breadthfirst(
            [this, &renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pWindow->m_wlSurface->resource();
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
            CFramebuffer* last = g_pHyprOpenGL->m_renderData.currentFB;
            for (auto const& t : pWindow->m_transformers) {
                last = t->transform(last);
            }

            g_pHyprOpenGL->bindBackOnMain();
            g_pHyprOpenGL->renderOffToMain(last);
        }
    }

    g_pHyprOpenGL->m_renderData.clipBox = CBox();

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

            if (pWindow->m_windowData.nearestNeighbor.valueOrDefault())
                renderdata.useNearestNeighbor = true;

            renderdata.surfaceCounter = 0;

            pWindow->m_popupHead->breadthfirst(
                [this, &renderdata](WP<CPopup> popup, void* data) {
                    if (popup->m_fadingOut) {
                        renderSnapshot(popup);
                        return;
                    }

                    if (!popup->m_wlSurface || !popup->m_wlSurface->resource() || !popup->m_mapped)
                        return;
                    const auto     pos    = popup->coordsRelativeToParent();
                    const Vector2D oldPos = renderdata.pos;
                    renderdata.pos += pos;
                    renderdata.fadeAlpha = popup->m_alpha->value();

                    popup->m_wlSurface->resource()->breadthfirst(
                        [this, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
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

void CHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, const Time::steady_tp& time, bool popups, bool lockscreen) {
    if (!pLayer)
        return;

    // skip rendering based on abovelock rule and make sure to not render abovelock layers twice
    if ((pLayer->m_aboveLockscreen && !lockscreen && g_pSessionLockManager->isSessionLocked()) || (lockscreen && !pLayer->m_aboveLockscreen))
        return;

    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    if (*PDIMAROUND && pLayer->m_dimAround && !m_bRenderingSnapshot && !popups) {
        CRectPassElement::SRectData data;
        data.box   = {0, 0, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y};
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
    renderdata.surface                          = pLayer->m_surface->resource();
    renderdata.decorate                         = false;
    renderdata.w                                = REALSIZ.x;
    renderdata.h                                = REALSIZ.y;
    renderdata.pLS                              = pLayer;
    renderdata.blockBlurOptimization            = pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    renderdata.clipBox = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y}.scale(pMonitor->m_scale);

    if (renderdata.blur && pLayer->m_ignoreAlpha) {
        renderdata.discardMode |= DISCARD_ALPHA;
        renderdata.discardOpacity = pLayer->m_ignoreAlphaValue;
    }

    if (!popups)
        pLayer->m_surface->resource()->breadthfirst(
            [this, &renderdata, &pLayer](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
                renderdata.localPos    = offset;
                renderdata.texture     = s->m_current.texture;
                renderdata.surface     = s;
                renderdata.mainSurface = s == pLayer->m_surface->resource();
                m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    renderdata.popup           = true;
    renderdata.blur            = pLayer->m_forceBlurPopups;
    renderdata.surfaceCounter  = 0;
    if (popups) {
        pLayer->m_popupHead->breadthfirst(
            [this, &renderdata](WP<CPopup> popup, void* data) {
                if (!popup->m_wlSurface || !popup->m_wlSurface->resource() || !popup->m_mapped)
                    return;

                Vector2D pos           = popup->coordsRelativeToParent();
                renderdata.localPos    = pos;
                renderdata.texture     = popup->m_wlSurface->resource()->m_current.texture;
                renderdata.surface     = popup->m_wlSurface->resource();
                renderdata.mainSurface = false;
                m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
                renderdata.surfaceCounter++;
            },
            &renderdata);
    }
}

void CHyprRenderer::renderIMEPopup(CInputPopup* pPopup, PHLMONITOR pMonitor, const Time::steady_tp& time) {
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
            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == SURF;
            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void CHyprRenderer::renderSessionLockSurface(WP<SSessionLockSurface> pSurface, PHLMONITOR pMonitor, const Time::steady_tp& time) {
    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->m_position, pMonitor->m_position};

    renderdata.blur     = false;
    renderdata.surface  = pSurface->surface->surface();
    renderdata.decorate = false;
    renderdata.w        = pMonitor->m_size.x;
    renderdata.h        = pMonitor->m_size.y;

    renderdata.surface->breadthfirst(
        [this, &renderdata, &pSurface](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pSurface->surface->surface();
            m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

void CHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate, const float& scale) {
    static auto PDIMSPECIAL      = CConfigValue<Hyprlang::FLOAT>("decoration:dim_special");
    static auto PBLURSPECIAL     = CConfigValue<Hyprlang::INT>("decoration:blur:special");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PXPMODE          = CConfigValue<Hyprlang::INT>("render:xp_mode");
    static auto PSESSIONLOCKXRAY = CConfigValue<Hyprlang::INT>("misc:session_lock_xray");

    if (!pMonitor)
        return;

    if (g_pSessionLockManager->isSessionLocked() && !*PSESSIONLOCKXRAY) {
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
    if (scale != 1.f)
        RENDERMODIFDATA.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale));

    if (!RENDERMODIFDATA.modifs.empty())
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RENDERMODIFDATA}));

    CScopeGuard x([&RENDERMODIFDATA] {
        if (!RENDERMODIFDATA.modifs.empty()) {
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        }
    });

    if (!pWorkspace) {
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

    if (!*PXPMODE) {
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
    g_pHyprOpenGL->preWindowPass();

    if (pWorkspace->m_hasFullscreenWindow)
        renderWorkspaceWindowsFullscreen(pMonitor, pWorkspace, time);
    else
        renderWorkspaceWindows(pMonitor, pWorkspace, time);

    // and then special
    if (pMonitor->m_specialFade->value() != 0.F) {
        const auto SPECIALANIMPROGRS = pMonitor->m_specialFade->getCurveValue();
        const bool ANIMOUT           = !pMonitor->m_activeSpecialWorkspace;

        if (*PDIMSPECIAL != 0.f) {
            CRectPassElement::SRectData data;
            data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
            data.color = CHyprColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS));

            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
        }

        if (*PBLURSPECIAL && *PBLUR) {
            CRectPassElement::SRectData data;
            data.box   = {translate.x, translate.y, pMonitor->m_transformedSize.x * scale, pMonitor->m_transformedSize.y * scale};
            data.color = CHyprColor(0, 0, 0, 0);
            data.blur  = true;
            data.blurA = (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS);

            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
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

void CHyprRenderer::renderBackground(PHLMONITOR pMonitor) {
    static auto PRENDERTEX       = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto PBACKGROUNDCOLOR = CConfigValue<Hyprlang::INT>("misc:background_color");

    if (*PRENDERTEX /* inverted cfg flag */ || pMonitor->m_backgroundOpacity->isBeingAnimated())
        m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));

    if (!*PRENDERTEX)
        g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"
}

void CHyprRenderer::renderLockscreen(PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry) {
    TRACY_GPU_ZONE("RenderLockscreen");

    const bool LOCKED = g_pSessionLockManager->isSessionLocked();
    if (!LOCKED) {
        g_pHyprOpenGL->ensureLockTexturesRendered(false);
        return;
    }

    const bool RENDERPRIMER = g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied();
    if (RENDERPRIMER)
        renderSessionLockPrimer(pMonitor);

    const auto PSLS              = g_pSessionLockManager->getSessionLockSurfaceForMonitor(pMonitor->m_id);
    const bool RENDERLOCKMISSING = (PSLS.expired() || g_pSessionLockManager->clientDenied()) && g_pSessionLockManager->shallConsiderLockMissing();

    g_pHyprOpenGL->ensureLockTexturesRendered(RENDERLOCKMISSING);

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

void CHyprRenderer::renderSessionLockPrimer(PHLMONITOR pMonitor) {
    static auto PSESSIONLOCKXRAY = CConfigValue<Hyprlang::INT>("misc:session_lock_xray");
    if (*PSESSIONLOCKXRAY)
        return;

    CRectPassElement::SRectData data;
    data.color = CHyprColor(0, 0, 0, 1.f);
    data.box   = CBox{{}, pMonitor->m_pixelSize};

    m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));
}

void CHyprRenderer::renderSessionLockMissing(PHLMONITOR pMonitor) {
    const bool ANY_PRESENT = g_pSessionLockManager->anySessionLockSurfacesPresent();

    // ANY_PRESENT: render image2, without instructions. Lock still "alive", unless texture dead
    // else: render image, with instructions. Lock is gone.
    CBox                         monbox = {{}, pMonitor->m_pixelSize};
    CTexPassElement::SRenderData data;
    data.tex = (ANY_PRESENT) ? g_pHyprOpenGL->m_lockDead2Texture : g_pHyprOpenGL->m_lockDeadTexture;
    data.box = monbox;
    data.a   = 1;

    m_renderPass.add(makeUnique<CTexPassElement>(data));

    if (!ANY_PRESENT && g_pHyprOpenGL->m_lockTtyTextTexture) {
        // also render text for the tty number
        CBox texbox = {{}, g_pHyprOpenGL->m_lockTtyTextTexture->m_size};
        data.tex    = g_pHyprOpenGL->m_lockTtyTextTexture;
        data.box    = texbox;

        m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
    }
}

static std::optional<Vector2D> getSurfaceExpectedSize(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main) {
    const auto CAN_USE_WINDOW       = pWindow && main;
    const auto WINDOW_SIZE_MISALIGN = CAN_USE_WINDOW && pWindow->getReportedSize() != pWindow->m_wlSurface->resource()->m_current.size;

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

void CHyprRenderer::calculateUVForSurface(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main, const Vector2D& projSize,
                                          const Vector2D& projSizeUnscaled, bool fixMisalignedFSV1) {
    if (!pWindow || !pWindow->m_isX11) {
        static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");

        Vector2D    uvTL;
        Vector2D    uvBR = Vector2D(1, 1);

        if (pSurface->m_current.viewport.hasSource && !fixMisalignedFSV1) {
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
            const Vector2D PIXELASUV    = Vector2D{1, 1} / pSurface->m_current.bufferSize;
            const Vector2D MISALIGNMENT = pSurface->m_current.bufferSize - projSize;
            if (MISALIGNMENT != Vector2D{})
                uvBR -= MISALIGNMENT * PIXELASUV;
        } else {
            // if the surface is smaller than our viewport, extend its edges.
            // this will break if later on xdg geometry is hit, but we really try
            // to let the apps know to NOT add CSD. Also if source is there.
            // there is no way to fix this if that's the case
            const auto MONITOR_WL_SCALE = std::ceil(pMonitor->m_scale);
            const bool SCALE_UNAWARE    = MONITOR_WL_SCALE > 1 && (MONITOR_WL_SCALE == pSurface->m_current.scale || !pSurface->m_current.viewport.hasDestination);
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

        g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
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

        g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

void CHyprRenderer::renderMonitor(PHLMONITOR pMonitor, bool commit) {
    static std::chrono::high_resolution_clock::time_point renderStart        = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point renderStartOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay   = std::chrono::high_resolution_clock::now();

    static auto                                           PDEBUGOVERLAY       = CConfigValue<Hyprlang::INT>("debug:overlay");
    static auto                                           PDAMAGETRACKINGMODE = CConfigValue<Hyprlang::INT>("debug:damage_tracking");
    static auto                                           PDAMAGEBLINK        = CConfigValue<Hyprlang::INT>("debug:damage_blink");
    static auto                                           PVFR                = CConfigValue<Hyprlang::INT>("misc:vfr");

    static int                                            damageBlinkCleanup = 0; // because double-buffered

    const float                                           ZOOMFACTOR = pMonitor->m_cursorZoom->value();

    if (pMonitor->m_pixelSize.x < 1 || pMonitor->m_pixelSize.y < 1) {
        Debug::log(ERR, "Refusing to render a monitor because of an invalid pixel size: {}", pMonitor->m_pixelSize);
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

    if (pMonitor->m_id == m_mostHzMonitor->m_id ||
        *PVFR == 1) { // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_wantsMonitorReload)
            g_pConfigManager->performMonitorReload();
    }

    if (pMonitor->m_scheduledRecalc) {
        pMonitor->m_scheduledRecalc = false;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->m_id);
    }

    if (!pMonitor->m_output->needsFrame && pMonitor->m_forceFullFrames == 0)
        return;

    // tearing and DS first
    bool shouldTear = pMonitor->updateTearing();

    if (pMonitor->attemptDirectScanout()) {
        return;
    } else if (!pMonitor->m_lastScanout.expired()) {
        Debug::log(LOG, "Left a direct scanout.");
        pMonitor->m_lastScanout.reset();

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
        Debug::log(CRIT, "Damage tracking mode -1 ????");
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
        g_pHyprOpenGL->m_renderData.mouseZoomFactor = std::clamp(ZOOMFACTOR, 1.f, INFINITY);
    else
        g_pHyprOpenGL->m_renderData.mouseZoomFactor = 1.f;

    if (pMonitor->m_zoomAnimProgress->value() != 1) {
        g_pHyprOpenGL->m_renderData.mouseZoomFactor    = 2.0 - pMonitor->m_zoomAnimProgress->value(); // 2x zoom -> 1x zoom
        g_pHyprOpenGL->m_renderData.mouseZoomUseMouse  = false;
        g_pHyprOpenGL->m_renderData.useNearestNeighbor = false;
    }

    CRegion damage, finalDamage;
    if (!beginRender(pMonitor, damage, RENDER_MODE_NORMAL)) {
        Debug::log(ERR, "renderer: couldn't beginRender()!");
        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->m_forceFullFrames > 0 || damageBlinkCleanup > 0)
        damage = {0, 0, sc<int>(pMonitor->m_transformedSize.x) * 10, sc<int>(pMonitor->m_transformedSize.y) * 10};

    finalDamage = damage;

    // update damage in renderdata as we modified it
    g_pHyprOpenGL->setDamage(damage, finalDamage);

    if (pMonitor->m_forceFullFrames > 0) {
        pMonitor->m_forceFullFrames -= 1;
        if (pMonitor->m_forceFullFrames > 10)
            pMonitor->m_forceFullFrames = 0;
    }

    EMIT_HOOK_EVENT("render", RENDER_BEGIN);

    bool renderCursor = true;

    if (!finalDamage.empty()) {
        if (pMonitor->m_solitaryClient.expired()) {
            if (pMonitor->isMirror()) {
                g_pHyprOpenGL->blend(false);
                g_pHyprOpenGL->renderMirrored();
                g_pHyprOpenGL->blend(true);
                EMIT_HOOK_EVENT("render", RENDER_POST_MIRROR);
                renderCursor = false;
            } else {
                CBox renderBox = {0, 0, sc<int>(pMonitor->m_pixelSize.x), sc<int>(pMonitor->m_pixelSize.y)};
                renderWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW, renderBox);

                renderLockscreen(pMonitor, NOW, renderBox);

                if (pMonitor == g_pCompositor->m_lastMonitor) {
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
        } else
            renderWindow(pMonitor->m_solitaryClient.lock(), pMonitor, NOW, false, RENDER_PASS_MAIN /* solitary = no popups */);
    } else if (!pMonitor->isMirror()) {
        sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW);
        if (pMonitor->m_activeSpecialWorkspace)
            sendFrameEventsToWorkspace(pMonitor, pMonitor->m_activeSpecialWorkspace, NOW);
    }

    renderCursor = renderCursor && shouldRenderCursor();

    if (renderCursor) {
        TRACY_GPU_ZONE("RenderCursor");
        g_pPointerManager->renderSoftwareCursorsFor(pMonitor->m_self.lock(), NOW, g_pHyprOpenGL->m_renderData.damage);
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

    CRegion    frameDamage{g_pHyprOpenGL->m_renderData.damage};

    const auto TRANSFORM = invertTransform(pMonitor->m_transform);
    frameDamage.transform(wlTransformToHyprutils(TRANSFORM), pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y);

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

    auto       colorimetry = settings.primariesNameSet || settings.primaries == SPCPRimaries{} ? getPrimaries(settings.primariesNamed) : settings.primaries;
    auto       luminances  = settings.masteringLuminances.max > 0 ? settings.masteringLuminances :
                                                                          SImageDescription::SPCMasteringLuminances{.min = monitor->minLuminance(), .max = monitor->maxLuminance(10000)};

    Debug::log(TRACE, "ColorManagement primaries {},{} {},{} {},{} {},{}", colorimetry.red.x, colorimetry.red.y, colorimetry.green.x, colorimetry.green.y, colorimetry.blue.x,
                     colorimetry.blue.y, colorimetry.white.x, colorimetry.white.y);
    Debug::log(TRACE, "ColorManagement min {}, max {}, cll {}, fall {}", luminances.min, luminances.max, settings.maxCLL, settings.maxFALL);
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
                      .max_cll                         = toNits(settings.maxCLL),
                      .max_fall                        = toNits(settings.maxFALL),
            },
    };
}

bool CHyprRenderer::commitPendingAndDoExplicitSync(PHLMONITOR pMonitor) {
    static auto PCT        = CConfigValue<Hyprlang::INT>("render:send_content_type");
    static auto PPASS      = CConfigValue<Hyprlang::INT>("render:cm_fs_passthrough");
    static auto PAUTOHDR   = CConfigValue<Hyprlang::INT>("render:cm_auto_hdr");
    static auto PNONSHADER = CConfigValue<Hyprlang::INT>("render:non_shader_cm");

    static bool needsHDRupdate = false;

    const bool  configuredHDR = (pMonitor->m_cmType == NCMType::CM_HDR_EDID || pMonitor->m_cmType == NCMType::CM_HDR);
    bool        wantHDR       = configuredHDR;

    const auto  FS_WINDOW = pMonitor->inFullscreenMode() ? pMonitor->m_activeWorkspace->getFullscreenWindow() : nullptr;

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
            const auto ROOT_SURF = FS_WINDOW->m_wlSurface->resource();
            const auto SURF      = ROOT_SURF->findWithCM();

            // we have a surface with image description
            if (SURF && SURF->m_colorManagement.valid() && SURF->m_colorManagement->hasImageDescription()) {
                const bool surfaceIsHDR = SURF->m_colorManagement->isHDR();
                if (!SURF->m_colorManagement->isWindowsScRGB() && (*PPASS == 1 || ((*PPASS == 2 || !pMonitor->m_lastScanout.expired()) && surfaceIsHDR))) {
                    // passthrough
                    bool needsHdrMetadataUpdate = SURF->m_colorManagement->needsHdrMetadataUpdate() || pMonitor->m_previousFSWindow != FS_WINDOW || needsHDRupdate;
                    if (SURF->m_colorManagement->needsHdrMetadataUpdate()) {
                        Debug::log(INFO, "[CM] Recreating HDR metadata for surface");
                        SURF->m_colorManagement->setHDRMetadata(createHDRMetadata(SURF->m_colorManagement->imageDescription(), pMonitor));
                    }
                    if (needsHdrMetadataUpdate) {
                        Debug::log(INFO, "[CM] Updating HDR metadata from surface");
                        pMonitor->m_output->state->setHDRMetadata(SURF->m_colorManagement->hdrMetadata());
                    }
                    hdrIsHandled   = true;
                    needsHDRupdate = false;
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
                    Debug::log(INFO, "[CM] Auto HDR: changing monitor cm to {}", sc<uint8_t>(targetCM));
                    pMonitor->applyCMType(targetCM, targetSDREOTF);
                    pMonitor->m_previousFSWindow.reset(); // trigger CTM update
                }
                Debug::log(INFO, wantHDR ? "[CM] Updating HDR metadata from monitor" : "[CM] Restoring SDR mode");
                pMonitor->m_output->state->setHDRMetadata(wantHDR ? createHDRMetadata(pMonitor->m_imageDescription, pMonitor) : NO_HDR_METADATA);
            }
            needsHDRupdate = true;
        }
    }

    const bool needsWCG = pMonitor->wantsWideColor();
    if (pMonitor->m_output->state->state().wideColorGamut != needsWCG) {
        Debug::log(TRACE, "Setting wide color gamut {}", needsWCG ? "on" : "off");
        pMonitor->m_output->state->setWideColorGamut(needsWCG);

        // FIXME do not trust enabled10bit, auto switch to 10bit and back if needed
        if (needsWCG && !pMonitor->m_enabled10bit) {
            Debug::log(WARN, "Wide color gamut is enabled but the display is not in 10bit mode");
            static bool shown = false;
            if (!shown) {
                g_pHyprNotificationOverlay->addNotification("Wide color gamut is enabled but the display is not in 10bit mode", CHyprColor{}, 15000, ICON_WARNING);
                shown = true;
            }
        }
    }

    if (*PCT)
        pMonitor->m_output->state->setContentType(NContentType::toDRM(FS_WINDOW ? FS_WINDOW->getContentType() : CONTENT_TYPE_NONE));

    if (FS_WINDOW != pMonitor->m_previousFSWindow) {
        if (*PNONSHADER == CM_NS_IGNORE || !FS_WINDOW || !pMonitor->needsCM() || !pMonitor->canNoShaderCM() ||
            (*PNONSHADER == CM_NS_ONDEMAND && pMonitor->m_lastScanout.expired() && *PPASS != 1)) {
            if (pMonitor->m_noShaderCTM) {
                Debug::log(INFO, "[CM] No fullscreen CTM, restoring previous one");
                pMonitor->m_noShaderCTM = false;
                pMonitor->m_ctmUpdated  = true;
            }
        } else {
            const auto FS_DESC = pMonitor->getFSImageDescription();
            if (FS_DESC.has_value()) {
                Debug::log(INFO, "[CM] Updating fullscreen CTM");
                pMonitor->m_noShaderCTM        = true;
                const auto                 mat = FS_DESC->getPrimaries().convertMatrix(pMonitor->m_imageDescription.getPrimaries()).mat();
                const std::array<float, 9> CTM = {
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
            Debug::log(TRACE, "Monitor state commit failed, retrying without a fence");
            pMonitor->m_output->state->resetExplicitFences();
            ok = pMonitor->m_state.commit();
        }

        if (!ok) {
            Debug::log(TRACE, "Monitor state commit failed");
            // rollback the buffer to avoid writing to the front buffer that is being
            // displayed
            pMonitor->m_output->swapchain->rollback();
            pMonitor->m_damage.damageEntire();
        }
    }

    return ok;
}

void CHyprRenderer::renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry) {
    Vector2D translate = {geometry.x, geometry.y};
    float    scale     = sc<float>(geometry.width) / pMonitor->m_pixelSize.x;

    TRACY_GPU_ZONE("RenderWorkspace");

    if (!DELTALESSTHAN(sc<double>(geometry.width) / sc<double>(geometry.height), pMonitor->m_pixelSize.x / pMonitor->m_pixelSize.y, 0.01)) {
        Debug::log(ERR, "Ignoring geometry in renderWorkspace: aspect ratio mismatch");
        scale     = 1.f;
        translate = Vector2D{};
    }

    renderAllClientsForWorkspace(pMonitor, pWorkspace, now, translate, scale);
}

void CHyprRenderer::sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now) {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped || w->m_fadingOut || !w->m_wlSurface->resource())
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        w->m_wlSurface->resource()->breadthfirst([now](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { r->frame(now); }, nullptr);
    }

    for (auto const& lsl : pMonitor->m_layerSurfaceLayers) {
        for (auto const& ls : lsl) {
            if (ls->m_fadingOut || !ls->m_surface->resource())
                continue;

            ls->m_surface->resource()->breadthfirst([now](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { r->frame(now); }, nullptr);
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
            Debug::log(ERR, "LayerSurface {:x} has a negative/zero w/h???", rc<uintptr_t>(ls.get()));
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

void CHyprRenderer::arrangeLayersForMonitor(const MONITORID& monitor) {
    const auto  PMONITOR = g_pCompositor->getMonitorFromID(monitor);

    static auto BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");

    if (!PMONITOR)
        return;

    // Reset the reserved
    PMONITOR->m_reservedBottomRight = Vector2D();
    PMONITOR->m_reservedTopLeft     = Vector2D();

    CBox usableArea = {PMONITOR->m_position.x, PMONITOR->m_position.y, PMONITOR->m_size.x, PMONITOR->m_size.y};

    if (g_pHyprError->active() && g_pCompositor->m_lastMonitor == PMONITOR->m_self) {
        const auto HEIGHT = g_pHyprError->height();
        if (*BAR_POSITION == 0) {
            PMONITOR->m_reservedTopLeft.y = HEIGHT;
            usableArea.y += HEIGHT;
            usableArea.h -= HEIGHT;
        } else {
            PMONITOR->m_reservedBottomRight.y = HEIGHT;
            usableArea.h -= HEIGHT;
        }
    }

    for (auto& la : PMONITOR->m_layerSurfaceLayers) {
        std::ranges::stable_sort(la, [](const PHLLSREF& a, const PHLLSREF& b) { return a->m_order > b->m_order; });
    }

    for (auto const& la : PMONITOR->m_layerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, true, &usableArea);

    for (auto const& la : PMONITOR->m_layerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, false, &usableArea);

    PMONITOR->m_reservedTopLeft     = Vector2D(usableArea.x, usableArea.y) - PMONITOR->m_position;
    PMONITOR->m_reservedBottomRight = PMONITOR->m_size - Vector2D(usableArea.width, usableArea.height) - PMONITOR->m_reservedTopLeft;

    auto ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(PMONITOR->m_name);
    if (ADDITIONALRESERVED == g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(""); // glob wildcard
    }

    if (ADDITIONALRESERVED != g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        PMONITOR->m_reservedTopLeft     = PMONITOR->m_reservedTopLeft + Vector2D(ADDITIONALRESERVED->second.left, ADDITIONALRESERVED->second.top);
        PMONITOR->m_reservedBottomRight = PMONITOR->m_reservedBottomRight + Vector2D(ADDITIONALRESERVED->second.right, ADDITIONALRESERVED->second.bottom);
    }

    // damage the monitor if can
    damageMonitor(PMONITOR);

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitor);
}

void CHyprRenderer::damageSurface(SP<CWLSurfaceResource> pSurface, double x, double y, double scale) {
    if (!pSurface)
        return; // wut?

    if (g_pCompositor->m_unsafeState)
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

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!m->m_output)
            continue;

        damageBoxForEach.set(damageBox);
        damageBoxForEach.translate({-m->m_position.x, -m->m_position.y}).scale(m->m_scale);

        m->addDamage(damageBoxForEach);
    }

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Surface (extents): xy: {}, {} wh: {}, {}", damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y1,
                   damageBox.pixman()->extents.x2 - damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y2 - damageBox.pixman()->extents.y1);
}

void CHyprRenderer::damageWindow(PHLWINDOW pWindow, bool forceFull) {
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
        Debug::log(LOG, "Damage: Window ({}): xy: {}, {} wh: {}, {}", pWindow->m_title, windowBox.x, windowBox.y, windowBox.width, windowBox.height);
}

void CHyprRenderer::damageMonitor(PHLMONITOR pMonitor) {
    if (g_pCompositor->m_unsafeState || pMonitor->isMirror())
        return;

    CBox damageBox = {0, 0, INT16_MAX, INT16_MAX};
    pMonitor->addDamage(damageBox);

    static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Monitor {}", pMonitor->m_name);
}

void CHyprRenderer::damageBox(const CBox& box, bool skipFrameSchedule) {
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
        Debug::log(LOG, "Damage: Box: xy: {}, {} wh: {}, {}", box.x, box.y, box.w, box.h);
}

void CHyprRenderer::damageBox(const int& x, const int& y, const int& w, const int& h) {
    CBox box = {x, y, w, h};
    damageBox(box);
}

void CHyprRenderer::damageRegion(const CRegion& rg) {
    rg.forEachRect([this](const auto& RECT) { damageBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1); });
}

void CHyprRenderer::damageMirrorsWith(PHLMONITOR pMonitor, const CRegion& pRegion) {
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
        transformed.transform(wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_pixelSize.x * scale, pMonitor->m_pixelSize.y * scale);
        transformed.translate(Vector2D(monbox.x, monbox.y));

        mirror->addDamage(transformed);

        g_pCompositor->scheduleFrameForMonitor(mirror.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }
}

void CHyprRenderer::renderDragIcon(PHLMONITOR pMonitor, const Time::steady_tp& time) {
    PROTO::data->renderDND(pMonitor, time);
}

void CHyprRenderer::setCursorSurface(SP<CWLSurface> surf, int hotspotX, int hotspotY, bool force) {
    m_cursorHasSurface = surf;

    m_lastCursorData.name     = "";
    m_lastCursorData.surf     = surf;
    m_lastCursorData.hotspotX = hotspotX;
    m_lastCursorData.hotspotY = hotspotY;

    if (m_cursorHidden && !force)
        return;

    g_pCursorManager->setCursorSurface(surf, {hotspotX, hotspotY});
}

void CHyprRenderer::setCursorFromName(const std::string& name, bool force) {
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

void CHyprRenderer::ensureCursorRenderingMode() {
    static auto PINVISIBLE     = CConfigValue<Hyprlang::INT>("cursor:invisible");
    static auto PCURSORTIMEOUT = CConfigValue<Hyprlang::FLOAT>("cursor:inactive_timeout");
    static auto PHIDEONTOUCH   = CConfigValue<Hyprlang::INT>("cursor:hide_on_touch");
    static auto PHIDEONKEY     = CConfigValue<Hyprlang::INT>("cursor:hide_on_key_press");

    if (*PCURSORTIMEOUT <= 0)
        m_cursorHiddenConditions.hiddenOnTimeout = false;
    if (*PHIDEONTOUCH == 0)
        m_cursorHiddenConditions.hiddenOnTouch = false;
    if (*PHIDEONKEY == 0)
        m_cursorHiddenConditions.hiddenOnKeyboard = false;

    if (*PCURSORTIMEOUT > 0)
        m_cursorHiddenConditions.hiddenOnTimeout = *PCURSORTIMEOUT < g_pInputManager->m_lastCursorMovement.getSeconds();

    m_cursorHiddenByCondition = m_cursorHiddenConditions.hiddenOnTimeout || m_cursorHiddenConditions.hiddenOnTouch || m_cursorHiddenConditions.hiddenOnKeyboard;

    const bool HIDE = m_cursorHiddenByCondition || (*PINVISIBLE != 0);

    if (HIDE == m_cursorHidden)
        return;

    if (HIDE) {
        Debug::log(LOG, "Hiding the cursor (hl-mandated)");

        for (auto const& m : g_pCompositor->m_monitors) {
            if (!g_pPointerManager->softwareLockedFor(m))
                continue;

            damageMonitor(m); // TODO: maybe just damage the cursor area?
        }

        setCursorHidden(true);

    } else {
        Debug::log(LOG, "Showing the cursor (hl-mandated)");

        for (auto const& m : g_pCompositor->m_monitors) {
            if (!g_pPointerManager->softwareLockedFor(m))
                continue;

            damageMonitor(m); // TODO: maybe just damage the cursor area?
        }

        setCursorHidden(false);
    }
}

void CHyprRenderer::setCursorHidden(bool hide) {

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

bool CHyprRenderer::shouldRenderCursor() {
    return !m_cursorHidden && m_cursorHasSurface;
}

std::tuple<float, float, float> CHyprRenderer::getRenderTimes(PHLMONITOR pMonitor) {
    const auto POVERLAY = &g_pDebugOverlay->m_monitorOverlays[pMonitor];

    float      avgRenderTime = 0;
    float      maxRenderTime = 0;
    float      minRenderTime = 9999;
    for (auto const& rt : POVERLAY->m_lastRenderTimes) {
        if (rt > maxRenderTime)
            maxRenderTime = rt;
        if (rt < minRenderTime)
            minRenderTime = rt;
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

void CHyprRenderer::initiateManualCrash() {
    g_pHyprNotificationOverlay->addNotification("Manual crash initiated. Farewell...", CHyprColor(0), 5000, ICON_INFO);

    m_crashingLoop = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_crashingLoop, 1000);

    m_crashingInProgress = true;
    m_crashingDistort    = 0.5;

    g_pHyprOpenGL->m_globalTimer.reset();

    static auto PDT = rc<Hyprlang::INT* const*>(g_pConfigManager->getConfigValuePtr("debug:damage_tracking"));

    **PDT = 0;
}

SP<CRenderbuffer> CHyprRenderer::getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto it = std::ranges::find_if(m_renderbuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });

    if (it != m_renderbuffers.end())
        return *it;

    auto buf = makeShared<CRenderbuffer>(buffer, fmt);

    if (!buf->good())
        return nullptr;

    m_renderbuffers.emplace_back(buf);
    return buf;
}

void CHyprRenderer::makeEGLCurrent() {
    if (!g_pCompositor || !g_pHyprOpenGL)
        return;

    if (eglGetCurrentContext() != g_pHyprOpenGL->m_eglContext)
        eglMakeCurrent(g_pHyprOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, g_pHyprOpenGL->m_eglContext);
}

void CHyprRenderer::unsetEGL() {
    if (!g_pHyprOpenGL)
        return;

    eglMakeCurrent(g_pHyprOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool CHyprRenderer::beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, CFramebuffer* fb, bool simple) {

    makeEGLCurrent();

    m_renderPass.clear();

    m_renderMode = mode;

    g_pHyprOpenGL->m_renderData.pMonitor = pMonitor; // has to be set cuz allocs

    if (mode == RENDER_MODE_FULL_FAKE) {
        RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
        fb->bind();
        if (simple)
            g_pHyprOpenGL->beginSimple(pMonitor, damage, nullptr, fb);
        else
            g_pHyprOpenGL->begin(pMonitor, damage, fb);
        return true;
    }

    int bufferAge = 0;

    if (!buffer) {
        m_currentBuffer = pMonitor->m_output->swapchain->next(&bufferAge);
        if (!m_currentBuffer) {
            Debug::log(ERR, "Failed to acquire swapchain buffer for {}", pMonitor->m_name);
            return false;
        }
    } else
        m_currentBuffer = buffer;

    try {
        m_currentRenderbuffer = getOrCreateRenderbuffer(m_currentBuffer, pMonitor->m_output->state->state().drmFormat);
    } catch (std::exception& e) {
        Debug::log(ERR, "getOrCreateRenderbuffer failed for {}", pMonitor->m_name);
        return false;
    }

    if (!m_currentRenderbuffer) {
        Debug::log(ERR, "failed to start a render pass for output {}, no RBO could be obtained", pMonitor->m_name);
        return false;
    }

    if (mode == RENDER_MODE_NORMAL) {
        damage = pMonitor->m_damage.getBufferDamage(bufferAge);
        pMonitor->m_damage.rotate();
    }

    m_currentRenderbuffer->bind();
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, m_currentRenderbuffer);
    else
        g_pHyprOpenGL->begin(pMonitor, damage);

    return true;
}

void CHyprRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    const auto  PMONITOR           = g_pHyprOpenGL->m_renderData.pMonitor;
    static auto PNVIDIAANTIFLICKER = CConfigValue<Hyprlang::INT>("opengl:nvidia_anti_flicker");

    g_pHyprOpenGL->m_renderData.damage = m_renderPass.render(g_pHyprOpenGL->m_renderData.damage);

    auto cleanup = CScopeGuard([this]() {
        if (m_currentRenderbuffer)
            m_currentRenderbuffer->unbind();
        m_currentRenderbuffer = nullptr;
        m_currentBuffer       = nullptr;
    });

    if (m_renderMode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pHyprOpenGL->end();
    else {
        g_pHyprOpenGL->m_renderData.pMonitor.reset();
        g_pHyprOpenGL->m_renderData.mouseZoomFactor   = 1.f;
        g_pHyprOpenGL->m_renderData.mouseZoomUseMouse = true;
    }

    if (m_renderMode == RENDER_MODE_FULL_FAKE)
        return;

    if (m_renderMode == RENDER_MODE_NORMAL)
        PMONITOR->m_output->state->setBuffer(m_currentBuffer);

    if (!g_pHyprOpenGL->explicitSyncSupported()) {
        Debug::log(TRACE, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        // nvidia doesn't have implicit sync, so we have to explicitly wait here, llvmpipe and other software renderer seems to bug out aswell.
        if ((isNvidia() && *PNVIDIAANTIFLICKER) || isSoftware())
            glFinish();
        else
            glFlush(); // mark an implicit sync point

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();

        return;
    }

    UP<CEGLSync> eglSync = CEGLSync::create();
    if (eglSync && eglSync->isValid()) {
        for (auto const& buf : m_usedAsyncBuffers) {
            for (const auto& releaser : buf->m_syncReleasers) {
                releaser->addSyncFileFd(eglSync->fd());
            }
        }

        // release buffer refs with release points now, since syncReleaser handles actual buffer release based on EGLSync
        std::erase_if(m_usedAsyncBuffers, [](const auto& buf) { return !buf->m_syncReleasers.empty(); });

        // release buffer refs without release points when EGLSync sync_file/fence is signalled
        g_pEventLoopManager->doOnReadable(eglSync->fd().duplicate(), [renderingDoneCallback, prevbfs = std::move(m_usedAsyncBuffers)]() mutable {
            prevbfs.clear();
            if (renderingDoneCallback)
                renderingDoneCallback();
        });
        m_usedAsyncBuffers.clear();

        if (m_renderMode == RENDER_MODE_NORMAL) {
            PMONITOR->m_inFence = eglSync->takeFd();
            PMONITOR->m_output->state->setExplicitInFence(PMONITOR->m_inFence.get());
        }
    } else {
        Debug::log(ERR, "renderer: Explicit sync failed, releasing resources");

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();
    }
}

void CHyprRenderer::onRenderbufferDestroy(CRenderbuffer* rb) {
    std::erase_if(m_renderbuffers, [&](const auto& rbo) { return rbo.get() == rb; });
}

SP<CRenderbuffer> CHyprRenderer::getCurrentRBO() {
    return m_currentRenderbuffer;
}

bool CHyprRenderer::isNvidia() {
    return m_nvidia;
}

bool CHyprRenderer::isIntel() {
    return m_intel;
}

bool CHyprRenderer::isSoftware() {
    return m_software;
}

bool CHyprRenderer::isMgpu() {
    return m_mgpu;
}

void CHyprRenderer::addWindowToRenderUnfocused(PHLWINDOW window) {
    static auto PFPS = CConfigValue<Hyprlang::INT>("misc:render_unfocused_fps");

    if (std::ranges::find(m_renderUnfocused, window) != m_renderUnfocused.end())
        return;

    m_renderUnfocused.emplace_back(window);

    if (!m_renderUnfocusedTimer->armed())
        m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));
}

void CHyprRenderer::makeSnapshot(PHLWINDOW pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    if (!shouldRenderWindow(pWindow))
        return; // ignore, window is not being rendered

    Debug::log(LOG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(pWindow.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion      fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    PHLWINDOWREF ref{pWindow};

    makeEGLCurrent();

    const auto PFRAMEBUFFER = &g_pHyprOpenGL->m_windowFramebuffers[ref];

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    renderWindow(pWindow, PMONITOR, Time::steadyNow(), !pWindow->m_X11DoesntWantBorders, RENDER_PASS_ALL);

    endRender();

    m_bRenderingSnapshot = false;
}

void CHyprRenderer::makeSnapshot(PHLLS pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = pLayer->m_monitor.lock();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    Debug::log(LOG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(pLayer.get()));

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesn't mess with the actual damage
    CRegion fakeDamage{0, 0, sc<int>(PMONITOR->m_transformedSize.x), sc<int>(PMONITOR->m_transformedSize.y)};

    makeEGLCurrent();

    const auto PFRAMEBUFFER = &g_pHyprOpenGL->m_layerFramebuffers[pLayer];

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    m_bRenderingSnapshot = true;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // JIC

    // draw the layer
    renderLayer(pLayer, PMONITOR, Time::steadyNow());

    endRender();

    m_bRenderingSnapshot = false;
}

void CHyprRenderer::makeSnapshot(WP<CPopup> popup) {
    // we trust the window is valid.
    const auto PMONITOR = popup->getMonitor();

    if (!PMONITOR || !PMONITOR->m_output || PMONITOR->m_pixelSize.x <= 0 || PMONITOR->m_pixelSize.y <= 0)
        return;

    if (!popup->m_wlSurface || !popup->m_wlSurface->resource() || !popup->m_mapped)
        return;

    Debug::log(LOG, "renderer: making a snapshot of {:x}", rc<uintptr_t>(popup.get()));

    CRegion fakeDamage{0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};

    makeEGLCurrent();

    const auto PFRAMEBUFFER = &g_pHyprOpenGL->m_popupFramebuffers[popup];

    PFRAMEBUFFER->alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, DRM_FORMAT_ABGR8888);

    beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

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

    popup->m_wlSurface->resource()->breadthfirst(
        [this, &renderdata](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
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

void CHyprRenderer::renderSnapshot(PHLWINDOW pWindow) {
    static auto  PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    PHLWINDOWREF ref{pWindow};

    if (!g_pHyprOpenGL->m_windowFramebuffers.contains(ref))
        return;

    const auto FBDATA = &g_pHyprOpenGL->m_windowFramebuffers.at(ref);

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

    if (*PDIMAROUND && pWindow->m_windowData.dimAround.valueOrDefault()) {
        CRectPassElement::SRectData data;

        data.box   = {0, 0, g_pHyprOpenGL->m_renderData.pMonitor->m_pixelSize.x, g_pHyprOpenGL->m_renderData.pMonitor->m_pixelSize.y};
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
        data.xray          = pWindow->m_windowData.xray.valueOr(false);

        m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));
    }

    CTexPassElement::SRenderData data;
    data.flipEndFrame = true;
    data.tex          = FBDATA->getTexture();
    data.box          = windowBox;
    data.a            = pWindow->m_alpha->value();
    data.damage       = fakeDamage;

    m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void CHyprRenderer::renderSnapshot(PHLLS pLayer) {
    if (!g_pHyprOpenGL->m_layerFramebuffers.contains(pLayer))
        return;

    const auto FBDATA = &g_pHyprOpenGL->m_layerFramebuffers.at(pLayer);

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
        data.ignoreAlpha = pLayer->m_ignoreAlpha ? pLayer->m_ignoreAlphaValue : 0.01F /* ignore the alpha 0 regions */;

    m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void CHyprRenderer::renderSnapshot(WP<CPopup> popup) {
    if (!g_pHyprOpenGL->m_popupFramebuffers.contains(popup))
        return;

    static CConfigValue PBLURIGNOREA = CConfigValue<Hyprlang::FLOAT>("decoration:blur:popups_ignorealpha");

    const auto          FBDATA = &g_pHyprOpenGL->m_popupFramebuffers.at(popup);

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

bool CHyprRenderer::shouldBlur(PHLLS ls) {
    if (m_bRenderingSnapshot)
        return false;

    static auto PBLUR = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    return *PBLUR && ls->m_forceBlur;
}

bool CHyprRenderer::shouldBlur(PHLWINDOW w) {
    if (m_bRenderingSnapshot)
        return false;

    static auto PBLUR     = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    const bool  DONT_BLUR = w->m_windowData.noBlur.valueOrDefault() || w->m_windowData.RGBX.valueOrDefault() || w->opaque();
    return *PBLUR && !DONT_BLUR;
}

bool CHyprRenderer::shouldBlur(WP<CPopup> p) {
    static CConfigValue PBLURPOPUPS = CConfigValue<Hyprlang::INT>("decoration:blur:popups");
    static CConfigValue PBLUR       = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    return *PBLURPOPUPS && *PBLUR;
}
