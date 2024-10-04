#include "Monitor.hpp"
#include "MiscFunctions.hpp"
#include "math/Math.hpp"
#include "sync/SyncReleaser.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/GammaControl.hpp"
#include "../devices/ITouch.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/DRMLease.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/core/Output.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../protocols/core/Compositor.hpp"
#include "sync/SyncTimeline.hpp"
#include <aquamarine/output/Output.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Utils;

int ratHandler(void* data) {
    g_pHyprRenderer->renderMonitor((CMonitor*)data);

    return 1;
}

CMonitor::CMonitor(SP<Aquamarine::IOutput> output_) : state(this), output(output_) {
    ;
}

CMonitor::~CMonitor() {
    events.destroy.emit();
}

void CMonitor::onConnect(bool noRule) {

    if (output->supportsExplicit) {
        inTimeline  = CSyncTimeline::create(output->getBackend()->drmFD());
        outTimeline = CSyncTimeline::create(output->getBackend()->drmFD());
    }

    listeners.frame  = output->events.frame.registerListener([this](std::any d) { Events::listener_monitorFrame(this, nullptr); });
    listeners.commit = output->events.commit.registerListener([this](std::any d) { Events::listener_monitorCommit(this, nullptr); });
    listeners.needsFrame =
        output->events.needsFrame.registerListener([this](std::any d) { g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME); });

    listeners.presented = output->events.present.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IOutput::SPresentEvent>(d);
        PROTO::presentation->onPresented(self.lock(), E.when, E.refresh, E.seq, E.flags);
    });

    listeners.destroy = output->events.destroy.registerListener([this](std::any d) {
        Debug::log(LOG, "Destroy called for monitor {}", szName);

        onDisconnect(true);

        output                 = nullptr;
        m_bRenderingInitPassed = false;

        Debug::log(LOG, "Removing monitor {} from realMonitors", szName);

        std::erase_if(g_pCompositor->m_vRealMonitors, [&](SP<CMonitor>& el) { return el.get() == this; });
    });

    listeners.state = output->events.state.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IOutput::SStateEvent>(d);

        if (E.size == Vector2D{}) {
            // an indication to re-set state
            // we can't do much for createdByUser displays I think
            if (createdByUser)
                return;

            Debug::log(LOG, "Reapplying monitor rule for {} from a state request", szName);
            g_pHyprRenderer->applyMonitorRule(this, &activeMonitorRule, true);
            return;
        }

        if (!createdByUser)
            return;

        const auto SIZE = E.size;

        forceSize = SIZE;

        SMonitorRule rule = activeMonitorRule;
        rule.resolution   = SIZE;

        g_pHyprRenderer->applyMonitorRule(this, &rule);
    });

    tearingState.canTear = output->getBackend()->type() == Aquamarine::AQ_BACKEND_DRM;

    if (m_bEnabled) {
        output->state->resetExplicitFences();
        output->state->setEnabled(true);
        state.commit();
        return;
    }

    szName = output->name;

    szDescription = output->description;
    // remove comma character from description. This allow monitor specific rules to work on monitor with comma on their description
    std::erase(szDescription, ',');

    // field is backwards-compatible with intended usage of `szDescription` but excludes the parenthesized DRM node name suffix
    szShortDescription = trim(std::format("{} {} {}", output->make, output->model, output->serial));
    std::erase(szShortDescription, ',');

    if (output->getBackend()->type() != Aquamarine::AQ_BACKEND_DRM)
        createdByUser = true; // should be true. WL and Headless backends should be addable / removable

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(self.lock());

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {

        output->state->resetExplicitFences();
        output->state->setEnabled(false);

        if (!state.commit())
            Debug::log(ERR, "Couldn't commit disabled state on output {}", output->name);

        m_bEnabled = false;

        listeners.frame.reset();
        return;
    }

    if (output->nonDesktop) {
        Debug::log(LOG, "Not configuring non-desktop output");
        if (PROTO::lease)
            PROTO::lease->offer(self.lock());

        return;
    }

    SP<CMonitor>* thisWrapper = nullptr;

    // find the wrap
    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (m->ID == ID) {
            thisWrapper = &m;
            break;
        }
    }

    RASSERT(thisWrapper->get(), "CMonitor::onConnect: Had no wrapper???");

    if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_vMonitors.end())
        g_pCompositor->m_vMonitors.push_back(*thisWrapper);

    m_bEnabled = true;

    output->state->resetExplicitFences();
    output->state->setEnabled(true);

    // set mode, also applies
    if (!noRule)
        g_pHyprRenderer->applyMonitorRule(this, &monitorRule, true);

    if (!state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onCommit");

    damage.setSize(vecTransformedSize);

    Debug::log(LOG, "Added new monitor with name {} at {:j0} with size {:j0}, pointer {:x}", output->name, vecPosition, vecPixelSize, (uintptr_t)output.get());

    setupDefaultWS(monitorRule);

    for (auto const& ws : g_pCompositor->m_vWorkspaces) {
        if (!valid(ws))
            continue;

        if (ws->m_szLastMonitor == szName || g_pCompositor->m_vMonitors.size() == 1 /* avoid lost workspaces on recover */) {
            g_pCompositor->moveWorkspaceToMonitor(ws, this);
            ws->startAnim(true, true, true);
            ws->m_szLastMonitor = "";
        }
    }

    scale = monitorRule.scale;
    if (scale < 0.1)
        scale = getDefaultScale();

    forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.
    //

    if (!activeMonitorRule.mirrorOf.empty())
        setMirror(activeMonitorRule.mirrorOf);

    if (!g_pCompositor->m_pLastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->setActiveMonitor(this);

    g_pHyprRenderer->arrangeLayersForMonitor(ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    // ensure VRR (will enable if necessary)
    g_pConfigManager->ensureVRR(this);

    // verify last mon valid
    bool found = false;
    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (m == g_pCompositor->m_pLastMonitor) {
            found = true;
            break;
        }
    }

    if (!found)
        g_pCompositor->setActiveMonitor(this);

    renderTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, ratHandler, this);

    g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_NEW_MONITOR);

    PROTO::gamma->applyGammaToState(this);

    events.connect.emit();

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"monitoraddedv2", std::format("{},{},{}", ID, szName, szShortDescription)});
    EMIT_HOOK_EVENT("monitorAdded", this);
}

