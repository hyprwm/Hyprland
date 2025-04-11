#include "Monitor.hpp"
#include "MiscFunctions.hpp"
#include "../macros.hpp"
#include "math/Math.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../protocols/GammaControl.hpp"
#include "../devices/ITouch.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/DRMLease.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/core/Output.hpp"
#include "../protocols/Screencopy.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "sync/SyncTimeline.hpp"
#include "time/Time.hpp"
#include "../desktop/LayerSurface.hpp"
#include <aquamarine/output/Output.hpp>
#include "debug/Log.hpp"
#include "debug/HyprNotificationOverlay.hpp"
#include <hyprutils/string/String.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <cstring>
#include <ranges>

using namespace Hyprutils::String;
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;

static int ratHandler(void* data) {
    g_pHyprRenderer->renderMonitor(((CMonitor*)data)->self.lock());

    return 1;
}

CMonitor::CMonitor(SP<Aquamarine::IOutput> output_) : state(this), output(output_) {
    ;
}

CMonitor::~CMonitor() {
    events.destroy.emit();
    if (g_pHyprOpenGL)
        g_pHyprOpenGL->destroyMonitorResources(self);
}

void CMonitor::onConnect(bool noRule) {
    EMIT_HOOK_EVENT("preMonitorAdded", self.lock());
    CScopeGuard x = {[]() { g_pCompositor->arrangeMonitors(); }};

    g_pEventLoopManager->doLater([] { g_pConfigManager->ensurePersistentWorkspacesPresent(); });

    if (output->supportsExplicit) {
        inTimeline = CSyncTimeline::create(output->getBackend()->drmFD());
    }

    listeners.frame  = output->events.frame.registerListener([this](std::any d) { onMonitorFrame(); });
    listeners.commit = output->events.commit.registerListener([this](std::any d) {
        if (true) { // FIXME: E->state->committed & WLR_OUTPUT_STATE_BUFFER
            PROTO::screencopy->onOutputCommit(self.lock());
            PROTO::toplevelExport->onOutputCommit(self.lock());
        }
    });
    listeners.needsFrame =
        output->events.needsFrame.registerListener([this](std::any d) { g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME); });

    listeners.presented = output->events.present.registerListener([this](std::any d) {
        auto      E = std::any_cast<Aquamarine::IOutput::SPresentEvent>(d);

        timespec* ts = E.when;
        if (!ts) {
            timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            PROTO::presentation->onPresented(self.lock(), Time::fromTimespec(&now), E.refresh, E.seq, E.flags);
        } else
            PROTO::presentation->onPresented(self.lock(), Time::fromTimespec(E.when), E.refresh, E.seq, E.flags);
    });

    listeners.destroy = output->events.destroy.registerListener([this](std::any d) {
        Debug::log(LOG, "Destroy called for monitor {}", szName);

        onDisconnect(true);

        output                 = nullptr;
        m_bRenderingInitPassed = false;

        Debug::log(LOG, "Removing monitor {} from realMonitors", szName);

        std::erase_if(g_pCompositor->m_realMonitors, [&](PHLMONITOR& el) { return el.get() == this; });
    });

    listeners.state = output->events.state.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IOutput::SStateEvent>(d);

        if (E.size == Vector2D{}) {
            // an indication to re-set state
            // we can't do much for createdByUser displays I think
            if (createdByUser)
                return;

            Debug::log(LOG, "Reapplying monitor rule for {} from a state request", szName);
            applyMonitorRule(&activeMonitorRule, true);
            return;
        }

        if (!createdByUser)
            return;

        const auto SIZE = E.size;

        forceSize = SIZE;

        SMonitorRule rule = activeMonitorRule;
        rule.resolution   = SIZE;

        applyMonitorRule(&rule);
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

    PHLMONITOR* thisWrapper = nullptr;

    // find the wrap
    for (auto& m : g_pCompositor->m_realMonitors) {
        if (m->ID == ID) {
            thisWrapper = &m;
            break;
        }
    }

    RASSERT(thisWrapper->get(), "CMonitor::onConnect: Had no wrapper???");

    if (std::find_if(g_pCompositor->m_monitors.begin(), g_pCompositor->m_monitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_monitors.end())
        g_pCompositor->m_monitors.push_back(*thisWrapper);

    m_bEnabled = true;

    output->state->resetExplicitFences();
    output->state->setEnabled(true);

    // set mode, also applies
    if (!noRule)
        applyMonitorRule(&monitorRule, true);

    if (!state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onCommit");

    damage.setSize(vecTransformedSize);

    Debug::log(LOG, "Added new monitor with name {} at {:j0} with size {:j0}, pointer {:x}", output->name, vecPosition, vecPixelSize, (uintptr_t)output.get());

    setupDefaultWS(monitorRule);

    for (auto const& ws : g_pCompositor->m_workspaces) {
        if (!valid(ws))
            continue;

        if (ws->m_lastMonitor == szName || g_pCompositor->m_monitors.size() == 1 /* avoid lost workspaces on recover */) {
            g_pCompositor->moveWorkspaceToMonitor(ws, self.lock());
            ws->startAnim(true, true, true);
            ws->m_lastMonitor = "";
        }
    }

    scale = monitorRule.scale;
    if (scale < 0.1)
        scale = getDefaultScale();

    forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.
    //

    if (!activeMonitorRule.mirrorOf.empty())
        setMirror(activeMonitorRule.mirrorOf);

    if (!g_pCompositor->m_lastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->setActiveMonitor(self.lock());

    g_pHyprRenderer->arrangeLayersForMonitor(ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    // ensure VRR (will enable if necessary)
    g_pConfigManager->ensureVRR(self.lock());

    // verify last mon valid
    bool found = false;
    for (auto const& m : g_pCompositor->m_monitors) {
        if (m == g_pCompositor->m_lastMonitor) {
            found = true;
            break;
        }
    }

    Debug::log(LOG, "checking if we have seen this monitor before: {}", szName);
    // if we saw this monitor before, set it to the workspace it was on
    if (g_pCompositor->m_seenMonitorWorkspaceMap.contains(szName)) {
        auto workspaceID = g_pCompositor->m_seenMonitorWorkspaceMap[szName];
        Debug::log(LOG, "Monitor {} was on workspace {}, setting it to that", szName, workspaceID);
        auto ws = g_pCompositor->getWorkspaceByID(workspaceID);
        if (ws) {
            g_pCompositor->moveWorkspaceToMonitor(ws, self.lock());
            changeWorkspace(ws, true, false, false);
        }
    } else
        Debug::log(LOG, "Monitor {} was not on any workspace", szName);

    if (!found)
        g_pCompositor->setActiveMonitor(self.lock());

    renderTimer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, ratHandler, this);

    g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_NEW_MONITOR);

    PROTO::gamma->applyGammaToState(self.lock());

    events.connect.emit();

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"monitoraddedv2", std::format("{},{},{}", ID, szName, szShortDescription)});
    EMIT_HOOK_EVENT("monitorAdded", self.lock());
}

