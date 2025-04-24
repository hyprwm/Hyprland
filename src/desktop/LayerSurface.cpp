#include "LayerSurface.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/AnimationManager.hpp"
#include "../render/Renderer.hpp"
#include "../config/ConfigManager.hpp"
#include "../helpers/Monitor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/EventManager.hpp"

PHLLS CLayerSurface::create(SP<CLayerShellResource> resource) {
    PHLLS pLS = SP<CLayerSurface>(new CLayerSurface(resource));

    auto  pMonitor = resource->monitor.empty() ? g_pCompositor->m_lastMonitor.lock() : g_pCompositor->getMonitorFromName(resource->monitor);

    pLS->m_surface->assign(resource->surface.lock(), pLS);

    if (!pMonitor) {
        Debug::log(ERR, "New LS has no monitor??");
        return pLS;
    }

    if (pMonitor->pMirrorOf)
        pMonitor = g_pCompositor->m_monitors.front();

    pLS->m_self = pLS;

    pLS->m_namespace = resource->layerNamespace;

    pLS->m_layer     = resource->current.layer;
    pLS->m_popupHead = CPopup::create(pLS);
    pLS->m_monitor   = pMonitor;
    pMonitor->m_aLayerSurfaceLayers[resource->current.layer].emplace_back(pLS);

    pLS->m_forceBlur = g_pConfigManager->shouldBlurLS(pLS->m_namespace);

    g_pAnimationManager->createAnimation(0.f, pLS->m_alpha, g_pConfigManager->getAnimationPropertyConfig("fadeLayersIn"), pLS, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pLS->m_realPosition, g_pConfigManager->getAnimationPropertyConfig("layersIn"), pLS, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pLS->m_realSize, g_pConfigManager->getAnimationPropertyConfig("layersIn"), pLS, AVARDAMAGE_ENTIRE);

    pLS->registerCallbacks();

    pLS->m_alpha->setValueAndWarp(0.f);

    Debug::log(LOG, "LayerSurface {:x} (namespace {} layer {}) created on monitor {}", (uintptr_t)resource.get(), resource->layerNamespace, (int)pLS->m_layer, pMonitor->szName);

    return pLS;
}

void CLayerSurface::registerCallbacks() {
    m_alpha->setUpdateCallback([this](auto) {
        if (m_dimAround)
            g_pHyprRenderer->damageMonitor(m_monitor.lock());
    });
}

CLayerSurface::CLayerSurface(SP<CLayerShellResource> resource_) : m_layerSurface(resource_) {
    m_listeners.commit  = m_layerSurface->events.commit.registerListener([this](std::any d) { onCommit(); });
    m_listeners.map     = m_layerSurface->events.map.registerListener([this](std::any d) { onMap(); });
    m_listeners.unmap   = m_layerSurface->events.unmap.registerListener([this](std::any d) { onUnmap(); });
    m_listeners.destroy = m_layerSurface->events.destroy.registerListener([this](std::any d) { onDestroy(); });

    m_surface = CWLSurface::create();
}

CLayerSurface::~CLayerSurface() {
    if (!g_pHyprOpenGL)
        return;

    if (m_surface)
        m_surface->unassign();
    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mLayerFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.lock() == m_self.lock(); });

    for (auto const& mon : g_pCompositor->m_realMonitors) {
        for (auto& lsl : mon->m_aLayerSurfaceLayers) {
            std::erase_if(lsl, [this](auto& ls) { return ls.expired() || ls.get() == this; });
        }
    }
}

void CLayerSurface::onDestroy() {
    Debug::log(LOG, "LayerSurface {:x} destroyed", (uintptr_t)m_layerSurface.get());

    const auto PMONITOR = m_monitor.lock();

    if (!PMONITOR)
        Debug::log(WARN, "Layersurface destroyed on an invalid monitor (removed?)");

    if (!m_fadingOut) {
        if (m_mapped) {
            Debug::log(LOG, "Forcing an unmap of a LS that did a straight destroy!");
            onUnmap();
        } else {
            Debug::log(LOG, "Removing LayerSurface that wasn't mapped.");
            if (m_alpha)
                m_alpha->setValueAndWarp(0.f);
            m_fadingOut = true;
            g_pCompositor->addToFadingOutSafe(m_self.lock());
        }
    }

    m_popupHead.reset();

    m_noProcess = true;

    // rearrange to fix the reserved areas
    if (PMONITOR) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
        PMONITOR->scheduledRecalc = true;

        // and damage
        CBox geomFixed = {m_geometry.x + PMONITOR->vecPosition.x, m_geometry.y + PMONITOR->vecPosition.y, m_geometry.width, m_geometry.height};
        g_pHyprRenderer->damageBox(geomFixed);
    }

    m_readyToDelete = true;
    m_layerSurface.reset();
    if (m_surface)
        m_surface->unassign();

    m_listeners.unmap.reset();
    m_listeners.destroy.reset();
    m_listeners.map.reset();
    m_listeners.commit.reset();
}