void CMonitor::onDisconnect(bool destroy) {

    if (renderTimer) {
        wl_event_source_remove(renderTimer);
        renderTimer = nullptr;
    }

    if (!m_bEnabled || g_pCompositor->m_bIsShuttingDown)
        return;

    Debug::log(LOG, "onDisconnect called for {}", output->name);

    events.disconnect.emit();

    // Cleanup everything. Move windows back, snap cursor, shit.
    CMonitor* BACKUPMON = nullptr;
    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (m.get() != this) {
            BACKUPMON = m.get();
            break;
        }
    }

    // remove mirror
    if (pMirrorOf) {
        pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == this; }));

        // unlock software for mirrored monitor
        g_pPointerManager->unlockSoftwareForMonitor(pMirrorOf);
        pMirrorOf = nullptr;
    }

    if (!mirrors.empty()) {
        for (auto const& m : mirrors) {
            m->setMirror("");
        }

        g_pConfigManager->m_bWantsMonitorReload = true;
    }

    listeners.frame.reset();
    listeners.presented.reset();
    listeners.needsFrame.reset();
    listeners.commit.reset();

    for (size_t i = 0; i < 4; ++i) {
        for (auto const& ls : m_aLayerSurfaceLayers[i]) {
            if (ls->layerSurface && !ls->fadingOut)
                ls->layerSurface->sendClosed();
        }
        m_aLayerSurfaceLayers[i].clear();
    }

    Debug::log(LOG, "Removed monitor {}!", szName);

    if (!BACKUPMON) {
        Debug::log(WARN, "Unplugged last monitor, entering an unsafe state. Good luck my friend.");
        g_pCompositor->enterUnsafeState();
    }

    m_bEnabled             = false;
    m_bRenderingInitPassed = false;

    if (BACKUPMON) {
        // snap cursor
        g_pCompositor->warpCursorTo(BACKUPMON->vecPosition + BACKUPMON->vecTransformedSize / 2.F, true);

        // move workspaces
        std::deque<PHLWORKSPACE> wspToMove;
        for (auto const& w : g_pCompositor->m_vWorkspaces) {
            if (w->m_iMonitorID == ID || !g_pCompositor->getMonitorFromID(w->m_iMonitorID)) {
                wspToMove.push_back(w);
            }
        }

        for (auto const& w : wspToMove) {
            w->m_szLastMonitor = szName;
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }
    } else {
        g_pCompositor->m_pLastFocus.reset();
        g_pCompositor->m_pLastWindow.reset();
        g_pCompositor->m_pLastMonitor.reset();
    }

    if (activeWorkspace)
        activeWorkspace->m_bVisible = false;
    activeWorkspace.reset();

    output->state->resetExplicitFences();
    output->state->setEnabled(false);

    if (!state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onDisconnect");

    if (g_pCompositor->m_pLastMonitor.get() == this)
        g_pCompositor->setActiveMonitor(BACKUPMON ? BACKUPMON : g_pCompositor->m_pUnsafeOutput);

    if (g_pHyprRenderer->m_pMostHzMonitor == this) {
        int       mostHz         = 0;
        CMonitor* pMonitorMostHz = nullptr;

        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (m->refreshRate > mostHz && m.get() != this) {
                pMonitorMostHz = m.get();
                mostHz         = m->refreshRate;
            }
        }

        g_pHyprRenderer->m_pMostHzMonitor = pMonitorMostHz;
    }
    std::erase_if(g_pCompositor->m_vMonitors, [&](SP<CMonitor>& el) { return el.get() == this; });

    g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", szName});
    EMIT_HOOK_EVENT("monitorRemoved", this);
}

