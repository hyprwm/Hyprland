#include "LayerSurface.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"

void Events::listener_newLayerSurface(wl_listener* listener, void* data) {
    const auto WLRLAYERSURFACE = (wlr_layer_surface_v1*)data;

    if (!WLRLAYERSURFACE->output) {
        const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

        if (!PMONITOR) {
            Debug::log(ERR, "No monitor at cursor on new layer without a monitor. Ignoring.");
            wlr_layer_surface_v1_destroy(WLRLAYERSURFACE);
            return;
        }

        Debug::log(LOG, "New LayerSurface has no preferred monitor. Assigning Monitor {}", PMONITOR->szName);

        WLRLAYERSURFACE->output = PMONITOR->output;
    }

    auto PMONITOR = g_pCompositor->getMonitorFromOutput(WLRLAYERSURFACE->output);

    if (!WLRLAYERSURFACE->output || !PMONITOR || PMONITOR->pMirrorOf) {
        PMONITOR                = g_pCompositor->m_vMonitors.front().get();
        WLRLAYERSURFACE->output = PMONITOR->output; // TODO: current mon
    }

    const auto PLS = PMONITOR->m_aLayerSurfaceLayers[WLRLAYERSURFACE->pending.layer].emplace_back(CLayerSurface::create(WLRLAYERSURFACE));

    Debug::log(LOG, "LayerSurface {:x} (namespace {} layer {}) created on monitor {}", (uintptr_t)WLRLAYERSURFACE, WLRLAYERSURFACE->_namespace, (int)PLS->layer, PMONITOR->szName);
}

static void onCommit(void* owner, void* data) {
    const auto LS = ((CLayerSurface*)owner)->self.lock();

    LS->onCommit();
}

static void onMap(void* owner, void* data) {
    const auto LS = ((CLayerSurface*)owner)->self.lock();

    LS->onMap();
}

static void onUnmap(void* owner, void* data) {
    const auto LS = ((CLayerSurface*)owner)->self.lock();

    LS->onUnmap();
}

static void onDestroy(void* owner, void* data) {
    const auto LS = ((CLayerSurface*)owner)->self.lock();

    LS->onDestroy();
}

// IMPL

PHLLS CLayerSurface::create(wlr_layer_surface_v1* pWLRLS) {
    PHLLS pLS = std::shared_ptr<CLayerSurface>(new CLayerSurface);

    auto  PMONITOR = g_pCompositor->getMonitorFromOutput(pWLRLS->output);

    pLS->self = pLS;

    pLS->szNamespace = pWLRLS->_namespace;

    pLS->hyprListener_commitLayerSurface.initCallback(&pWLRLS->surface->events.commit, ::onCommit, pLS.get(), "layerSurface");
    pLS->hyprListener_destroyLayerSurface.initCallback(&pWLRLS->events.destroy, ::onDestroy, pLS.get(), "layerSurface");
    pLS->hyprListener_mapLayerSurface.initCallback(&pWLRLS->surface->events.map, ::onMap, pLS.get(), "layerSurface");
    pLS->hyprListener_unmapLayerSurface.initCallback(&pWLRLS->surface->events.unmap, ::onUnmap, pLS.get(), "layerSurface");

    pLS->layerSurface = pWLRLS;
    pLS->layer        = pWLRLS->current.layer;
    pWLRLS->data      = pLS.get();
    pLS->monitorID    = PMONITOR->ID;
    pLS->popupHead    = std::make_unique<CPopup>(pLS);

    pLS->forceBlur = g_pConfigManager->shouldBlurLS(pLS->szNamespace);

    pLS->alpha.create(g_pConfigManager->getAnimationPropertyConfig("fadeLayersIn"), pLS, AVARDAMAGE_ENTIRE);
    pLS->realPosition.create(g_pConfigManager->getAnimationPropertyConfig("layersIn"), pLS, AVARDAMAGE_ENTIRE);
    pLS->realSize.create(g_pConfigManager->getAnimationPropertyConfig("layersIn"), pLS, AVARDAMAGE_ENTIRE);
    pLS->alpha.registerVar();
    pLS->realPosition.registerVar();
    pLS->realSize.registerVar();

    pLS->registerCallbacks();

    pLS->alpha.setValueAndWarp(0.f);

    pLS->surface.assign(pWLRLS->surface);

    return pLS;
}