void CMonitor::onDisconnect(bool destroy) {
    EMIT_HOOK_EVENT("preMonitorRemoved", self.lock());
    CScopeGuard x = {[this]() {
        if (g_pCompositor->m_isShuttingDown)
            return;
        g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", szName});
        EMIT_HOOK_EVENT("monitorRemoved", self.lock());
        g_pCompositor->arrangeMonitors();
    }};

    if (renderTimer) {
        wl_event_source_remove(renderTimer);
        renderTimer = nullptr;
    }

    if (!m_bEnabled || g_pCompositor->m_isShuttingDown)
        return;

    Debug::log(LOG, "onDisconnect called for {}", output->name);

    events.disconnect.emit();
    if (g_pHyprOpenGL)
        g_pHyprOpenGL->destroyMonitorResources(self);

    // record what workspace this monitor was on
    if (activeWorkspace) {
        Debug::log(LOG, "Disconnecting Monitor {} was on workspace {}", szName, activeWorkspace->m_id);
        g_pCompositor->m_seenMonitorWorkspaceMap[szName] = activeWorkspace->m_id;
    }

    // Cleanup everything. Move windows back, snap cursor, shit.
    PHLMONITOR BACKUPMON = nullptr;
    for (auto const& m : g_pCompositor->m_monitors) {
        if (m.get() != this) {
            BACKUPMON = m;
            break;
        }
    }

    // remove mirror
    if (pMirrorOf) {
        pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == self; }));

        // unlock software for mirrored monitor
        g_pPointerManager->unlockSoftwareForMonitor(pMirrorOf.lock());
        pMirrorOf.reset();
    }

    if (!mirrors.empty()) {
        for (auto const& m : mirrors) {
            m->setMirror("");
        }

        g_pConfigManager->m_wantsMonitorReload = true;
    }

    listeners.frame.reset();
    listeners.presented.reset();
    listeners.needsFrame.reset();
    listeners.commit.reset();

    for (size_t i = 0; i < 4; ++i) {
        for (auto const& ls : m_aLayerSurfaceLayers[i]) {
            if (ls->m_layerSurface && !ls->m_fadingOut)
                ls->m_layerSurface->sendClosed();
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
        std::vector<PHLWORKSPACE> wspToMove;
        for (auto const& w : g_pCompositor->m_workspaces) {
            if (w->m_monitor == self || !w->m_monitor)
                wspToMove.push_back(w);
        }

        for (auto const& w : wspToMove) {
            w->m_lastMonitor = szName;
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }
    } else {
        g_pCompositor->m_lastFocus.reset();
        g_pCompositor->m_lastWindow.reset();
        g_pCompositor->m_lastMonitor.reset();
    }

    if (activeWorkspace)
        activeWorkspace->m_visible = false;
    activeWorkspace.reset();

    output->state->resetExplicitFences();
    output->state->setAdaptiveSync(false);
    output->state->setEnabled(false);

    if (!state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onDisconnect");

    if (g_pCompositor->m_lastMonitor == self)
        g_pCompositor->setActiveMonitor(BACKUPMON ? BACKUPMON : g_pCompositor->m_unsafeOutput.lock());

    if (g_pHyprRenderer->m_pMostHzMonitor == self) {
        int        mostHz         = 0;
        PHLMONITOR pMonitorMostHz = nullptr;

        for (auto const& m : g_pCompositor->m_monitors) {
            if (m->refreshRate > mostHz && m != self) {
                pMonitorMostHz = m;
                mostHz         = m->refreshRate;
            }
        }

        g_pHyprRenderer->m_pMostHzMonitor = pMonitorMostHz;
    }
    std::erase_if(g_pCompositor->m_monitors, [&](PHLMONITOR& el) { return el.get() == this; });
}

bool CMonitor::applyMonitorRule(SMonitorRule* pMonitorRule, bool force) {

    static auto PDISABLESCALECHECKS = CConfigValue<Hyprlang::INT>("debug:disable_scale_checks");

    Debug::log(LOG, "Applying monitor rule for {}", szName);

    activeMonitorRule = *pMonitorRule;

    if (forceSize.has_value())
        activeMonitorRule.resolution = forceSize.value();

    const auto RULE = &activeMonitorRule;

    // if it's disabled, disable and ignore
    if (RULE->disabled) {
        if (m_bEnabled)
            onDisconnect();

        events.modeChanged.emit();

        return true;
    }

    // don't touch VR headsets
    if (output->nonDesktop)
        return true;

    if (!m_bEnabled) {
        onConnect(true); // enable it.
        Debug::log(LOG, "Monitor {} is disabled but is requested to be enabled", szName);
        force = true;
    }

    // Check if the rule isn't already applied
    // TODO: clean this up lol
    if (!force && DELTALESSTHAN(vecPixelSize.x, RULE->resolution.x, 1) && DELTALESSTHAN(vecPixelSize.y, RULE->resolution.y, 1) &&
        DELTALESSTHAN(refreshRate, RULE->refreshRate, 1) && setScale == RULE->scale &&
        ((DELTALESSTHAN(vecPosition.x, RULE->offset.x, 1) && DELTALESSTHAN(vecPosition.y, RULE->offset.y, 1)) || RULE->offset == Vector2D(-INT32_MAX, -INT32_MAX)) &&
        transform == RULE->transform && RULE->enable10bit == enabled10bit && RULE->cmType == cmType && RULE->sdrSaturation == sdrSaturation &&
        RULE->sdrBrightness == sdrBrightness && !std::memcmp(&customDrmMode, &RULE->drmMode, sizeof(customDrmMode))) {

        Debug::log(LOG, "Not applying a new rule to {} because it's already applied!", szName);

        setMirror(RULE->mirrorOf);

        return true;
    }

    bool autoScale = false;

    if (RULE->scale > 0.1) {
        scale = RULE->scale;
    } else {
        autoScale               = true;
        const auto DEFAULTSCALE = getDefaultScale();
        scale                   = DEFAULTSCALE;
    }

    setScale  = scale;
    transform = RULE->transform;

    // accumulate requested modes in reverse order (cause inesrting at front is inefficient)
    std::vector<SP<Aquamarine::SOutputMode>> requestedModes;
    std::string                              requestedStr = "unknown";

    // use sortFunc, add best 3 to requestedModes in reverse, since we test in reverse
    auto addBest3Modes = [&](auto const& sortFunc) {
        auto sortedModes = output->modes;
        std::ranges::sort(sortedModes, sortFunc);
        if (sortedModes.size() > 3)
            sortedModes.erase(sortedModes.begin() + 3, sortedModes.end());
        requestedModes.insert(requestedModes.end(), sortedModes.rbegin(), sortedModes.rend());
    };

    // last fallback is always preferred mode
    if (!output->preferredMode())
        Debug::log(ERR, "Monitor {} has NO PREFERRED MODE", output->name);
    else
        requestedModes.push_back(output->preferredMode());

    if (RULE->resolution == Vector2D()) {
        requestedStr = "preferred";

        // fallback to first 3 modes if preferred fails/doesn't exist
        requestedModes = output->modes;
        if (requestedModes.size() > 3)
            requestedModes.erase(requestedModes.begin() + 3, requestedModes.end());
        std::ranges::reverse(requestedModes.begin(), requestedModes.end());

        if (output->preferredMode())
            requestedModes.push_back(output->preferredMode());
    } else if (RULE->resolution == Vector2D(-1, -1)) {
        requestedStr = "highrr";

        // sort prioritizing refresh rate 1st and resolution 2nd, then add best 3
        addBest3Modes([](auto const& a, auto const& b) {
            if (std::round(a->refreshRate) > std::round(b->refreshRate))
                return true;
            else if (DELTALESSTHAN((float)a->refreshRate, (float)b->refreshRate, 1.F) && a->pixelSize.x > b->pixelSize.x && a->pixelSize.y > b->pixelSize.y)
                return true;
            return false;
        });
    } else if (RULE->resolution == Vector2D(-1, -2)) {
        requestedStr = "highres";

        // sort prioritizing resultion 1st and refresh rate 2nd, then add best 3
        addBest3Modes([](auto const& a, auto const& b) {
            if (a->pixelSize.x > b->pixelSize.x && a->pixelSize.y > b->pixelSize.y)
                return true;
            else if (DELTALESSTHAN(a->pixelSize.x, b->pixelSize.x, 1) && DELTALESSTHAN(a->pixelSize.y, b->pixelSize.y, 1) &&
                     std::round(a->refreshRate) > std::round(b->refreshRate))
                return true;
            return false;
        });
    } else if (RULE->resolution != Vector2D()) {
        // user requested mode
        requestedStr = std::format("{:X0}@{:.2f}Hz", RULE->resolution, RULE->refreshRate);

        // sort by closeness to requested, then add best 3
        addBest3Modes([&](auto const& a, auto const& b) {
            if (abs(a->pixelSize.x - RULE->resolution.x) < abs(b->pixelSize.x - RULE->resolution.x))
                return true;
            if (a->pixelSize.x == b->pixelSize.x && abs(a->pixelSize.y - RULE->resolution.y) < abs(b->pixelSize.y - RULE->resolution.y))
                return true;
            if (a->pixelSize == b->pixelSize && abs((a->refreshRate / 1000.f) - RULE->refreshRate) < abs((b->refreshRate / 1000.f) - RULE->refreshRate))
                return true;
            return false;
        });

        // if the best mode isnt close to requested, then try requested as custom mode first
        if (!requestedModes.empty()) {
            auto bestMode = requestedModes.back();
            if (!DELTALESSTHAN(bestMode->pixelSize.x, RULE->resolution.x, 1) || !DELTALESSTHAN(bestMode->pixelSize.y, RULE->resolution.y, 1) ||
                !DELTALESSTHAN(bestMode->refreshRate / 1000.f, RULE->refreshRate, 1))
                requestedModes.push_back(makeShared<Aquamarine::SOutputMode>(Aquamarine::SOutputMode{.pixelSize = RULE->resolution, .refreshRate = RULE->refreshRate * 1000.f}));
        }

        // then if requested is custom, try custom mode first
        if (RULE->drmMode.type == DRM_MODE_TYPE_USERDEF) {
            if (output->getBackend()->type() != Aquamarine::eBackendType::AQ_BACKEND_DRM)
                Debug::log(ERR, "Tried to set custom modeline on non-DRM output");
            else
                requestedModes.push_back(makeShared<Aquamarine::SOutputMode>(
                    Aquamarine::SOutputMode{.pixelSize = {RULE->drmMode.hdisplay, RULE->drmMode.vdisplay}, .refreshRate = RULE->drmMode.vrefresh, .modeInfo = RULE->drmMode}));
        }
    }

    const auto WAS10B  = enabled10bit;
    const auto OLDRES  = vecPixelSize;
    bool       success = false;

    // Needed in case we are switching from a custom modeline to a standard mode
    customDrmMode = {};
    currentMode   = nullptr;

    output->state->setFormat(DRM_FORMAT_XRGB8888);
    prevDrmFormat = drmFormat;
    drmFormat     = DRM_FORMAT_XRGB8888;
    output->state->resetExplicitFences();

    if (Debug::m_trace) {
        Debug::log(TRACE, "Monitor {} requested modes:", szName);
        if (requestedModes.empty())
            Debug::log(TRACE, "| None");
        else {
            for (auto const& mode : requestedModes | std::views::reverse) {
                Debug::log(TRACE, "| {:X0}@{:.2f}Hz", mode->pixelSize, mode->refreshRate / 1000.f);
            }
        }
    }

    for (auto const& mode : requestedModes | std::views::reverse) {
        std::string modeStr = std::format("{:X0}@{:.2f}Hz", mode->pixelSize, mode->refreshRate / 1000.f);

        if (mode->modeInfo.has_value() && mode->modeInfo->type == DRM_MODE_TYPE_USERDEF) {
            output->state->setCustomMode(mode);

            if (!state.test()) {
                Debug::log(ERR, "Monitor {}: REJECTED custom mode {}!", szName, modeStr);
                continue;
            }

            customDrmMode = mode->modeInfo.value();
        } else {
            output->state->setMode(mode);

            if (!state.test()) {
                Debug::log(ERR, "Monitor {}: REJECTED available mode {}!", szName, modeStr);
                if (mode->preferred)
                    Debug::log(ERR, "Monitor {}: REJECTED preferred mode!!!", szName);
                continue;
            }

            customDrmMode = {};
        }

        refreshRate = mode->refreshRate / 1000.f;
        vecSize     = mode->pixelSize;
        currentMode = mode;

        success = true;

        if (mode->preferred)
            Debug::log(LOG, "Monitor {}: requested {}, using preferred mode {}", szName, requestedStr, modeStr);
        else if (mode->modeInfo.has_value() && mode->modeInfo->type == DRM_MODE_TYPE_USERDEF)
            Debug::log(LOG, "Monitor {}: requested {}, using custom mode {}", szName, requestedStr, modeStr);
        else
            Debug::log(LOG, "Monitor {}: requested {}, using available mode {}", szName, requestedStr, modeStr);

        break;
    }

    // try requested as custom mode jic it works
    if (!success && RULE->resolution != Vector2D() && RULE->resolution != Vector2D(-1, -1) && RULE->resolution != Vector2D(-1, -2)) {
        auto        refreshRate = output->getBackend()->type() == Aquamarine::eBackendType::AQ_BACKEND_DRM ? RULE->refreshRate * 1000 : 0;
        auto        mode        = makeShared<Aquamarine::SOutputMode>(Aquamarine::SOutputMode{.pixelSize = RULE->resolution, .refreshRate = refreshRate});
        std::string modeStr     = std::format("{:X0}@{:.2f}Hz", mode->pixelSize, mode->refreshRate / 1000.f);

        output->state->setCustomMode(mode);

        if (state.test()) {
            Debug::log(LOG, "Monitor {}: requested {}, using custom mode {}", szName, requestedStr, modeStr);

            refreshRate   = mode->refreshRate / 1000.f;
            vecSize       = mode->pixelSize;
            currentMode   = mode;
            customDrmMode = {};

            success = true;
        } else
            Debug::log(ERR, "Monitor {}: REJECTED custom mode {}!", szName, modeStr);
    }

    // try any of the modes if none of the above work
    if (!success) {
        for (auto const& mode : output->modes) {
            output->state->setMode(mode);

            if (!state.test())
                continue;

            auto errorMessage =
                std::format("Monitor {} failed to set any requested modes, falling back to mode {:X0}@{:.2f}Hz", szName, mode->pixelSize, mode->refreshRate / 1000.f);
            Debug::log(WARN, errorMessage);
            g_pHyprNotificationOverlay->addNotification(errorMessage, CHyprColor(0xff0000ff), 5000, ICON_WARNING);

            refreshRate   = mode->refreshRate / 1000.f;
            vecSize       = mode->pixelSize;
            currentMode   = mode;
            customDrmMode = {};

            success = true;

            break;
        }
    }

    if (!success) {
        Debug::log(ERR, "Monitor {} has NO FALLBACK MODES, and an INVALID one was requested: {:X0}@{:.2f}Hz", szName, RULE->resolution, RULE->refreshRate);
        return true;
    }

    vrrActive = output->state->state().adaptiveSync // disabled here, will be tested in CConfigManager::ensureVRR()
        || createdByUser;                           // wayland backend doesn't allow for disabling adaptive_sync

    vecPixelSize = vecSize;

    // clang-format off
    static const std::array<std::vector<std::pair<std::string, uint32_t>>, 2> formats{
        std::vector<std::pair<std::string, uint32_t>>{ /* 10-bit */
            {"DRM_FORMAT_XRGB2101010", DRM_FORMAT_XRGB2101010}, {"DRM_FORMAT_XBGR2101010", DRM_FORMAT_XBGR2101010}, {"DRM_FORMAT_XRGB8888", DRM_FORMAT_XRGB8888}, {"DRM_FORMAT_XBGR8888", DRM_FORMAT_XBGR8888}
        },
        std::vector<std::pair<std::string, uint32_t>>{ /* 8-bit */
            {"DRM_FORMAT_XRGB8888", DRM_FORMAT_XRGB8888}, {"DRM_FORMAT_XBGR8888", DRM_FORMAT_XBGR8888}
        }
    };
    // clang-format on

    bool set10bit = false;

    for (auto const& fmt : formats[(int)!RULE->enable10bit]) {
        output->state->setFormat(fmt.second);
        prevDrmFormat = drmFormat;
        drmFormat     = fmt.second;

        if (!state.test()) {
            Debug::log(ERR, "output {} failed basic test on format {}", szName, fmt.first);
        } else {
            Debug::log(LOG, "output {} succeeded basic test on format {}", szName, fmt.first);
            if (RULE->enable10bit && fmt.first.contains("101010"))
                set10bit = true;
            break;
        }
    }

    enabled10bit = set10bit;

    auto oldImageDescription = imageDescription;
    cmType                   = RULE->cmType;
    switch (cmType) {
        case CM_AUTO: cmType = enabled10bit && output->parsedEDID.supportsBT2020 ? CM_WIDE : CM_SRGB; break;
        case CM_EDID: cmType = output->parsedEDID.chromaticityCoords.has_value() ? CM_EDID : CM_SRGB; break;
        case CM_HDR:
        case CM_HDR_EDID:
            cmType = output->parsedEDID.supportsBT2020 && output->parsedEDID.hdrMetadata.has_value() && output->parsedEDID.hdrMetadata->supportsPQ ? cmType : CM_SRGB;
            break;
        default: break;
    }
    switch (cmType) {
        case CM_SRGB: imageDescription = {}; break; // assumes SImageDescirption defaults to sRGB
        case CM_WIDE:
            imageDescription = {.primariesNameSet = true,
                                .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020)};
            break;
        case CM_EDID:
            imageDescription = {.primariesNameSet = false,
                                .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                .primaries        = {
                                           .red   = {.x = output->parsedEDID.chromaticityCoords->red.x, .y = output->parsedEDID.chromaticityCoords->red.y},
                                           .green = {.x = output->parsedEDID.chromaticityCoords->green.x, .y = output->parsedEDID.chromaticityCoords->green.y},
                                           .blue  = {.x = output->parsedEDID.chromaticityCoords->blue.x, .y = output->parsedEDID.chromaticityCoords->blue.y},
                                           .white = {.x = output->parsedEDID.chromaticityCoords->white.x, .y = output->parsedEDID.chromaticityCoords->white.y},
                                }};
            break;
        case CM_HDR:
            imageDescription = {.transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
                                .primariesNameSet = true,
                                .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
                                .luminances       = {.min = 0, .max = 10000, .reference = 203}};
            break;
        case CM_HDR_EDID:
            imageDescription = {.transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
                                .primariesNameSet = false,
                                .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                .primaries        = output->parsedEDID.chromaticityCoords.has_value() ?
                                           NColorManagement::SPCPRimaries{
                                               .red   = {.x = output->parsedEDID.chromaticityCoords->red.x, .y = output->parsedEDID.chromaticityCoords->red.y},
                                               .green = {.x = output->parsedEDID.chromaticityCoords->green.x, .y = output->parsedEDID.chromaticityCoords->green.y},
                                               .blue  = {.x = output->parsedEDID.chromaticityCoords->blue.x, .y = output->parsedEDID.chromaticityCoords->blue.y},
                                               .white = {.x = output->parsedEDID.chromaticityCoords->white.x, .y = output->parsedEDID.chromaticityCoords->white.y},
                                    } :
                                           NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
                                .luminances       = {.min       = output->parsedEDID.hdrMetadata->desiredContentMinLuminance,
                                                     .max       = output->parsedEDID.hdrMetadata->desiredContentMaxLuminance,
                                                     .reference = output->parsedEDID.hdrMetadata->desiredMaxFrameAverageLuminance}};

            break;
        default: UNREACHABLE();
    }
    if (oldImageDescription != imageDescription)
        PROTO::colorManagement->onMonitorImageDescriptionChanged(self);

    sdrSaturation = RULE->sdrSaturation;
    sdrBrightness = RULE->sdrBrightness;

    Vector2D logicalSize = vecPixelSize / scale;
    if (!*PDISABLESCALECHECKS && (logicalSize.x != std::round(logicalSize.x) || logicalSize.y != std::round(logicalSize.y))) {
        // invalid scale, will produce fractional pixels.
        // find the nearest valid.

        float    searchScale = std::round(scale * 120.0);
        bool     found       = false;

        double   scaleZero = searchScale / 120.0;

        Vector2D logicalZero = vecPixelSize / scaleZero;
        if (logicalZero == logicalZero.round())
            scale = scaleZero;
        else {
            for (size_t i = 1; i < 90; ++i) {
                double   scaleUp   = (searchScale + i) / 120.0;
                double   scaleDown = (searchScale - i) / 120.0;

                Vector2D logicalUp   = vecPixelSize / scaleUp;
                Vector2D logicalDown = vecPixelSize / scaleDown;

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
                    scale = std::round(scaleZero);
                else {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} failed to find a clean divisor", scale);
                    g_pConfigManager->addParseError("Invalid scale passed to monitor " + szName + ", failed to find a clean divisor");
                    scale = getDefaultScale();
                }
            } else {
                if (!autoScale) {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} found suggestion {}", scale, searchScale);
                    g_pConfigManager->addParseError(
                        std::format("Invalid scale passed to monitor {}, failed to find a clean divisor. Suggested nearest scale: {:5f}", szName, searchScale));
                    scale = getDefaultScale();
                } else
                    scale = searchScale;
            }
        }
    }

    output->scheduleFrame();

    if (!state.commit())
        Debug::log(ERR, "Couldn't commit output named {}", output->name);

    Vector2D xfmd      = transform % 2 == 1 ? Vector2D{vecPixelSize.y, vecPixelSize.x} : vecPixelSize;
    vecSize            = (xfmd / scale).round();
    vecTransformedSize = xfmd;

    if (createdByUser) {
        CBox transformedBox = {0, 0, vecTransformedSize.x, vecTransformedSize.y};
        transformedBox.transform(wlTransformToHyprutils(invertTransform(transform)), vecTransformedSize.x, vecTransformedSize.y);

        vecPixelSize = Vector2D(transformedBox.width, transformedBox.height);
    }

    updateMatrix();

    if (WAS10B != enabled10bit || OLDRES != vecPixelSize)
        g_pHyprOpenGL->destroyMonitorResources(self);

    g_pCompositor->arrangeMonitors();

    damage.setSize(vecTransformedSize);

    // Set scale for all surfaces on this monitor, needed for some clients
    // but not on unsafe state to avoid crashes
    if (!g_pCompositor->m_unsafeState) {
        for (auto const& w : g_pCompositor->m_windows) {
            w->updateSurfaceScaleTransformDetails();
        }
    }
    // updato us
    g_pHyprRenderer->arrangeLayersForMonitor(ID);

    // reload to fix mirrors
    g_pConfigManager->m_wantsMonitorReload = true;

    Debug::log(LOG, "Monitor {} data dump: res {:X}@{:.2f}Hz, scale {:.2f}, transform {}, pos {:X}, 10b {}", szName, vecPixelSize, refreshRate, scale, (int)transform, vecPosition,
               (int)enabled10bit);

    EMIT_HOOK_EVENT("monitorLayoutChanged", nullptr);

    events.modeChanged.emit();

    return true;
}