void CMonitor::addDamage(const pixman_region32_t* rg) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == this) {
        damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    } else if (damage.damage(rg))
        g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

void CMonitor::addDamage(const CRegion* rg) {
    addDamage(const_cast<CRegion*>(rg)->pixman());
}

void CMonitor::addDamage(const CBox* box) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == this) {
        damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }

    if (damage.damage(*box))
        g_pCompositor->scheduleFrameForMonitor(this, Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

bool CMonitor::shouldSkipScheduleFrameOnMouseEvent() {
    static auto PNOBREAK = CConfigValue<Hyprlang::INT>("cursor:no_break_fs_vrr");
    static auto PMINRR   = CConfigValue<Hyprlang::INT>("cursor:min_refresh_rate");

    // skip scheduling extra frames for fullsreen apps with vrr
    bool shouldSkip =
        *PNOBREAK && output->state->state().adaptiveSync && activeWorkspace && activeWorkspace->m_bHasFullscreenWindow && activeWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN;

    // keep requested minimum refresh rate
    if (shouldSkip && *PMINRR && lastPresentationTimer.getMillis() > 1000 / *PMINRR) {
        // damage whole screen because some previous cursor box damages were skipped
        damage.damageEntire();
        return false;
    }

    return shouldSkip;
}

bool CMonitor::isMirror() {
    return pMirrorOf != nullptr;
}

bool CMonitor::matchesStaticSelector(const std::string& selector) const {
    if (selector.starts_with("desc:")) {
        // match by description
        const auto DESCRIPTIONSELECTOR = selector.substr(5);

        return DESCRIPTIONSELECTOR == szShortDescription || DESCRIPTIONSELECTOR == szDescription;
    } else {
        // match by selector
        return szName == selector;
    }
}

WORKSPACEID CMonitor::findAvailableDefaultWS() {
    for (WORKSPACEID i = 1; i < LONG_MAX; ++i) {
        if (g_pCompositor->getWorkspaceByID(i))
            continue;

        if (const auto BOUND = g_pConfigManager->getBoundMonitorStringForWS(std::to_string(i)); !BOUND.empty() && BOUND != szName)
            continue;

        return i;
    }

    return LONG_MAX; // shouldn't be reachable
}

void CMonitor::setupDefaultWS(const SMonitorRule& monitorRule) {
    // Workspace
    std::string newDefaultWorkspaceName = "";
    int64_t     wsID                    = WORKSPACE_INVALID;
    if (g_pConfigManager->getDefaultWorkspaceFor(szName).empty())
        wsID = findAvailableDefaultWS();
    else {
        const auto ws           = getWorkspaceIDNameFromString(g_pConfigManager->getDefaultWorkspaceFor(szName));
        wsID                    = ws.id;
        newDefaultWorkspaceName = ws.name;
    }

    if (wsID == WORKSPACE_INVALID || (wsID >= SPECIAL_WORKSPACE_START && wsID <= -2)) {
        wsID                    = g_pCompositor->m_vWorkspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(wsID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"{}\" is invalid.", g_pConfigManager->getDefaultWorkspaceFor(szName));
    }

    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(wsID);

    Debug::log(LOG, "New monitor: WORKSPACEID {}, exists: {}", wsID, (int)(PNEWWORKSPACE != nullptr));

    if (PNEWWORKSPACE) {
        // workspace exists, move it to the newly connected monitor
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, this);
        activeWorkspace = PNEWWORKSPACE;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);
        PNEWWORKSPACE->startAnim(true, true, true);
    } else {
        if (newDefaultWorkspaceName == "")
            newDefaultWorkspaceName = std::to_string(wsID);

        PNEWWORKSPACE = g_pCompositor->m_vWorkspaces.emplace_back(CWorkspace::create(wsID, ID, newDefaultWorkspaceName));
    }

    activeWorkspace = PNEWWORKSPACE;

    PNEWWORKSPACE->setActive(true);
    PNEWWORKSPACE->m_bVisible      = true;
    PNEWWORKSPACE->m_szLastMonitor = "";
}