void CLayerSurface::registerCallbacks() {
    alpha.setUpdateCallback([this](void*) {
        if (dimAround)
            g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(monitorID));
    });
}

CLayerSurface::CLayerSurface() {
    ;
}

CLayerSurface::~CLayerSurface() {
    if (!g_pHyprOpenGL)
        return;

    surface.unassign();
    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mLayerFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.lock() == self.lock(); });
}

void CLayerSurface::onDestroy() {
    Debug::log(LOG, "LayerSurface {:x} destroyed", (uintptr_t)layerSurface);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitorID);

    popupHead.reset();

    if (!g_pCompositor->getMonitorFromID(monitorID))
        Debug::log(WARN, "Layersurface destroyed on an invalid monitor (removed?)");

    if (!fadingOut) {
        if (mapped) {
            Debug::log(LOG, "Forcing an unmap of a LS that did a straight destroy!");
            onUnmap();
        } else {
            Debug::log(LOG, "Removing LayerSurface that wasn't mapped.");
            alpha.setValueAndWarp(0.f);
            fadingOut = true;
            g_pCompositor->addToFadingOutSafe(self.lock());
        }
    }

    noProcess = true;

    hyprListener_commitLayerSurface.removeCallback();
    hyprListener_destroyLayerSurface.removeCallback();
    hyprListener_mapLayerSurface.removeCallback();
    hyprListener_unmapLayerSurface.removeCallback();

    // rearrange to fix the reserved areas
    if (PMONITOR) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
        PMONITOR->scheduledRecalc = true;

        // and damage
        CBox geomFixed = {geometry.x + PMONITOR->vecPosition.x, geometry.y + PMONITOR->vecPosition.y, geometry.width, geometry.height};
        g_pHyprRenderer->damageBox(&geomFixed);
    }

    readyToDelete = true;
    layerSurface  = nullptr;
    surface.unassign();
}