void CMonitor::addDamage(const pixman_region32_t* rg) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == self) {
        damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    } else if (damage.damage(rg))
        g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

void CMonitor::addDamage(const CRegion& rg) {
    addDamage(const_cast<CRegion*>(&rg)->pixman());
}

void CMonitor::addDamage(const CBox& box) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == self) {
        damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }

    if (damage.damage(box))
        g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

bool CMonitor::shouldSkipScheduleFrameOnMouseEvent() {
    static auto PNOBREAK = CConfigValue<Hyprlang::INT>("cursor:no_break_fs_vrr");
    static auto PMINRR   = CConfigValue<Hyprlang::INT>("cursor:min_refresh_rate");

    // skip scheduling extra frames for fullsreen apps with vrr
    const bool shouldSkip = activeWorkspace && activeWorkspace->m_hasFullscreenWindow && activeWorkspace->m_fullscreenMode == FSMODE_FULLSCREEN &&
        (*PNOBREAK == 1 || (*PNOBREAK == 2 && activeWorkspace->getFullscreenWindow()->getContentType() == CONTENT_TYPE_GAME)) && output->state->state().adaptiveSync;

    // keep requested minimum refresh rate
    if (shouldSkip && *PMINRR && lastPresentationTimer.getMillis() > 1000.0f / *PMINRR) {
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
        const auto DESCRIPTIONSELECTOR = trim(selector.substr(5));

        return szDescription.starts_with(DESCRIPTIONSELECTOR) || szShortDescription.starts_with(DESCRIPTIONSELECTOR);
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
        wsID                    = g_pCompositor->m_workspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(wsID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"{}\" is invalid.", g_pConfigManager->getDefaultWorkspaceFor(szName));
    }

    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(wsID);

    Debug::log(LOG, "New monitor: WORKSPACEID {}, exists: {}", wsID, (int)(PNEWWORKSPACE != nullptr));

    if (PNEWWORKSPACE) {
        // workspace exists, move it to the newly connected monitor
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, self.lock());
        activeWorkspace = PNEWWORKSPACE;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);
        PNEWWORKSPACE->startAnim(true, true, true);
    } else {
        if (newDefaultWorkspaceName == "")
            newDefaultWorkspaceName = std::to_string(wsID);

        PNEWWORKSPACE = g_pCompositor->m_workspaces.emplace_back(CWorkspace::create(wsID, self.lock(), newDefaultWorkspaceName));
    }

    activeWorkspace = PNEWWORKSPACE;

    PNEWWORKSPACE->setActive(true);
    PNEWWORKSPACE->m_visible     = true;
    PNEWWORKSPACE->m_lastMonitor = "";
}