void CMonitor::setMirror(const std::string& mirrorOf) {
    const auto PMIRRORMON = g_pCompositor->getMonitorFromString(mirrorOf);

    if (PMIRRORMON == pMirrorOf)
        return;

    if (PMIRRORMON && PMIRRORMON->isMirror()) {
        Debug::log(ERR, "Cannot mirror a mirror!");
        return;
    }

    if (PMIRRORMON == this) {
        Debug::log(ERR, "Cannot mirror self!");
        return;
    }

    if (!PMIRRORMON) {
        // disable mirroring

        if (pMirrorOf) {
            pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == this; }));

            // unlock software for mirrored monitor
            g_pPointerManager->unlockSoftwareForMonitor(pMirrorOf);
        }

        pMirrorOf = nullptr;

        // set rule
        const auto RULE = g_pConfigManager->getMonitorRuleFor(self.lock());

        vecPosition = RULE.offset;

        // push to mvmonitors

        SP<CMonitor>* thisWrapper = nullptr;

        // find the wrap
        for (auto& m : g_pCompositor->m_vRealMonitors) {
            if (m->ID == ID) {
                thisWrapper = &m;
                break;
            }
        }

        RASSERT(thisWrapper->get(), "CMonitor::setMirror: Had no wrapper???");

        if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) ==
            g_pCompositor->m_vMonitors.end()) {
            g_pCompositor->m_vMonitors.push_back(*thisWrapper);
        }

        setupDefaultWS(RULE);

        g_pHyprRenderer->applyMonitorRule(this, (SMonitorRule*)&RULE, true); // will apply the offset and stuff
    } else {
        CMonitor* BACKUPMON = nullptr;
        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (m.get() != this) {
                BACKUPMON = m.get();
                break;
            }
        }

        // move all the WS
        std::deque<PHLWORKSPACE> wspToMove;
        for (auto const& w : g_pCompositor->m_vWorkspaces) {
            if (w->m_iMonitorID == ID) {
                wspToMove.push_back(w);
            }
        }

        for (auto const& w : wspToMove) {
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }

        activeWorkspace.reset();

        vecPosition = PMIRRORMON->vecPosition;

        pMirrorOf = PMIRRORMON;

        pMirrorOf->mirrors.push_back(this);

        // remove from mvmonitors
        std::erase_if(g_pCompositor->m_vMonitors, [&](const auto& other) { return other.get() == this; });

        g_pCompositor->arrangeMonitors();

        g_pCompositor->setActiveMonitor(g_pCompositor->m_vMonitors.front().get());

        g_pCompositor->sanityCheckWorkspaces();

        // Software lock mirrored monitor
        g_pPointerManager->lockSoftwareForMonitor(PMIRRORMON);
    }

    events.modeChanged.emit();
}