void CLayerSurface::onMap() {
    Debug::log(LOG, "LayerSurface {:x} mapped", (uintptr_t)m_layerSurface.get());

    m_mapped        = true;
    m_interactivity = m_layerSurface->current.interactivity;

    m_layerSurface->surface->map();

    // this layer might be re-mapped.
    m_fadingOut = false;
    g_pCompositor->removeFromFadingOutSafe(m_self.lock());

    // fix if it changed its mon
    const auto PMONITOR = m_monitor.lock();

    if (!PMONITOR)
        return;

    applyRules();

    PMONITOR->scheduledRecalc = true;

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    m_surface->resource()->enter(PMONITOR->self.lock());

    const bool ISEXCLUSIVE = m_layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;

    if (ISEXCLUSIVE)
        g_pInputManager->m_dExclusiveLSes.push_back(m_self);

    const bool GRABSFOCUS = ISEXCLUSIVE ||
        (m_layerSurface->current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
         // don't focus if constrained
         (g_pSeatManager->mouse.expired() || !g_pInputManager->isConstrained()));

    if (GRABSFOCUS) {
        // TODO: use the new superb really very cool grab
        g_pSeatManager->setGrab(nullptr);
        g_pInputManager->releaseAllMouseButtons();
        g_pCompositor->focusSurface(m_surface->resource());

        const auto LOCAL = g_pInputManager->getMouseCoordsInternal() - Vector2D(m_geometry.x + PMONITOR->vecPosition.x, m_geometry.y + PMONITOR->vecPosition.y);
        g_pSeatManager->setPointerFocus(m_surface->resource(), LOCAL);
        g_pInputManager->m_bEmptyFocusCursorSet = false;
    }

    m_position = Vector2D(m_geometry.x, m_geometry.y);

    CBox geomFixed = {m_geometry.x + PMONITOR->vecPosition.x, m_geometry.y + PMONITOR->vecPosition.y, m_geometry.width, m_geometry.height};
    g_pHyprRenderer->damageBox(geomFixed);
    const bool FULLSCREEN = PMONITOR->activeWorkspace && PMONITOR->activeWorkspace->m_bHasFullscreenWindow && PMONITOR->activeWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN;

    startAnimation(!(m_layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP && FULLSCREEN && !GRABSFOCUS));
    m_readyToDelete = false;
    m_fadingOut     = false;

    g_pEventManager->postEvent(SHyprIPCEvent{"openlayer", m_namespace});
    EMIT_HOOK_EVENT("openLayer", m_self.lock());

    g_pCompositor->setPreferredScaleForSurface(m_surface->resource(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(m_surface->resource(), PMONITOR->transform);
}

void CLayerSurface::onUnmap() {
    Debug::log(LOG, "LayerSurface {:x} unmapped", (uintptr_t)m_layerSurface.get());

    g_pEventManager->postEvent(SHyprIPCEvent{"closelayer", m_layerSurface->layerNamespace});
    EMIT_HOOK_EVENT("closeLayer", m_self.lock());

    std::erase_if(g_pInputManager->m_dExclusiveLSes, [this](const auto& other) { return !other.lock() || other.lock() == m_self.lock(); });

    if (!m_monitor || g_pCompositor->m_unsafeState) {
        Debug::log(WARN, "Layersurface unmapping on invalid monitor (removed?) ignoring.");

        g_pCompositor->addToFadingOutSafe(m_self.lock());

        m_mapped = false;
        if (m_layerSurface && m_layerSurface->surface)
            m_layerSurface->surface->unmap();

        startAnimation(false);
        return;
    }

    // end any pending animations so that snapshot has right dimensions
    m_realPosition->warp();
    m_realSize->warp();

    // make a snapshot and start fade
    g_pHyprRenderer->makeLayerSnapshot(m_self.lock());

    startAnimation(false);

    m_mapped = false;
    if (m_layerSurface && m_layerSurface->surface)
        m_layerSurface->surface->unmap();

    g_pCompositor->addToFadingOutSafe(m_self.lock());

    const auto PMONITOR = m_monitor.lock();

    const bool WASLASTFOCUS = g_pSeatManager->state.keyboardFocus == m_surface->resource() || g_pSeatManager->state.pointerFocus == m_surface->resource();

    if (!PMONITOR)
        return;

    // refocus if needed
    //                                vvvvvvvvvvvvv if there is a last focus and the last focus is not keyboard focusable, fallback to window
    if (WASLASTFOCUS || (g_pCompositor->m_lastFocus && g_pCompositor->m_lastFocus->hlSurface && !g_pCompositor->m_lastFocus->hlSurface->keyboardFocusable())) {
        if (!g_pInputManager->refocusLastWindow(PMONITOR))
            g_pInputManager->refocus();
    } else if (g_pCompositor->m_lastFocus && g_pCompositor->m_lastFocus != m_surface->resource())
        g_pSeatManager->setKeyboardFocus(g_pCompositor->m_lastFocus.lock());

    CBox geomFixed = {m_geometry.x + PMONITOR->vecPosition.x, m_geometry.y + PMONITOR->vecPosition.y, m_geometry.width, m_geometry.height};
    g_pHyprRenderer->damageBox(geomFixed);

    geomFixed = {m_geometry.x + (int)PMONITOR->vecPosition.x, m_geometry.y + (int)PMONITOR->vecPosition.y, (int)m_layerSurface->surface->current.size.x,
                 (int)m_layerSurface->surface->current.size.y};
    g_pHyprRenderer->damageBox(geomFixed);

    g_pInputManager->simulateMouseMovement();

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
}

void CLayerSurface::onCommit() {
    if (!m_layerSurface)
        return;

    if (!m_mapped) {
        // we're re-mapping if this is the case
        if (m_layerSurface->surface && !m_layerSurface->surface->current.texture) {
            m_fadingOut = false;
            m_geometry  = {};
            g_pHyprRenderer->arrangeLayersForMonitor(monitorID());
        }

        return;
    }

    const auto PMONITOR = m_monitor.lock();

    if (!PMONITOR)
        return;

    if (m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd

    CBox geomFixed = {m_geometry.x, m_geometry.y, m_geometry.width, m_geometry.height};
    g_pHyprRenderer->damageBox(geomFixed);

    if (m_layerSurface->current.committed != 0) {
        if (m_layerSurface->current.committed & CLayerShellResource::eCommittedState::STATE_LAYER) {

            for (auto it = PMONITOR->m_aLayerSurfaceLayers[m_layer].begin(); it != PMONITOR->m_aLayerSurfaceLayers[m_layer].end(); it++) {
                if (*it == m_self) {
                    if (m_layerSurface->current.layer == m_layer)
                        break;
                    PMONITOR->m_aLayerSurfaceLayers[m_layerSurface->current.layer].emplace_back(*it);
                    PMONITOR->m_aLayerSurfaceLayers[m_layer].erase(it);
                    break;
                }
            }

            m_layer = m_layerSurface->current.layer;

            if (m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
                g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd
        }

        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

        PMONITOR->scheduledRecalc = true;
    } else {
        m_position = Vector2D(m_geometry.x, m_geometry.y);

        // update geom if it changed
        if (m_layerSurface->surface->current.scale == 1 && PMONITOR->scale != 1.f && m_layerSurface->surface->current.viewport.hasDestination) {
            // fractional scaling. Dirty hack.
            m_geometry = {m_geometry.pos(), m_layerSurface->surface->current.viewport.destination};
        } else {
            // this is because some apps like e.g. rofi-lbonn can't fucking use the protocol correctly.
            m_geometry = {m_geometry.pos(), m_layerSurface->surface->current.size};
        }
    }

    if (m_realPosition->goal() != m_geometry.pos()) {
        if (m_realPosition->isBeingAnimated())
            *m_realPosition = m_geometry.pos();
        else
            m_realPosition->setValueAndWarp(m_geometry.pos());
    }
    if (m_realSize->goal() != m_geometry.size()) {
        if (m_realSize->isBeingAnimated())
            *m_realSize = m_geometry.size();
        else
            m_realSize->setValueAndWarp(m_geometry.size());
    }

    if (m_mapped && (m_layerSurface->current.committed & CLayerShellResource::eCommittedState::STATE_INTERACTIVITY)) {
        bool WASLASTFOCUS = false;
        m_layerSurface->surface->breadthfirst(
            [&WASLASTFOCUS](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) { WASLASTFOCUS = WASLASTFOCUS || g_pSeatManager->state.keyboardFocus == surf; },
            nullptr);
        if (!WASLASTFOCUS && m_popupHead) {
            m_popupHead->breadthfirst(
                [&WASLASTFOCUS](WP<CPopup> popup, void* data) {
                    WASLASTFOCUS = WASLASTFOCUS || (popup->m_wlSurface && g_pSeatManager->state.keyboardFocus == popup->m_wlSurface->resource());
                },
                nullptr);
        }
        const bool WASEXCLUSIVE = m_interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
        const bool ISEXCLUSIVE  = m_layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;

        if (!WASEXCLUSIVE && ISEXCLUSIVE)
            g_pInputManager->m_dExclusiveLSes.push_back(m_self);
        else if (WASEXCLUSIVE && !ISEXCLUSIVE)
            std::erase_if(g_pInputManager->m_dExclusiveLSes, [this](const auto& other) { return !other.lock() || other.lock() == m_self.lock(); });

        // if the surface was focused and interactive but now isn't, refocus
        if (WASLASTFOCUS && m_layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
            // moveMouseUnified won't focus non interactive layers but it won't unfocus them either,
            // so unfocus the surface here.
            g_pCompositor->focusSurface(nullptr);
            g_pInputManager->refocusLastWindow(m_monitor.lock());
        } else if (WASLASTFOCUS && WASEXCLUSIVE && m_layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
            g_pInputManager->simulateMouseMovement();
        } else if (!WASEXCLUSIVE && ISEXCLUSIVE) {
            // if now exclusive and not previously
            g_pSeatManager->setGrab(nullptr);
            g_pInputManager->releaseAllMouseButtons();
            g_pCompositor->focusSurface(m_surface->resource());

            const auto LOCAL = g_pInputManager->getMouseCoordsInternal() - Vector2D(m_geometry.x + PMONITOR->vecPosition.x, m_geometry.y + PMONITOR->vecPosition.y);
            g_pSeatManager->setPointerFocus(m_surface->resource(), LOCAL);
            g_pInputManager->m_bEmptyFocusCursorSet = false;
        }
    }

    m_interactivity = m_layerSurface->current.interactivity;

    g_pHyprRenderer->damageSurface(m_surface->resource(), m_position.x, m_position.y);

    g_pCompositor->setPreferredScaleForSurface(m_surface->resource(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(m_surface->resource(), PMONITOR->transform);
}

void CLayerSurface::applyRules() {
    m_noAnimations     = false;
    m_forceBlur        = false;
    m_ignoreAlpha      = false;
    m_ignoreAlphaValue = 0.f;
    m_dimAround        = false;
    m_xray             = -1;
    m_animationStyle.reset();

    for (auto const& rule : g_pConfigManager->getMatchingRules(m_self.lock())) {
        switch (rule->m_ruleType) {
            case CLayerRule::RULE_NOANIM: {
                m_noAnimations = true;
                break;
            }
            case CLayerRule::RULE_BLUR: {
                m_forceBlur = true;
                break;
            }
            case CLayerRule::RULE_BLURPOPUPS: {
                m_forceBlurPopups = true;
                break;
            }
            case CLayerRule::RULE_IGNOREALPHA:
            case CLayerRule::RULE_IGNOREZERO: {
                const auto  FIRST_SPACE_POS = rule->m_rule.find_first_of(' ');
                std::string alphaValue      = "";
                if (FIRST_SPACE_POS != std::string::npos)
                    alphaValue = rule->m_rule.substr(FIRST_SPACE_POS + 1);

                try {
                    m_ignoreAlpha = true;
                    if (!alphaValue.empty())
                        m_ignoreAlphaValue = std::stof(alphaValue);
                } catch (...) { Debug::log(ERR, "Invalid value passed to ignoreAlpha"); }
                break;
            }
            case CLayerRule::RULE_DIMAROUND: {
                m_dimAround = true;
                break;
            }
            case CLayerRule::RULE_XRAY: {
                CVarList vars{rule->m_rule, 0, ' '};
                try {
                    m_xray = configStringToInt(vars[1]).value_or(false);
                } catch (...) {}
                break;
            }
            case CLayerRule::RULE_ANIMATION: {
                CVarList vars{rule->m_rule, 2, 's'};
                m_animationStyle = vars[1];
                break;
            }
            case CLayerRule::RULE_ORDER: {
                CVarList vars{rule->m_rule, 2, 's'};
                try {
                    m_order = std::stoi(vars[1]);
                } catch (...) { Debug::log(ERR, "Invalid value passed to order"); }
                break;
            }
            default: break;
        }
    }
}

void CLayerSurface::startAnimation(bool in, bool instant) {
    if (in) {
        m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersIn"));
        m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersIn"));
        m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeLayersIn"));
    } else {
        m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersOut"));
        m_realSize->setConfig(g_pConfigManager->getAnimationPropertyConfig("layersOut"));
        m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeLayersOut"));
    }

    const auto ANIMSTYLE = m_animationStyle.value_or(m_realPosition->getStyle());
    if (ANIMSTYLE.starts_with("slide")) {
        // get closest edge
        const auto MIDDLE = m_geometry.middle();

        const auto PMONITOR = g_pCompositor->getMonitorFromVector(MIDDLE);

        int        force = -1;

        CVarList   args(ANIMSTYLE, 0, 's');
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
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, 0.0},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{0.0, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x, PMONITOR->vecSize.y / 2},
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

        m_realSize->setValueAndWarp(m_geometry.size());
        m_alpha->setValueAndWarp(in ? 0.f : 1.f);
        *m_alpha = in ? 1.f : 0.f;

        Vector2D prePos;

        switch (leader) {
            case 0:
                // TOP
                prePos = {m_geometry.x, PMONITOR->vecPosition.y - m_geometry.h};
                break;
            case 1:
                // BOTTOM
                prePos = {m_geometry.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y};
                break;
            case 2:
                // LEFT
                prePos = {PMONITOR->vecPosition.x - m_geometry.w, m_geometry.y};
                break;
            case 3:
                // RIGHT
                prePos = {PMONITOR->vecPosition.x + PMONITOR->vecSize.x, m_geometry.y};
                break;
            default: UNREACHABLE();
        }

        if (in) {
            m_realPosition->setValueAndWarp(prePos);
            *m_realPosition = m_geometry.pos();
        } else {
            m_realPosition->setValueAndWarp(m_geometry.pos());
            *m_realPosition = prePos;
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

        const auto GOALSIZE = (m_geometry.size() * minPerc).clamp({5, 5});
        const auto GOALPOS  = m_geometry.pos() + (m_geometry.size() - GOALSIZE) / 2.f;

        m_alpha->setValueAndWarp(in ? 0.f : 1.f);
        *m_alpha = in ? 1.f : 0.f;

        if (in) {
            m_realSize->setValueAndWarp(GOALSIZE);
            m_realPosition->setValueAndWarp(GOALPOS);
            *m_realSize     = m_geometry.size();
            *m_realPosition = m_geometry.pos();
        } else {
            m_realSize->setValueAndWarp(m_geometry.size());
            m_realPosition->setValueAndWarp(m_geometry.pos());
            *m_realSize     = GOALSIZE;
            *m_realPosition = GOALPOS;
        }
    } else {
        // fade
        m_realPosition->setValueAndWarp(m_geometry.pos());
        m_realSize->setValueAndWarp(m_geometry.size());
        *m_alpha = in ? 1.f : 0.f;
    }

    if (!in)
        m_fadingOut = true;
}

bool CLayerSurface::isFadedOut() {
    if (!m_fadingOut)
        return false;

    return !m_realPosition->isBeingAnimated() && !m_realSize->isBeingAnimated() && !m_alpha->isBeingAnimated();
}

int CLayerSurface::popupsCount() {
    if (!m_layerSurface || !m_mapped || m_fadingOut)
        return 0;

    int no = -1; // we have one dummy
    m_popupHead->breadthfirst([](WP<CPopup> p, void* data) { *(int*)data += 1; }, &no);
    return no;
}

MONITORID CLayerSurface::monitorID() {
    return m_monitor ? m_monitor->ID : MONITOR_INVALID;
}

pid_t CLayerSurface::getPID() {
    pid_t PID = -1;

    if (!m_layerSurface || !m_layerSurface->surface || !m_layerSurface->surface->getResource() || !m_layerSurface->surface->getResource()->resource() ||
        !m_layerSurface->surface->getResource()->resource()->client)
        return -1;

    wl_client_get_credentials(m_layerSurface->surface->getResource()->resource()->client, &PID, nullptr, nullptr);

    return PID;
}