void CMonitor::setMirror(const std::string& mirrorOf) {
    const auto PMIRRORMON = g_pCompositor->getMonitorFromString(mirrorOf);

    if (PMIRRORMON == pMirrorOf)
        return;

    if (PMIRRORMON && PMIRRORMON->isMirror()) {
        Debug::log(ERR, "Cannot mirror a mirror!");
        return;
    }

    if (PMIRRORMON == self) {
        Debug::log(ERR, "Cannot mirror self!");
        return;
    }

    if (!PMIRRORMON) {
        // disable mirroring

        if (pMirrorOf) {
            pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == self; }));

            // unlock software for mirrored monitor
            g_pPointerManager->unlockSoftwareForMonitor(pMirrorOf.lock());
        }

        pMirrorOf.reset();

        // set rule
        const auto RULE = g_pConfigManager->getMonitorRuleFor(self.lock());

        vecPosition = RULE.offset;

        // push to mvmonitors

        PHLMONITOR* thisWrapper = nullptr;

        // find the wrap
        for (auto& m : g_pCompositor->m_realMonitors) {
            if (m->ID == ID) {
                thisWrapper = &m;
                break;
            }
        }

        RASSERT(thisWrapper->get(), "CMonitor::setMirror: Had no wrapper???");

        if (std::find_if(g_pCompositor->m_monitors.begin(), g_pCompositor->m_monitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_monitors.end()) {
            g_pCompositor->m_monitors.push_back(*thisWrapper);
        }

        setupDefaultWS(RULE);

        applyMonitorRule((SMonitorRule*)&RULE, true); // will apply the offset and stuff
    } else {
        PHLMONITOR BACKUPMON = nullptr;
        for (auto const& m : g_pCompositor->m_monitors) {
            if (m.get() != this) {
                BACKUPMON = m;
                break;
            }
        }

        // move all the WS
        std::vector<PHLWORKSPACE> wspToMove;
        for (auto const& w : g_pCompositor->m_workspaces) {
            if (w->m_monitor == self || !w->m_monitor)
                wspToMove.push_back(w);
        }

        for (auto const& w : wspToMove) {
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }

        activeWorkspace.reset();

        vecPosition = PMIRRORMON->vecPosition;

        pMirrorOf = PMIRRORMON;

        pMirrorOf->mirrors.push_back(self);

        // remove from mvmonitors
        std::erase_if(g_pCompositor->m_monitors, [&](const auto& other) { return other == self; });

        g_pCompositor->arrangeMonitors();

        g_pCompositor->setActiveMonitor(g_pCompositor->m_monitors.front());

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

    if (pWorkspace->m_isSpecialWorkspace) {
        if (activeSpecialWorkspace != pWorkspace) {
            Debug::log(LOG, "changeworkspace on special, togglespecialworkspace to id {}", pWorkspace->m_id);
            setSpecialWorkspace(pWorkspace);
        }
        return;
    }

    if (pWorkspace == activeWorkspace)
        return;

    const auto POLDWORKSPACE = activeWorkspace;
    if (POLDWORKSPACE)
        POLDWORKSPACE->m_visible = false;
    pWorkspace->m_visible = true;

    activeWorkspace = pWorkspace;

    if (!internal) {
        const auto ANIMTOLEFT = POLDWORKSPACE && pWorkspace->m_id > POLDWORKSPACE->m_id;
        if (POLDWORKSPACE)
            POLDWORKSPACE->startAnim(false, ANIMTOLEFT);
        pWorkspace->startAnim(true, ANIMTOLEFT);

        // move pinned windows
        for (auto const& w : g_pCompositor->m_windows) {
            if (w->m_workspace == POLDWORKSPACE && w->m_pinned)
                w->moveToWorkspace(pWorkspace);
        }

        if (!noFocus && !g_pCompositor->m_lastMonitor->activeSpecialWorkspace &&
            !(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == self)) {
            static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
            auto        pWindow      = pWorkspace->m_hasFullscreenWindow ? pWorkspace->getFullscreenWindow() : pWorkspace->getLastFocusedWindow();

            if (!pWindow) {
                if (*PFOLLOWMOUSE == 1)
                    pWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                if (!pWindow)
                    pWindow = pWorkspace->getTopLeftWindow();

                if (!pWindow)
                    pWindow = pWorkspace->getFirstWindow();
            }

            g_pCompositor->focusWindow(pWindow);
        }

        if (!noMouseMove)
            g_pInputManager->simulateMouseMovement();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        g_pEventManager->postEvent(SHyprIPCEvent{"workspace", pWorkspace->m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{"workspacev2", std::format("{},{}", pWorkspace->m_id, pWorkspace->m_name)});
        EMIT_HOOK_EVENT("workspace", pWorkspace);
    }

    g_pHyprRenderer->damageMonitor(self.lock());

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(self.lock());

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

    g_pHyprRenderer->damageMonitor(self.lock());

    if (!pWorkspace) {
        // remove special if exists
        if (activeSpecialWorkspace) {
            activeSpecialWorkspace->m_visible = false;
            activeSpecialWorkspace->startAnim(false, false);
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + szName});
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", ",," + szName});
        }
        activeSpecialWorkspace.reset();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        if (!(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == self)) {
            if (const auto PLAST = activeWorkspace->getLastFocusedWindow(); PLAST)
                g_pCompositor->focusWindow(PLAST);
            else
                g_pInputManager->refocus();
        }

        g_pCompositor->updateFullscreenFadeOnWorkspace(activeWorkspace);

        g_pConfigManager->ensureVRR(self.lock());

        g_pCompositor->updateSuspendedStates();

        return;
    }

    if (activeSpecialWorkspace) {
        activeSpecialWorkspace->m_visible = false;
        activeSpecialWorkspace->startAnim(false, false);
    }

    bool animate = true;
    //close if open elsewhere
    const auto PMONITORWORKSPACEOWNER = pWorkspace->m_monitor.lock();
    if (const auto PMWSOWNER = pWorkspace->m_monitor.lock(); PMWSOWNER && PMWSOWNER->activeSpecialWorkspace == pWorkspace) {
        PMWSOWNER->activeSpecialWorkspace.reset();
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMWSOWNER->ID);
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + PMWSOWNER->szName});
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", ",," + PMWSOWNER->szName});

        const auto PACTIVEWORKSPACE = PMWSOWNER->activeWorkspace;
        g_pCompositor->updateFullscreenFadeOnWorkspace(PACTIVEWORKSPACE);

        animate = false;
    }

    // open special
    pWorkspace->m_monitor             = self;
    activeSpecialWorkspace            = pWorkspace;
    activeSpecialWorkspace->m_visible = true;
    if (animate)
        pWorkspace->startAnim(true, true);

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == pWorkspace) {
            w->m_monitor = self;
            w->updateSurfaceScaleTransformDetails();
            w->setAnimationsToMove();

            const auto MIDDLE = w->middle();
            if (w->m_isFloating && !VECINRECT(MIDDLE, vecPosition.x, vecPosition.y, vecPosition.x + vecSize.x, vecPosition.y + vecSize.y) && !w->isX11OverrideRedirect()) {
                // if it's floating and the middle isnt on the current mon, move it to the center
                const auto PMONFROMMIDDLE = g_pCompositor->getMonitorFromVector(MIDDLE);
                Vector2D   pos            = w->m_realPosition->goal();
                if (!VECINRECT(MIDDLE, PMONFROMMIDDLE->vecPosition.x, PMONFROMMIDDLE->vecPosition.y, PMONFROMMIDDLE->vecPosition.x + PMONFROMMIDDLE->vecSize.x,
                               PMONFROMMIDDLE->vecPosition.y + PMONFROMMIDDLE->vecSize.y)) {
                    // not on any monitor, center
                    pos = middle() / 2.f - w->m_realSize->goal() / 2.f;
                } else
                    pos = pos - PMONFROMMIDDLE->vecPosition + vecPosition;

                *w->m_realPosition = pos;
                w->m_position      = pos;
            }
        }
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    if (!(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == self)) {
        if (const auto PLAST = pWorkspace->getLastFocusedWindow(); PLAST)
            g_pCompositor->focusWindow(PLAST);
        else
            g_pInputManager->refocus();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", pWorkspace->m_name + "," + szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", std::to_string(pWorkspace->m_id) + "," + pWorkspace->m_name + "," + szName});

    g_pHyprRenderer->damageMonitor(self.lock());

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(self.lock());

    g_pCompositor->updateSuspendedStates();
}