float CMonitor::getDefaultScale() {
    if (!m_bEnabled)
        return 1;

    static constexpr double MMPERINCH = 25.4;

    const auto              DIAGONALPX = sqrt(pow(vecPixelSize.x, 2) + pow(vecPixelSize.y, 2));
    const auto              DIAGONALIN = sqrt(pow(output->physicalSize.x / MMPERINCH, 2) + pow(output->physicalSize.y / MMPERINCH, 2));

    const auto              PPI = DIAGONALPX / DIAGONALIN;

    if (PPI > 200 /* High PPI, 2x*/)
        return 2;
    else if (PPI > 140 /* Medium PPI, 1.5x*/)
        return 1.5;
    return 1;
}

void CMonitor::changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    if (!pWorkspace)
        return;

    if (pWorkspace->m_bIsSpecialWorkspace) {
        if (activeSpecialWorkspace != pWorkspace) {
            Debug::log(LOG, "changeworkspace on special, togglespecialworkspace to id {}", pWorkspace->m_iID);
            setSpecialWorkspace(pWorkspace);
        }
        return;
    }

    if (pWorkspace == activeWorkspace)
        return;

    const auto POLDWORKSPACE  = activeWorkspace;
    POLDWORKSPACE->m_bVisible = false;
    pWorkspace->m_bVisible    = true;

    activeWorkspace = pWorkspace;

    if (!internal) {
        const auto ANIMTOLEFT = pWorkspace->m_iID > POLDWORKSPACE->m_iID;
        POLDWORKSPACE->startAnim(false, ANIMTOLEFT);
        pWorkspace->startAnim(true, ANIMTOLEFT);

        // move pinned windows
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (w->m_pWorkspace == POLDWORKSPACE && w->m_bPinned)
                w->moveToWorkspace(pWorkspace);
        }

        if (!noFocus && !g_pCompositor->m_pLastMonitor->activeSpecialWorkspace &&
            !(g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bPinned && g_pCompositor->m_pLastWindow->m_iMonitorID == ID)) {
            static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
            auto        pWindow      = pWorkspace->getLastFocusedWindow();

            if (!pWindow) {
                if (*PFOLLOWMOUSE == 1)
                    pWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                if (!pWindow)
                    pWindow = g_pCompositor->getTopLeftWindowOnWorkspace(pWorkspace->m_iID);

                if (!pWindow)
                    pWindow = g_pCompositor->getFirstWindowOnWorkspace(pWorkspace->m_iID);
            }

            g_pCompositor->focusWindow(pWindow);
        }

        if (!noMouseMove)
            g_pInputManager->simulateMouseMovement();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        g_pEventManager->postEvent(SHyprIPCEvent{"workspace", pWorkspace->m_szName});
        g_pEventManager->postEvent(SHyprIPCEvent{"workspacev2", std::format("{},{}", pWorkspace->m_iID, pWorkspace->m_szName)});
        EMIT_HOOK_EVENT("workspace", pWorkspace);
    }

    g_pHyprRenderer->damageMonitor(this);

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(this);

    g_pCompositor->updateSuspendedStates();

    if (activeSpecialWorkspace)
        g_pCompositor->updateFullscreenFadeOnWorkspace(activeSpecialWorkspace);
}

void CMonitor::changeWorkspace(const WORKSPACEID& id, bool internal, bool noMouseMove, bool noFocus) {
    changeWorkspace(g_pCompositor->getWorkspaceByID(id), internal, noMouseMove, noFocus);
}