void CLayerSurface::onMap() {
    Debug::log(LOG, "LayerSurface {:x} mapped", (uintptr_t)layerSurface);

    mapped            = true;
    keyboardExclusive = layerSurface->current.keyboard_interactive;

    // fix if it changed its mon
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitorID);

    if (!PMONITOR)
        return;

    applyRules();

    if ((uint64_t)monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(monitorID);
        for (auto it = POLDMON->m_aLayerSurfaceLayers[layer].begin(); it != POLDMON->m_aLayerSurfaceLayers[layer].end(); it++) {
            if (*it == self.lock()) {
                PMONITOR->m_aLayerSurfaceLayers[layer].emplace_back(std::move(*it));
                POLDMON->m_aLayerSurfaceLayers[layer].erase(it);
                break;
            }
        }
        monitorID                 = PMONITOR->ID;
        PMONITOR->scheduledRecalc = true;
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    wlr_surface_send_enter(surface.wlr(), PMONITOR->output);

    if (layerSurface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
        g_pInputManager->m_dExclusiveLSes.push_back(self);

    const bool GRABSFOCUS = layerSurface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
        // don't focus if constrained
        (g_pCompositor->m_sSeat.mouse.expired() || !g_pInputManager->isConstrained());

    if (GRABSFOCUS) {
        g_pInputManager->releaseAllMouseButtons();
        g_pCompositor->focusSurface(surface.wlr());

        const auto LOCAL = g_pInputManager->getMouseCoordsInternal() - Vector2D(geometry.x + PMONITOR->vecPosition.x, geometry.y + PMONITOR->vecPosition.y);
        wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, surface.wlr(), LOCAL.x, LOCAL.y);
        wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, 0, LOCAL.x, LOCAL.y);
        g_pInputManager->m_bEmptyFocusCursorSet = false;
    }

    position = Vector2D(geometry.x, geometry.y);

    CBox geomFixed = {geometry.x + PMONITOR->vecPosition.x, geometry.y + PMONITOR->vecPosition.y, geometry.width, geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);
    const auto WORKSPACE  = PMONITOR->activeWorkspace;
    const bool FULLSCREEN = WORKSPACE->m_bHasFullscreenWindow && WORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL;

    startAnimation(!(layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP && FULLSCREEN && !GRABSFOCUS));
    readyToDelete = false;
    fadingOut     = false;

    g_pEventManager->postEvent(SHyprIPCEvent{"openlayer", szNamespace});
    EMIT_HOOK_EVENT("openLayer", self.lock());

    g_pCompositor->setPreferredScaleForSurface(surface.wlr(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(surface.wlr(), PMONITOR->transform);
}

void CLayerSurface::onUnmap() {
    Debug::log(LOG, "LayerSurface {:x} unmapped", (uintptr_t)layerSurface);

    g_pEventManager->postEvent(SHyprIPCEvent{"closelayer", std::string(layerSurface->_namespace ? layerSurface->_namespace : "")});
    EMIT_HOOK_EVENT("closeLayer", self.lock());

    std::erase_if(g_pInputManager->m_dExclusiveLSes, [this](const auto& other) { return !other.lock() || other.lock() == self.lock(); });

    if (!g_pInputManager->m_dExclusiveLSes.empty())
        g_pCompositor->focusSurface(g_pInputManager->m_dExclusiveLSes[0].lock()->layerSurface->surface);

    if (!g_pCompositor->getMonitorFromID(monitorID) || g_pCompositor->m_bUnsafeState) {
        Debug::log(WARN, "Layersurface unmapping on invalid monitor (removed?) ignoring.");

        g_pCompositor->addToFadingOutSafe(self.lock());

        mapped = false;

        startAnimation(false);
        return;
    }

    // make a snapshot and start fade
    g_pHyprOpenGL->makeLayerSnapshot(self.lock());

    startAnimation(false);

    mapped = false;

    g_pCompositor->addToFadingOutSafe(self.lock());

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layerSurface->output);

    const bool WASLASTFOCUS = g_pCompositor->m_pLastFocus == layerSurface->surface;

    surface = nullptr;

    if (!PMONITOR)
        return;

    // refocus if needed
    if (WASLASTFOCUS) {
        g_pInputManager->releaseAllMouseButtons();

        Vector2D     surfaceCoords;
        PHLLS        pFoundLayerSurface;
        wlr_surface* foundSurface = nullptr;

        g_pCompositor->m_pLastFocus = nullptr;

        // find LS-es to focus
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                                                           &surfaceCoords, &pFoundLayerSurface);

        if (!foundSurface)
            foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
                                                               &surfaceCoords, &pFoundLayerSurface);

        if (!foundSurface && g_pCompositor->m_pLastWindow.lock() && g_pCompositor->isWorkspaceVisible(g_pCompositor->m_pLastWindow.lock()->m_pWorkspace)) {
            // if there isn't any, focus the last window
            const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
            g_pCompositor->focusWindow(nullptr);
            g_pCompositor->focusWindow(PLASTWINDOW);
        } else {
            // otherwise, full refocus
            g_pInputManager->refocus();
        }
    }

    CBox geomFixed = {geometry.x + PMONITOR->vecPosition.x, geometry.y + PMONITOR->vecPosition.y, geometry.width, geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    geomFixed = {geometry.x + (int)PMONITOR->vecPosition.x, geometry.y + (int)PMONITOR->vecPosition.y, (int)layerSurface->surface->current.width,
                 (int)layerSurface->surface->current.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    g_pInputManager->sendMotionEventsToFocused();
}

void CLayerSurface::onCommit() {
    if (!layerSurface || !layerSurface->output)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layerSurface->output);

    if (!PMONITOR)
        return;

    if (layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd

    CBox geomFixed = {geometry.x, geometry.y, geometry.width, geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    // fix if it changed its mon
    if ((uint64_t)monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(monitorID);

        for (auto it = POLDMON->m_aLayerSurfaceLayers[layer].begin(); it != POLDMON->m_aLayerSurfaceLayers[layer].end(); it++) {
            if (*it == self.lock()) {
                PMONITOR->m_aLayerSurfaceLayers[layer].emplace_back(std::move(*it));
                POLDMON->m_aLayerSurfaceLayers[layer].erase(it);
                break;
            }
        }

        monitorID                 = PMONITOR->ID;
        PMONITOR->scheduledRecalc = true;
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    if (layerSurface->current.committed != 0) {
        if (layer != layerSurface->current.layer) {

            for (auto it = PMONITOR->m_aLayerSurfaceLayers[layer].begin(); it != PMONITOR->m_aLayerSurfaceLayers[layer].end(); it++) {
                if (*it == self.lock()) {
                    PMONITOR->m_aLayerSurfaceLayers[layerSurface->current.layer].emplace_back(std::move(*it));
                    PMONITOR->m_aLayerSurfaceLayers[layer].erase(it);
                    break;
                }
            }

            layer = layerSurface->current.layer;

            if (layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
                g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd
        }

        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

        PMONITOR->scheduledRecalc = true;
    } else {
        position = Vector2D(geometry.x, geometry.y);

        // update geom if it changed
        if (layerSurface->surface->current.scale == 1 && PMONITOR->scale != 1.f && layerSurface->surface->current.viewport.has_dst) {
            // fractional scaling. Dirty hack.
            geometry = {geometry.x, geometry.y, (int)(layerSurface->surface->current.viewport.dst_width), (int)(layerSurface->surface->current.viewport.dst_height)};
        } else {
            // this is because some apps like e.g. rofi-lbonn can't fucking use the protocol correctly.
            geometry = {geometry.x, geometry.y, (int)layerSurface->surface->current.width, (int)layerSurface->surface->current.height};
        }
    }

    if (realPosition.goal() != geometry.pos()) {
        if (realPosition.isBeingAnimated())
            realPosition = geometry.pos();
        else
            realPosition.setValueAndWarp(geometry.pos());
    }
    if (realSize.goal() != geometry.size()) {
        if (realSize.isBeingAnimated())
            realSize = geometry.size();
        else
            realSize.setValueAndWarp(geometry.size());
    }

    if (layerSurface->current.keyboard_interactive && (g_pCompositor->m_sSeat.mouse.expired() || !g_pInputManager->isConstrained()) // don't focus if constrained
        && !keyboardExclusive && mapped) {
        g_pCompositor->focusSurface(layerSurface->surface);

        const auto LOCAL = g_pInputManager->getMouseCoordsInternal() - Vector2D(geometry.x + PMONITOR->vecPosition.x, geometry.y + PMONITOR->vecPosition.y);
        wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, layerSurface->surface, LOCAL.x, LOCAL.y);
        wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, 0, LOCAL.x, LOCAL.y);
        g_pInputManager->m_bEmptyFocusCursorSet = false;
    } else if (!layerSurface->current.keyboard_interactive && (g_pCompositor->m_sSeat.mouse.expired() || !g_pInputManager->isConstrained()) && keyboardExclusive) {
        g_pInputManager->refocus();
    }

    keyboardExclusive = layerSurface->current.keyboard_interactive;

    g_pHyprRenderer->damageSurface(layerSurface->surface, position.x, position.y);

    g_pCompositor->setPreferredScaleForSurface(layerSurface->surface, PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(layerSurface->surface, PMONITOR->transform);
}

void CLayerSurface::applyRules() {
    noAnimations     = false;
    forceBlur        = false;
    ignoreAlpha      = false;
    ignoreAlphaValue = 0.f;
    dimAround        = false;
    xray             = -1;
    animationStyle.reset();

    for (auto& rule : g_pConfigManager->getMatchingRules(self.lock())) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule == "blurpopups")
            forceBlurPopups = true;
        else if (rule.rule.starts_with("ignorealpha") || rule.rule.starts_with("ignorezero")) {
            const auto  FIRST_SPACE_POS = rule.rule.find_first_of(' ');
            std::string alphaValue      = "";
            if (FIRST_SPACE_POS != std::string::npos)
                alphaValue = rule.rule.substr(FIRST_SPACE_POS + 1);

            try {
                ignoreAlpha = true;
                if (!alphaValue.empty())
                    ignoreAlphaValue = std::stof(alphaValue);
            } catch (...) { Debug::log(ERR, "Invalid value passed to ignoreAlpha"); }
        } else if (rule.rule == "dimaround") {
            dimAround = true;
        } else if (rule.rule.starts_with("xray")) {
            CVarList vars{rule.rule, 0, ' '};
            try {
                xray = configStringToInt(vars[1]);
            } catch (...) {}
        } else if (rule.rule.starts_with("animation")) {
            CVarList vars{rule.rule, 2, 's'};
            animationStyle = vars[1];
        }
    }
}

void CLayerSurface::startAnimation(bool in, bool instant) {
    const auto ANIMSTYLE = animationStyle.value_or(realPosition.m_pConfig->pValues->internalStyle);
    if (in) {
        realPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("layersIn");
        realSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("layersIn");
        alpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeLayersIn");
    } else {
        realPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("layersOut");
        realSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("layersOut");
        alpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeLayersOut");
    }

    if (ANIMSTYLE.starts_with("slide")) {
        // get closest edge
        const auto MIDDLE = geometry.middle();

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
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, 0},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{0, PMONITOR->vecSize.y},
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

        realSize.setValueAndWarp(geometry.size());
        alpha.setValueAndWarp(in ? 0.f : 1.f);
        alpha = in ? 1.f : 0.f;

        Vector2D prePos;

        switch (leader) {
            case 0:
                // TOP
                prePos = {geometry.x, PMONITOR->vecPosition.y - geometry.h};
                break;
            case 1:
                // BOTTOM
                prePos = {geometry.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y};
                break;
            case 2:
                // LEFT
                prePos = {PMONITOR->vecPosition.x - geometry.w, geometry.y};
                break;
            case 3:
                // RIGHT
                prePos = {PMONITOR->vecPosition.x + PMONITOR->vecSize.x, geometry.y};
                break;
            default: UNREACHABLE();
        }

        if (in) {
            realPosition.setValueAndWarp(prePos);
            realPosition = geometry.pos();
        } else {
            realPosition.setValueAndWarp(geometry.pos());
            realPosition = prePos;
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

        const auto GOALSIZE = (geometry.size() * minPerc).clamp({5, 5});
        const auto GOALPOS  = geometry.pos() + (geometry.size() - GOALSIZE) / 2.f;

        alpha.setValueAndWarp(in ? 0.f : 1.f);
        alpha = in ? 1.f : 0.f;

        if (in) {
            realSize.setValueAndWarp(GOALSIZE);
            realPosition.setValueAndWarp(GOALPOS);
            realSize     = geometry.size();
            realPosition = geometry.pos();
        } else {
            realSize.setValueAndWarp(geometry.size());
            realPosition.setValueAndWarp(geometry.pos());
            realSize     = GOALSIZE;
            realPosition = GOALPOS;
        }
    } else {
        // fade
        realPosition.setValueAndWarp(geometry.pos());
        realSize.setValueAndWarp(geometry.size());
        alpha = in ? 1.f : 0.f;
    }

    if (!in)
        fadingOut = true;
}

bool CLayerSurface::isFadedOut() {
    if (!fadingOut)
        return false;

    return !realPosition.isBeingAnimated() && !realSize.isBeingAnimated() && !alpha.isBeingAnimated();
}

int CLayerSurface::popupsCount() {
    if (!layerSurface || !mapped || fadingOut)
        return 0;

    int no = 0;
    wlr_layer_surface_v1_for_each_popup_surface(
        layerSurface, [](wlr_surface* s, int x, int y, void* data) { *(int*)data += 1; }, &no);
    return no;
}