void CMonitor::setSpecialWorkspace(const WORKSPACEID& id) {
    setSpecialWorkspace(g_pCompositor->getWorkspaceByID(id));
}

void CMonitor::moveTo(const Vector2D& pos) {
    vecPosition = pos;
}

SWorkspaceIDName CMonitor::getPrevWorkspaceIDName(const WORKSPACEID id) {
    while (!prevWorkSpaces.empty()) {
        const int PREVID = prevWorkSpaces.top();
        prevWorkSpaces.pop();
        if (PREVID == id) // skip same workspace
            continue;

        // recheck if previous workspace's was moved to another monitor
        const auto ws = g_pCompositor->getWorkspaceByID(PREVID);
        if (ws && ws->monitorID() == ID)
            return {.id = PREVID, .name = ws->m_name};
    }

    return {.id = WORKSPACE_INVALID};
}

void CMonitor::addPrevWorkspaceID(const WORKSPACEID id) {
    if (!prevWorkSpaces.empty() && prevWorkSpaces.top() == id)
        return;

    prevWorkSpaces.emplace(id);
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
    return activeWorkspace ? activeWorkspace->m_id : 0;
}

WORKSPACEID CMonitor::activeSpecialWorkspaceID() {
    return activeSpecialWorkspace ? activeSpecialWorkspace->m_id : 0;
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

void CMonitor::setCTM(const Mat3x3& ctm_) {
    ctm        = ctm_;
    ctmUpdated = true;
    g_pCompositor->scheduleFrameForMonitor(self.lock(), Aquamarine::IOutput::scheduleFrameReason::AQ_SCHEDULE_NEEDS_FRAME);
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

    if (!PSURFACE || !PSURFACE->current.texture || !PSURFACE->current.buffer)
        return false;

    if (PSURFACE->current.bufferSize != vecPixelSize || PSURFACE->current.transform != transform)
        return false;

    // we can't scanout shm buffers.
    const auto params = PSURFACE->current.buffer->dmabuf();
    if (!params.success || !PSURFACE->current.texture->m_pEglImage /* dmabuf */)
        return false;

    Debug::log(TRACE, "attemptDirectScanout: surface {:x} passed, will attempt, buffer {}", (uintptr_t)PSURFACE.get(), (uintptr_t)PSURFACE->current.buffer.buffer.get());

    auto PBUFFER = PSURFACE->current.buffer.buffer;

    if (PBUFFER == output->state->state().buffer) {
        PSURFACE->presentFeedback(Time::steadyNow(), self.lock());

        if (scanoutNeedsCursorUpdate) {
            if (!state.test()) {
                Debug::log(TRACE, "attemptDirectScanout: failed basic test");
                return false;
            }

            if (!output->commit()) {
                Debug::log(TRACE, "attemptDirectScanout: failed to commit cursor update");
                lastScanout.reset();
                return false;
            }

            scanoutNeedsCursorUpdate = false;
        }

        return true;
    }

    // FIXME: make sure the buffer actually follows the available scanout dmabuf formats
    // and comes from the appropriate device. This may implode on multi-gpu!!

    // entering into scanout, so save monitor format
    if (lastScanout.expired())
        prevDrmFormat = drmFormat;

    if (drmFormat != params.format) {
        output->state->setFormat(params.format);
        drmFormat = params.format;
    }

    output->state->setBuffer(PBUFFER);
    output->state->setPresentationMode(tearingState.activelyTearing ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE :
                                                                      Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC);

    if (!state.test()) {
        Debug::log(TRACE, "attemptDirectScanout: failed basic test");
        return false;
    }

    PSURFACE->presentFeedback(Time::steadyNow(), self.lock());

    output->state->addDamage(PSURFACE->current.accumulateBufferDamage());
    output->state->resetExplicitFences();

    // no need to do explicit sync here as surface current can only ever be ready to read

    bool ok = output->commit();

    if (!ok) {
        Debug::log(TRACE, "attemptDirectScanout: failed to scanout surface");
        lastScanout.reset();
        return false;
    }

    if (lastScanout.expired()) {
        lastScanout = PCANDIDATE;
        Debug::log(LOG, "Entered a direct scanout to {:x}: \"{}\"", (uintptr_t)PCANDIDATE.get(), PCANDIDATE->m_title);
    }

    scanoutNeedsCursorUpdate = false;

    if (!PBUFFER->lockedByBackend || PBUFFER->hlEvents.backendRelease)
        return true;

    // lock buffer while DRM/KMS is using it, then release it when page flip happens since DRM/KMS should be done by then
    // btw buffer's syncReleaser will take care of signaling release point, so we don't do that here
    PBUFFER->lock();
    PBUFFER->onBackendRelease([PBUFFER]() { PBUFFER->unlock(); });

    return true;
}

void CMonitor::debugLastPresentation(const std::string& message) {
    Debug::log(TRACE, "{} (last presentation {} - {} fps)", message, lastPresentationTimer.getMillis(),
               lastPresentationTimer.getMillis() > 0 ? 1000.0f / lastPresentationTimer.getMillis() : 0.0f);
}

void CMonitor::onMonitorFrame() {
    if ((g_pCompositor->m_aqBackend->hasSession() && !g_pCompositor->m_aqBackend->session->active) || !g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");

        if (g_pCompositor->m_unsafeState &&
            std::ranges::any_of(g_pCompositor->m_monitors.begin(), g_pCompositor->m_monitors.end(), [&](auto& m) { return m->output != g_pCompositor->m_unsafeOutput->output; })) {
            // restore from unsafe state
            g_pCompositor->leaveUnsafeState();
        }

        return; // cannot draw on session inactive (different tty)
    }

    if (!m_bEnabled)
        return;

    g_pHyprRenderer->recheckSolitaryForMonitor(self.lock());

    tearingState.busy = false;

    if (tearingState.activelyTearing && solitaryClient.lock() /* can be invalidated by a recheck */) {

        if (!tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        tearingState.nextRenderTorn          = true;
        tearingState.frameScheduledWhileBusy = false;
    }

    static auto PENABLERAT = CConfigValue<Hyprlang::INT>("misc:render_ahead_of_time");
    static auto PRATSAFE   = CConfigValue<Hyprlang::INT>("misc:render_ahead_safezone");

    lastPresentationTimer.reset();

    if (*PENABLERAT && !tearingState.nextRenderTorn) {
        if (!RATScheduled) {
            // render
            g_pHyprRenderer->renderMonitor(self.lock());
        }

        RATScheduled = false;

        const auto& [avg, max, min] = g_pHyprRenderer->getRenderTimes(self.lock());

        if (max + *PRATSAFE > 1000.0 / refreshRate)
            return;

        const auto MSLEFT = 1000.0 / refreshRate - lastPresentationTimer.getMillis();

        RATScheduled = true;

        const auto ESTRENDERTIME = std::ceil(avg + *PRATSAFE);
        const auto TIMETOSLEEP   = std::floor(MSLEFT - ESTRENDERTIME);

        if (MSLEFT < 1 || MSLEFT < ESTRENDERTIME || TIMETOSLEEP < 1)
            g_pHyprRenderer->renderMonitor(self.lock());
        else
            wl_event_source_timer_update(renderTimer, TIMETOSLEEP);
    } else
        g_pHyprRenderer->renderMonitor(self.lock());
}

void CMonitor::onCursorMovedOnMonitor() {
    if (!tearingState.activelyTearing || !solitaryClient || !g_pHyprRenderer->shouldRenderCursor())
        return;

    // submit a frame immediately. This will only update the cursor pos.
    // output->state->setBuffer(output->state->state().buffer);
    // output->state->addDamage(CRegion{});
    // output->state->setPresentationMode(Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE);
    // if (!output->commit())
    //     Debug::log(ERR, "onCursorMovedOnMonitor: tearing and wanted to update cursor, failed.");

    // FIXME: try to do the above. We currently can't just render because drm is a fucking bitch
    // and throws a "nO pRoP cAn Be ChAnGeD dUrInG AsYnC fLiP" on crtc_x
    // this will throw too but fix it if we use sw cursors

    tearingState.frameScheduledWhileBusy = true;
}

CMonitorState::CMonitorState(CMonitor* owner) : m_pOwner(owner) {
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

    EMIT_HOOK_EVENT("preMonitorCommit", m_pOwner->self.lock());

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