void CMonitor::setSpecialWorkspace(const PHLWORKSPACE& pWorkspace) {
    if (activeSpecialWorkspace == pWorkspace)
        return;

    g_pHyprRenderer->damageMonitor(this);

    if (!pWorkspace) {
        // remove special if exists
        if (activeSpecialWorkspace) {
            activeSpecialWorkspace->m_bVisible = false;
            activeSpecialWorkspace->startAnim(false, false);
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + szName});
        }
        activeSpecialWorkspace.reset();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        if (!(g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bPinned && g_pCompositor->m_pLastWindow->m_iMonitorID == ID)) {
            if (const auto PLAST = activeWorkspace->getLastFocusedWindow(); PLAST)
                g_pCompositor->focusWindow(PLAST);
            else
                g_pInputManager->refocus();
        }

        g_pCompositor->updateFullscreenFadeOnWorkspace(activeWorkspace);

        g_pConfigManager->ensureVRR(this);

        g_pCompositor->updateSuspendedStates();

        return;
    }

    if (activeSpecialWorkspace) {
        activeSpecialWorkspace->m_bVisible = false;
        activeSpecialWorkspace->startAnim(false, false);
    }

    bool animate = true;
    //close if open elsewhere
    const auto PMONITORWORKSPACEOWNER = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    if (PMONITORWORKSPACEOWNER->activeSpecialWorkspace == pWorkspace) {
        PMONITORWORKSPACEOWNER->activeSpecialWorkspace.reset();
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITORWORKSPACEOWNER->ID);
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + PMONITORWORKSPACEOWNER->szName});

        const auto PACTIVEWORKSPACE = PMONITORWORKSPACEOWNER->activeWorkspace;
        g_pCompositor->updateFullscreenFadeOnWorkspace(PACTIVEWORKSPACE);

        animate = false;
    }

    // open special
    pWorkspace->m_iMonitorID           = ID;
    activeSpecialWorkspace             = pWorkspace;
    activeSpecialWorkspace->m_bVisible = true;
    if (animate)
        pWorkspace->startAnim(true, true);

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == pWorkspace) {
            w->m_iMonitorID = ID;
            w->updateSurfaceScaleTransformDetails();
            w->setAnimationsToMove();

            const auto MIDDLE = w->middle();
            if (w->m_bIsFloating && !VECINRECT(MIDDLE, vecPosition.x, vecPosition.y, vecPosition.x + vecSize.x, vecPosition.y + vecSize.y) && !w->isX11OverrideRedirect()) {
                // if it's floating and the middle isnt on the current mon, move it to the center
                const auto PMONFROMMIDDLE = g_pCompositor->getMonitorFromVector(MIDDLE);
                Vector2D   pos            = w->m_vRealPosition.goal();
                if (!VECINRECT(MIDDLE, PMONFROMMIDDLE->vecPosition.x, PMONFROMMIDDLE->vecPosition.y, PMONFROMMIDDLE->vecPosition.x + PMONFROMMIDDLE->vecSize.x,
                               PMONFROMMIDDLE->vecPosition.y + PMONFROMMIDDLE->vecSize.y)) {
                    // not on any monitor, center
                    pos = middle() / 2.f - w->m_vRealSize.goal() / 2.f;
                } else
                    pos = pos - PMONFROMMIDDLE->vecPosition + vecPosition;

                w->m_vRealPosition = pos;
                w->m_vPosition     = pos;
            }
        }
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    if (!(g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bPinned && g_pCompositor->m_pLastWindow->m_iMonitorID == ID)) {
        if (const auto PLAST = pWorkspace->getLastFocusedWindow(); PLAST)
            g_pCompositor->focusWindow(PLAST);
        else
            g_pInputManager->refocus();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", pWorkspace->m_szName + "," + szName});

    g_pHyprRenderer->damageMonitor(this);

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(this);

    g_pCompositor->updateSuspendedStates();
}

void CMonitor::setSpecialWorkspace(const WORKSPACEID& id) {
    setSpecialWorkspace(g_pCompositor->getWorkspaceByID(id));
}

void CMonitor::moveTo(const Vector2D& pos) {
    vecPosition = pos;
}

Vector2D CMonitor::middle() {
    return vecPosition + vecSize / 2.f;
}

void CMonitor::updateMatrix() {
    projMatrix = Mat3x3::identity();
    if (transform != WL_OUTPUT_TRANSFORM_NORMAL)
        projMatrix.translate(vecPixelSize / 2.0).transform(wlTransformToHyprutils(transform)).translate(-vecTransformedSize / 2.0);
}

WORKSPACEID CMonitor::activeWorkspaceID() {
    return activeWorkspace ? activeWorkspace->m_iID : 0;
}

WORKSPACEID CMonitor::activeSpecialWorkspaceID() {
    return activeSpecialWorkspace ? activeSpecialWorkspace->m_iID : 0;
}

CBox CMonitor::logicalBox() {
    return {vecPosition, vecSize};
}

void CMonitor::scheduleDone() {
    if (doneScheduled)
        return;

    doneScheduled = true;

    g_pEventLoopManager->doLater([M = self] {
        if (!M) // if M is gone, we got destroyed, doesn't matter.
            return;

        if (!PROTO::outputs.contains(M->szName))
            return;

        PROTO::outputs.at(M->szName)->sendDone();
        M->doneScheduled = false;
    });
}

bool CMonitor::attemptDirectScanout() {
    if (!mirrors.empty() || isMirror() || g_pHyprRenderer->m_bDirectScanoutBlocked)
        return false; // do not DS if this monitor is being mirrored. Will break the functionality.

    if (g_pPointerManager->softwareLockedFor(self.lock()))
        return false;

    const auto PCANDIDATE = solitaryClient.lock();

    if (!PCANDIDATE)
        return false;

    const auto PSURFACE = g_pXWaylandManager->getWindowSurface(PCANDIDATE);

    if (!PSURFACE || !PSURFACE->current.buffer || PSURFACE->current.bufferSize != vecPixelSize || PSURFACE->current.transform != transform)
        return false;

    // we can't scanout shm buffers.
    if (!PSURFACE->current.buffer || !PSURFACE->current.texture || !PSURFACE->current.texture->m_pEglImage /* dmabuf */)
        return false;

    Debug::log(TRACE, "attemptDirectScanout: surface {:x} passed, will attempt", (uintptr_t)PSURFACE.get());

    // FIXME: make sure the buffer actually follows the available scanout dmabuf formats
    // and comes from the appropriate device. This may implode on multi-gpu!!

    const auto params = PSURFACE->current.buffer->buffer->dmabuf();
    // scanout buffer isn't dmabuf, so no scanout
    if (!params.success)
        return false;

    // entering into scanout, so save monitor format
    if (lastScanout.expired())
        prevDrmFormat = drmFormat;

    if (drmFormat != params.format) {
        output->state->setFormat(params.format);
        drmFormat = params.format;
    }

    output->state->setBuffer(PSURFACE->current.buffer->buffer.lock());
    output->state->setPresentationMode(tearingState.activelyTearing ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE :
                                                                      Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC);

    if (!state.test()) {
        Debug::log(TRACE, "attemptDirectScanout: failed basic test");
        return false;
    }

    auto explicitOptions = g_pHyprRenderer->getExplicitSyncSettings();

    // wait for the explicit fence if present, and if kms explicit is allowed
    bool DOEXPLICIT = PSURFACE->syncobj && PSURFACE->syncobj->current.acquireTimeline && PSURFACE->syncobj->current.acquireTimeline->timeline && explicitOptions.explicitKMSEnabled;
    int  explicitWaitFD = -1;
    if (DOEXPLICIT) {
        explicitWaitFD = PSURFACE->syncobj->current.acquireTimeline->timeline->exportAsSyncFileFD(PSURFACE->syncobj->current.acquirePoint);
        if (explicitWaitFD < 0)
            Debug::log(TRACE, "attemptDirectScanout: failed to acquire an explicit wait fd");
    }
    DOEXPLICIT = DOEXPLICIT && explicitWaitFD >= 0;

    auto     cleanup = CScopeGuard([explicitWaitFD, this]() {
        output->state->resetExplicitFences();
        if (explicitWaitFD >= 0)
            close(explicitWaitFD);
    });

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    PSURFACE->presentFeedback(&now, self.lock());

    output->state->addDamage(CBox{{}, vecPixelSize});
    output->state->resetExplicitFences();

    if (DOEXPLICIT) {
        Debug::log(TRACE, "attemptDirectScanout: setting IN_FENCE for aq to {}", explicitWaitFD);
        output->state->setExplicitInFence(explicitWaitFD);
    }

    bool ok = output->commit();

    if (!ok && DOEXPLICIT) {
        Debug::log(TRACE, "attemptDirectScanout: EXPLICIT SYNC FAILED: commit() returned false. Resetting fences and retrying, might result in glitches.");
        output->state->resetExplicitFences();

        ok = output->commit();
    }

    if (ok) {
        if (lastScanout.expired()) {
            lastScanout = PCANDIDATE;
            Debug::log(LOG, "Entered a direct scanout to {:x}: \"{}\"", (uintptr_t)PCANDIDATE.get(), PCANDIDATE->m_szTitle);
        }

        // delay explicit sync feedback until kms release of the buffer
        if (DOEXPLICIT) {
            Debug::log(TRACE, "attemptDirectScanout: Delaying explicit sync release feedback until kms release");
            PSURFACE->current.buffer->releaser->drop();

            PSURFACE->current.buffer->buffer->hlEvents.backendRelease2 = PSURFACE->current.buffer->buffer->events.backendRelease.registerListener([PSURFACE](std::any d) {
                const bool DOEXPLICIT = PSURFACE->syncobj && PSURFACE->syncobj->current.releaseTimeline && PSURFACE->syncobj->current.releaseTimeline->timeline;
                if (DOEXPLICIT)
                    PSURFACE->syncobj->current.releaseTimeline->timeline->signal(PSURFACE->syncobj->current.releasePoint);
            });
        }
    } else {
        Debug::log(TRACE, "attemptDirectScanout: failed to scanout surface");
        lastScanout.reset();
        return false;
    }

    return true;
}

CMonitorState::CMonitorState(CMonitor* owner) {
    m_pOwner = owner;
}

CMonitorState::~CMonitorState() {
    ;
}

void CMonitorState::ensureBufferPresent() {
    const auto STATE = m_pOwner->output->state->state();
    if (!STATE.enabled) {
        Debug::log(TRACE, "CMonitorState::ensureBufferPresent: Ignoring, monitor is not enabled");
        return;
    }

    if (STATE.buffer) {
        if (const auto params = STATE.buffer->dmabuf(); params.success && params.format == m_pOwner->drmFormat)
            return;
    }

    // this is required for modesetting being possible and might be missing in case of first tests in the renderer
    // where we test modes and buffers
    Debug::log(LOG, "CMonitorState::ensureBufferPresent: no buffer or mismatched format, attaching one from the swapchain for modeset being possible");
    m_pOwner->output->state->setBuffer(m_pOwner->output->swapchain->next(nullptr));
    m_pOwner->output->swapchain->rollback(); // restore the counter, don't advance the swapchain
}

bool CMonitorState::commit() {
    if (!updateSwapchain())
        return false;

    EMIT_HOOK_EVENT("preMonitorCommit", m_pOwner);

    ensureBufferPresent();

    bool ret = m_pOwner->output->commit();
    return ret;
}

bool CMonitorState::test() {
    if (!updateSwapchain())
        return false;

    ensureBufferPresent();

    return m_pOwner->output->test();
}

bool CMonitorState::updateSwapchain() {
    auto        options = m_pOwner->output->swapchain->currentOptions();
    const auto& STATE   = m_pOwner->output->state->state();
    const auto& MODE    = STATE.mode ? STATE.mode : STATE.customMode;
    if (!MODE) {
        Debug::log(WARN, "updateSwapchain: No mode?");
        return true;
    }
    options.format  = m_pOwner->drmFormat;
    options.scanout = true;
    options.length  = 2;
    options.size    = MODE->pixelSize;
    return m_pOwner->output->swapchain->reconfigure(options);
}
