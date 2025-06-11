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
#include "../managers/AnimationManager.hpp"
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
#include <vector>
#include <algorithm>

using namespace Hyprutils::String;
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;

static int ratHandler(void* data) {
    g_pHyprRenderer->renderMonitor(((CMonitor*)data)->m_self.lock());

    return 1;
}

CMonitor::CMonitor(SP<Aquamarine::IOutput> output_) : m_state(this), m_output(output_) {
    g_pAnimationManager->createAnimation(0.f, m_specialFade, g_pConfigManager->getAnimationPropertyConfig("specialWorkspaceIn"), AVARDAMAGE_NONE);
    m_specialFade->setUpdateCallback([this](auto) { g_pHyprRenderer->damageMonitor(m_self.lock()); });
}

CMonitor::~CMonitor() {
    m_events.destroy.emit();
    if (g_pHyprOpenGL)
        g_pHyprOpenGL->destroyMonitorResources(m_self);
}

void CMonitor::onConnect(bool noRule) {
    EMIT_HOOK_EVENT("preMonitorAdded", m_self.lock());
    CScopeGuard x = {[]() { g_pCompositor->arrangeMonitors(); }};

    g_pEventLoopManager->doLater([] { g_pConfigManager->ensurePersistentWorkspacesPresent(); });

    m_listeners.frame  = m_output->events.frame.registerListener([this](std::any d) { onMonitorFrame(); });
    m_listeners.commit = m_output->events.commit.registerListener([this](std::any d) {
        if (true) { // FIXME: E->state->committed & WLR_OUTPUT_STATE_BUFFER
            PROTO::screencopy->onOutputCommit(m_self.lock());
            PROTO::toplevelExport->onOutputCommit(m_self.lock());
        }
    });
    m_listeners.needsFrame =
        m_output->events.needsFrame.registerListener([this](std::any d) { g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME); });

    m_listeners.presented = m_output->events.present.registerListener([this](std::any d) {
        auto      E = std::any_cast<Aquamarine::IOutput::SPresentEvent>(d);

        timespec* ts = E.when;

        if (ts && ts->tv_sec <= 2) {
            // drop this timestamp, it's not valid. Likely drm is cringe. We can't push it further because
            // a) it's wrong, b) our translations aren't 100% accurate and risk underflows
            ts = nullptr;
        }

        if (!ts)
            PROTO::presentation->onPresented(m_self.lock(), Time::steadyNow(), E.refresh, E.seq, E.flags);
        else
            PROTO::presentation->onPresented(m_self.lock(), Time::fromTimespec(E.when), E.refresh, E.seq, E.flags);
    });

    m_listeners.destroy = m_output->events.destroy.registerListener([this](std::any d) {
        Debug::log(LOG, "Destroy called for monitor {}", m_name);

        onDisconnect(true);

        m_output              = nullptr;
        m_renderingInitPassed = false;

        Debug::log(LOG, "Removing monitor {} from realMonitors", m_name);

        std::erase_if(g_pCompositor->m_realMonitors, [&](PHLMONITOR& el) { return el.get() == this; });
    });

    m_listeners.state = m_output->events.state.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IOutput::SStateEvent>(d);

        if (E.size == Vector2D{}) {
            // an indication to re-set state
            // we can't do much for createdByUser displays I think
            if (m_createdByUser)
                return;

            Debug::log(LOG, "Reapplying monitor rule for {} from a state request", m_name);
            applyMonitorRule(&m_activeMonitorRule, true);
            return;
        }

        if (!m_createdByUser)
            return;

        const auto SIZE = E.size;

        m_forceSize = SIZE;

        SMonitorRule rule = m_activeMonitorRule;
        rule.resolution   = SIZE;

        applyMonitorRule(&rule);
    });

    m_tearingState.canTear = m_output->getBackend()->type() == Aquamarine::AQ_BACKEND_DRM;

    m_name = m_output->name;

    m_description = m_output->description;
    // remove comma character from description. This allow monitor specific rules to work on monitor with comma on their description
    std::erase(m_description, ',');

    // field is backwards-compatible with intended usage of `szDescription` but excludes the parenthesized DRM node name suffix
    m_shortDescription = trim(std::format("{} {} {}", m_output->make, m_output->model, m_output->serial));
    std::erase(m_shortDescription, ',');

    if (m_output->getBackend()->type() != Aquamarine::AQ_BACKEND_DRM)
        m_createdByUser = true; // should be true. WL and Headless backends should be addable / removable

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(m_self.lock());

    if (m_enabled && !monitorRule.disabled) {
        applyMonitorRule(&monitorRule, m_pixelSize == Vector2D{});

        m_output->state->resetExplicitFences();
        m_output->state->setEnabled(true);
        m_state.commit();
        return;
    }

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {

        m_output->state->resetExplicitFences();
        m_output->state->setEnabled(false);

        if (!m_state.commit())
            Debug::log(ERR, "Couldn't commit disabled state on output {}", m_output->name);

        m_enabled = false;

        m_listeners.frame.reset();
        return;
    }

    if (m_output->nonDesktop) {
        Debug::log(LOG, "Not configuring non-desktop output");
        if (PROTO::lease)
            PROTO::lease->offer(m_self.lock());

        return;
    }

    PHLMONITOR* thisWrapper = nullptr;

    // find the wrap
    for (auto& m : g_pCompositor->m_realMonitors) {
        if (m->m_id == m_id) {
            thisWrapper = &m;
            break;
        }
    }

    RASSERT(thisWrapper->get(), "CMonitor::onConnect: Had no wrapper???");

    if (std::ranges::find_if(g_pCompositor->m_monitors, [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_monitors.end())
        g_pCompositor->m_monitors.push_back(*thisWrapper);

    m_enabled = true;

    m_output->state->resetExplicitFences();
    m_output->state->setEnabled(true);

    // set mode, also applies
    if (!noRule)
        applyMonitorRule(&monitorRule, true);

    if (!m_state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onCommit");

    m_damage.setSize(m_transformedSize);

    Debug::log(LOG, "Added new monitor with name {} at {:j0} with size {:j0}, pointer {:x}", m_output->name, m_position, m_pixelSize, (uintptr_t)m_output.get());

    setupDefaultWS(monitorRule);

    for (auto const& ws : g_pCompositor->m_workspaces) {
        if (!valid(ws))
            continue;

        if (ws->m_lastMonitor == m_name || g_pCompositor->m_monitors.size() == 1 /* avoid lost workspaces on recover */) {
            g_pCompositor->moveWorkspaceToMonitor(ws, m_self.lock());
            ws->startAnim(true, true, true);
            ws->m_lastMonitor = "";
        }
    }

    m_scale = monitorRule.scale;
    if (m_scale < 0.1)
        m_scale = getDefaultScale();

    m_forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.
    //

    if (!m_activeMonitorRule.mirrorOf.empty())
        setMirror(m_activeMonitorRule.mirrorOf);

    if (!g_pCompositor->m_lastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->setActiveMonitor(m_self.lock());

    g_pHyprRenderer->arrangeLayersForMonitor(m_id);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_id);

    // ensure VRR (will enable if necessary)
    g_pConfigManager->ensureVRR(m_self.lock());

    // verify last mon valid
    bool found = false;
    for (auto const& m : g_pCompositor->m_monitors) {
        if (m == g_pCompositor->m_lastMonitor) {
            found = true;
            break;
        }
    }

    Debug::log(LOG, "checking if we have seen this monitor before: {}", m_name);
    // if we saw this monitor before, set it to the workspace it was on
    if (g_pCompositor->m_seenMonitorWorkspaceMap.contains(m_name)) {
        auto workspaceID = g_pCompositor->m_seenMonitorWorkspaceMap[m_name];
        Debug::log(LOG, "Monitor {} was on workspace {}, setting it to that", m_name, workspaceID);
        auto ws = g_pCompositor->getWorkspaceByID(workspaceID);
        if (ws) {
            g_pCompositor->moveWorkspaceToMonitor(ws, m_self.lock());
            changeWorkspace(ws, true, false, false);
        }
    } else
        Debug::log(LOG, "Monitor {} was not on any workspace", m_name);

    if (!found)
        g_pCompositor->setActiveMonitor(m_self.lock());

    m_renderTimer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, ratHandler, this);

    g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_NEW_MONITOR);

    PROTO::gamma->applyGammaToState(m_self.lock());

    m_events.connect.emit();

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{"monitoraddedv2", std::format("{},{},{}", m_id, m_name, m_shortDescription)});
    EMIT_HOOK_EVENT("monitorAdded", m_self.lock());
}

void CMonitor::onDisconnect(bool destroy) {
    EMIT_HOOK_EVENT("preMonitorRemoved", m_self.lock());
    CScopeGuard x = {[this]() {
        if (g_pCompositor->m_isShuttingDown)
            return;
        g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{"monitorremovedv2", std::format("{},{},{}", m_id, m_name, m_shortDescription)});
        EMIT_HOOK_EVENT("monitorRemoved", m_self.lock());
        g_pCompositor->arrangeMonitors();
    }};

    if (m_renderTimer) {
        wl_event_source_remove(m_renderTimer);
        m_renderTimer = nullptr;
    }

    if (!m_enabled || g_pCompositor->m_isShuttingDown)
        return;

    Debug::log(LOG, "onDisconnect called for {}", m_output->name);

    m_events.disconnect.emit();
    if (g_pHyprOpenGL)
        g_pHyprOpenGL->destroyMonitorResources(m_self);

    // record what workspace this monitor was on
    if (m_activeWorkspace) {
        Debug::log(LOG, "Disconnecting Monitor {} was on workspace {}", m_name, m_activeWorkspace->m_id);
        g_pCompositor->m_seenMonitorWorkspaceMap[m_name] = m_activeWorkspace->m_id;
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
    if (m_mirrorOf) {
        m_mirrorOf->m_mirrors.erase(std::ranges::find_if(m_mirrorOf->m_mirrors, [&](const auto& other) { return other == m_self; }));

        // unlock software for mirrored monitor
        g_pPointerManager->unlockSoftwareForMonitor(m_mirrorOf.lock());
        m_mirrorOf.reset();
    }

    if (!m_mirrors.empty()) {
        for (auto const& m : m_mirrors) {
            m->setMirror("");
        }

        g_pConfigManager->m_wantsMonitorReload = true;
    }

    m_listeners.frame.reset();
    m_listeners.presented.reset();
    m_listeners.needsFrame.reset();
    m_listeners.commit.reset();

    for (size_t i = 0; i < 4; ++i) {
        for (auto const& ls : m_layerSurfaceLayers[i]) {
            if (ls->m_layerSurface && !ls->m_fadingOut)
                ls->m_layerSurface->sendClosed();
        }
        m_layerSurfaceLayers[i].clear();
    }

    Debug::log(LOG, "Removed monitor {}!", m_name);

    if (!BACKUPMON) {
        Debug::log(WARN, "Unplugged last monitor, entering an unsafe state. Good luck my friend.");
        g_pCompositor->enterUnsafeState();
    }

    m_enabled             = false;
    m_renderingInitPassed = false;

    if (BACKUPMON) {
        // snap cursor
        g_pCompositor->warpCursorTo(BACKUPMON->m_position + BACKUPMON->m_transformedSize / 2.F, true);

        // move workspaces
        std::vector<PHLWORKSPACE> wspToMove;
        for (auto const& w : g_pCompositor->m_workspaces) {
            if (w->m_monitor == m_self || !w->m_monitor)
                wspToMove.push_back(w);
        }

        for (auto const& w : wspToMove) {
            w->m_lastMonitor = m_name;
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }
    } else {
        g_pCompositor->m_lastFocus.reset();
        g_pCompositor->m_lastWindow.reset();
        g_pCompositor->m_lastMonitor.reset();
    }

    if (m_activeWorkspace)
        m_activeWorkspace->m_visible = false;
    m_activeWorkspace.reset();

    m_output->state->resetExplicitFences();
    m_output->state->setAdaptiveSync(false);
    m_output->state->setEnabled(false);

    if (!m_state.commit())
        Debug::log(WARN, "state.commit() failed in CMonitor::onDisconnect");

    if (g_pCompositor->m_lastMonitor == m_self)
        g_pCompositor->setActiveMonitor(BACKUPMON ? BACKUPMON : g_pCompositor->m_unsafeOutput.lock());

    if (g_pHyprRenderer->m_mostHzMonitor == m_self) {
        int        mostHz         = 0;
        PHLMONITOR pMonitorMostHz = nullptr;

        for (auto const& m : g_pCompositor->m_monitors) {
            if (m->m_refreshRate > mostHz && m != m_self) {
                pMonitorMostHz = m;
                mostHz         = m->m_refreshRate;
            }
        }

        g_pHyprRenderer->m_mostHzMonitor = pMonitorMostHz;
    }
    std::erase_if(g_pCompositor->m_monitors, [&](PHLMONITOR& el) { return el.get() == this; });
}

bool CMonitor::applyMonitorRule(SMonitorRule* pMonitorRule, bool force) {

    static auto PDISABLESCALECHECKS = CConfigValue<Hyprlang::INT>("debug:disable_scale_checks");

    Debug::log(LOG, "Applying monitor rule for {}", m_name);

    m_activeMonitorRule = *pMonitorRule;

    if (m_forceSize.has_value())
        m_activeMonitorRule.resolution = m_forceSize.value();

    const auto RULE = &m_activeMonitorRule;

    // if it's disabled, disable and ignore
    if (RULE->disabled) {
        if (m_enabled)
            onDisconnect();

        m_events.modeChanged.emit();

        return true;
    }

    // don't touch VR headsets
    if (m_output->nonDesktop)
        return true;

    if (!m_enabled) {
        onConnect(true); // enable it.
        Debug::log(LOG, "Monitor {} is disabled but is requested to be enabled", m_name);
        force = true;
    }

    // Check if the rule isn't already applied
    // TODO: clean this up lol
    if (!force && DELTALESSTHAN(m_pixelSize.x, RULE->resolution.x, 1) /* ↓ */
        && DELTALESSTHAN(m_pixelSize.y, RULE->resolution.y, 1)        /* Resolution is the same */
        && m_pixelSize.x > 1 && m_pixelSize.y > 1                     /* Active resolution is not invalid */
        && DELTALESSTHAN(m_refreshRate, RULE->refreshRate, 1)         /* Refresh rate is the same */
        && m_setScale == RULE->scale                                  /* Scale is the same */
        && m_autoDir == RULE->autoDir                                 /* Auto direction is the same */
        /* position is set correctly */
        && ((DELTALESSTHAN(m_position.x, RULE->offset.x, 1) && DELTALESSTHAN(m_position.y, RULE->offset.y, 1)) || RULE->offset == Vector2D(-INT32_MAX, -INT32_MAX))
        /* other properties hadnt changed */
        && m_transform == RULE->transform && RULE->enable10bit == m_enabled10bit && RULE->cmType == m_cmType && RULE->sdrSaturation == m_sdrSaturation &&
        RULE->sdrBrightness == m_sdrBrightness && !std::memcmp(&m_customDrmMode, &RULE->drmMode, sizeof(m_customDrmMode))) {

        Debug::log(LOG, "Not applying a new rule to {} because it's already applied!", m_name);

        setMirror(RULE->mirrorOf);

        return true;
    }

    bool autoScale = false;

    if (RULE->scale > 0.1) {
        m_scale = RULE->scale;
    } else {
        autoScale               = true;
        const auto DEFAULTSCALE = getDefaultScale();
        m_scale                 = DEFAULTSCALE;
    }

    m_setScale  = m_scale;
    m_transform = RULE->transform;
    m_autoDir   = RULE->autoDir;

    // accumulate requested modes in reverse order (cause inesrting at front is inefficient)
    std::vector<SP<Aquamarine::SOutputMode>> requestedModes;
    std::string                              requestedStr = "unknown";

    // use sortFunc, add best 3 to requestedModes in reverse, since we test in reverse
    auto addBest3Modes = [&](auto const& sortFunc) {
        auto sortedModes = m_output->modes;
        std::ranges::sort(sortedModes, sortFunc);
        if (sortedModes.size() > 3)
            sortedModes.erase(sortedModes.begin() + 3, sortedModes.end());
        requestedModes.insert_range(requestedModes.end(), sortedModes | std::views::reverse);
    };

    // last fallback is always preferred mode
    if (!m_output->preferredMode())
        Debug::log(ERR, "Monitor {} has NO PREFERRED MODE", m_output->name);
    else
        requestedModes.push_back(m_output->preferredMode());

    if (RULE->resolution == Vector2D()) {
        requestedStr = "preferred";

        // fallback to first 3 modes if preferred fails/doesn't exist
        requestedModes = m_output->modes;
        if (requestedModes.size() > 3)
            requestedModes.erase(requestedModes.begin() + 3, requestedModes.end());
        std::ranges::reverse(requestedModes.begin(), requestedModes.end());

        if (m_output->preferredMode())
            requestedModes.push_back(m_output->preferredMode());
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

        // sort prioritizing resolution 1st and refresh rate 2nd, then add best 3
        addBest3Modes([](auto const& a, auto const& b) {
            if (a->pixelSize.x > b->pixelSize.x && a->pixelSize.y > b->pixelSize.y)
                return true;
            else if (DELTALESSTHAN(a->pixelSize.x, b->pixelSize.x, 1) && DELTALESSTHAN(a->pixelSize.y, b->pixelSize.y, 1) &&
                     std::round(a->refreshRate) > std::round(b->refreshRate))
                return true;
            return false;
        });
    } else if (RULE->resolution == Vector2D(-1, -3)) {
        requestedStr = "maxwidth";

        // sort prioritizing widest resolution 1st and refresh rate 2nd, then add best 3
        addBest3Modes([](auto const& a, auto const& b) {
            if (a->pixelSize.x > b->pixelSize.x)
                return true;
            if (a->pixelSize.x == b->pixelSize.x && std::round(a->refreshRate) > std::round(b->refreshRate))
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
            if (m_output->getBackend()->type() != Aquamarine::eBackendType::AQ_BACKEND_DRM)
                Debug::log(ERR, "Tried to set custom modeline on non-DRM output");
            else
                requestedModes.push_back(makeShared<Aquamarine::SOutputMode>(
                    Aquamarine::SOutputMode{.pixelSize = {RULE->drmMode.hdisplay, RULE->drmMode.vdisplay}, .refreshRate = RULE->drmMode.vrefresh, .modeInfo = RULE->drmMode}));
        }
    }

    const auto WAS10B  = m_enabled10bit;
    const auto OLDRES  = m_pixelSize;
    bool       success = false;

    // Needed in case we are switching from a custom modeline to a standard mode
    m_customDrmMode = {};
    m_currentMode   = nullptr;

    m_output->state->setFormat(DRM_FORMAT_XRGB8888);
    m_prevDrmFormat = m_drmFormat;
    m_drmFormat     = DRM_FORMAT_XRGB8888;
    m_output->state->resetExplicitFences();

    if (Debug::m_trace) {
        Debug::log(TRACE, "Monitor {} requested modes:", m_name);
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
            m_output->state->setCustomMode(mode);

            if (!m_state.test()) {
                Debug::log(ERR, "Monitor {}: REJECTED custom mode {}!", m_name, modeStr);
                continue;
            }

            m_customDrmMode = mode->modeInfo.value();
        } else {
            m_output->state->setMode(mode);

            if (!m_state.test()) {
                Debug::log(ERR, "Monitor {}: REJECTED available mode {}!", m_name, modeStr);
                if (mode->preferred)
                    Debug::log(ERR, "Monitor {}: REJECTED preferred mode!!!", m_name);
                continue;
            }

            m_customDrmMode = {};
        }

        m_refreshRate = mode->refreshRate / 1000.f;
        m_size        = mode->pixelSize;
        m_currentMode = mode;

        success = true;

        if (mode->preferred)
            Debug::log(LOG, "Monitor {}: requested {}, using preferred mode {}", m_name, requestedStr, modeStr);
        else if (mode->modeInfo.has_value() && mode->modeInfo->type == DRM_MODE_TYPE_USERDEF)
            Debug::log(LOG, "Monitor {}: requested {}, using custom mode {}", m_name, requestedStr, modeStr);
        else
            Debug::log(LOG, "Monitor {}: requested {}, using available mode {}", m_name, requestedStr, modeStr);

        break;
    }

    // try requested as custom mode jic it works
    if (!success && RULE->resolution != Vector2D() && RULE->resolution != Vector2D(-1, -1) && RULE->resolution != Vector2D(-1, -2)) {
        auto        refreshRate = m_output->getBackend()->type() == Aquamarine::eBackendType::AQ_BACKEND_DRM ? RULE->refreshRate * 1000 : 0;
        auto        mode        = makeShared<Aquamarine::SOutputMode>(Aquamarine::SOutputMode{.pixelSize = RULE->resolution, .refreshRate = refreshRate});
        std::string modeStr     = std::format("{:X0}@{:.2f}Hz", mode->pixelSize, mode->refreshRate / 1000.f);

        m_output->state->setCustomMode(mode);

        if (m_state.test()) {
            Debug::log(LOG, "Monitor {}: requested {}, using custom mode {}", m_name, requestedStr, modeStr);

            refreshRate     = mode->refreshRate / 1000.f;
            m_size          = mode->pixelSize;
            m_currentMode   = mode;
            m_customDrmMode = {};

            success = true;
        } else
            Debug::log(ERR, "Monitor {}: REJECTED custom mode {}!", m_name, modeStr);
    }

    // try any of the modes if none of the above work
    if (!success) {
        for (auto const& mode : m_output->modes) {
            m_output->state->setMode(mode);

            if (!m_state.test())
                continue;

            auto errorMessage =
                std::format("Monitor {} failed to set any requested modes, falling back to mode {:X0}@{:.2f}Hz", m_name, mode->pixelSize, mode->refreshRate / 1000.f);
            Debug::log(WARN, errorMessage);
            g_pHyprNotificationOverlay->addNotification(errorMessage, CHyprColor(0xff0000ff), 5000, ICON_WARNING);

            m_refreshRate   = mode->refreshRate / 1000.f;
            m_size          = mode->pixelSize;
            m_currentMode   = mode;
            m_customDrmMode = {};

            success = true;

            break;
        }
    }

    if (!success) {
        Debug::log(ERR, "Monitor {} has NO FALLBACK MODES, and an INVALID one was requested: {:X0}@{:.2f}Hz", m_name, RULE->resolution, RULE->refreshRate);
        return true;
    }

    m_vrrActive = m_output->state->state().adaptiveSync // disabled here, will be tested in CConfigManager::ensureVRR()
        || m_createdByUser;                             // wayland backend doesn't allow for disabling adaptive_sync

    m_pixelSize = m_size;

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
        m_output->state->setFormat(fmt.second);
        m_prevDrmFormat = m_drmFormat;
        m_drmFormat     = fmt.second;

        if (!m_state.test()) {
            Debug::log(ERR, "output {} failed basic test on format {}", m_name, fmt.first);
        } else {
            Debug::log(LOG, "output {} succeeded basic test on format {}", m_name, fmt.first);
            if (RULE->enable10bit && fmt.first.contains("101010"))
                set10bit = true;
            break;
        }
    }

    m_enabled10bit = set10bit;

    auto oldImageDescription = m_imageDescription;
    m_cmType                 = RULE->cmType;
    switch (m_cmType) {
        case CM_AUTO: m_cmType = m_enabled10bit && m_output->parsedEDID.supportsBT2020 ? CM_WIDE : CM_SRGB; break;
        case CM_EDID: m_cmType = m_output->parsedEDID.chromaticityCoords.has_value() ? CM_EDID : CM_SRGB; break;
        case CM_HDR:
        case CM_HDR_EDID:
            m_cmType = m_output->parsedEDID.supportsBT2020 && m_output->parsedEDID.hdrMetadata.has_value() && m_output->parsedEDID.hdrMetadata->supportsPQ ? m_cmType : CM_SRGB;
            break;
        default: break;
    }
    switch (m_cmType) {
        case CM_SRGB: m_imageDescription = {}; break; // assumes SImageDescirption defaults to sRGB
        case CM_WIDE:
            m_imageDescription = {.primariesNameSet = true,
                                  .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                  .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020)};
            break;
        case CM_EDID:
            m_imageDescription = {.primariesNameSet = false,
                                  .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                  .primaries        = {
                                             .red   = {.x = m_output->parsedEDID.chromaticityCoords->red.x, .y = m_output->parsedEDID.chromaticityCoords->red.y},
                                             .green = {.x = m_output->parsedEDID.chromaticityCoords->green.x, .y = m_output->parsedEDID.chromaticityCoords->green.y},
                                             .blue  = {.x = m_output->parsedEDID.chromaticityCoords->blue.x, .y = m_output->parsedEDID.chromaticityCoords->blue.y},
                                             .white = {.x = m_output->parsedEDID.chromaticityCoords->white.x, .y = m_output->parsedEDID.chromaticityCoords->white.y},
                                  }};
            break;
        case CM_HDR:
            m_imageDescription = {.transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
                                  .primariesNameSet = true,
                                  .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                  .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
                                  .luminances       = {.min = 0, .max = 10000, .reference = 203}};
            break;
        case CM_HDR_EDID:
            m_imageDescription = {.transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
                                  .primariesNameSet = false,
                                  .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
                                  .primaries        = m_output->parsedEDID.chromaticityCoords.has_value() ?
                                             NColorManagement::SPCPRimaries{
                                                 .red   = {.x = m_output->parsedEDID.chromaticityCoords->red.x, .y = m_output->parsedEDID.chromaticityCoords->red.y},
                                                 .green = {.x = m_output->parsedEDID.chromaticityCoords->green.x, .y = m_output->parsedEDID.chromaticityCoords->green.y},
                                                 .blue  = {.x = m_output->parsedEDID.chromaticityCoords->blue.x, .y = m_output->parsedEDID.chromaticityCoords->blue.y},
                                                 .white = {.x = m_output->parsedEDID.chromaticityCoords->white.x, .y = m_output->parsedEDID.chromaticityCoords->white.y},
                                      } :
                                             NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
                                  .luminances       = {.min       = m_output->parsedEDID.hdrMetadata->desiredContentMinLuminance,
                                                       .max       = m_output->parsedEDID.hdrMetadata->desiredContentMaxLuminance,
                                                       .reference = m_output->parsedEDID.hdrMetadata->desiredMaxFrameAverageLuminance}};

            break;
        default: UNREACHABLE();
    }
    if (oldImageDescription != m_imageDescription)
        PROTO::colorManagement->onMonitorImageDescriptionChanged(m_self);

    m_sdrSaturation = RULE->sdrSaturation;
    m_sdrBrightness = RULE->sdrBrightness;

    Vector2D logicalSize = m_pixelSize / m_scale;
    if (!*PDISABLESCALECHECKS && (logicalSize.x != std::round(logicalSize.x) || logicalSize.y != std::round(logicalSize.y))) {
        // invalid scale, will produce fractional pixels.
        // find the nearest valid.

        float    searchScale = std::round(m_scale * 120.0);
        bool     found       = false;

        double   scaleZero = searchScale / 120.0;

        Vector2D logicalZero = m_pixelSize / scaleZero;
        if (logicalZero == logicalZero.round())
            m_scale = scaleZero;
        else {
            for (size_t i = 1; i < 90; ++i) {
                double   scaleUp   = (searchScale + i) / 120.0;
                double   scaleDown = (searchScale - i) / 120.0;

                Vector2D logicalUp   = m_pixelSize / scaleUp;
                Vector2D logicalDown = m_pixelSize / scaleDown;

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
                    m_scale = std::round(scaleZero);
                else {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} failed to find a clean divisor", m_scale);
                    g_pConfigManager->addParseError("Invalid scale passed to monitor " + m_name + ", failed to find a clean divisor");
                    m_scale = getDefaultScale();
                }
            } else {
                if (!autoScale) {
                    Debug::log(ERR, "Invalid scale passed to monitor, {} found suggestion {}", m_scale, searchScale);
                    g_pConfigManager->addParseError(
                        std::format("Invalid scale passed to monitor {}, failed to find a clean divisor. Suggested nearest scale: {:5f}", m_name, searchScale));
                    m_scale = getDefaultScale();
                } else
                    m_scale = searchScale;
            }
        }
    }

    m_output->scheduleFrame();

    if (!m_state.commit())
        Debug::log(ERR, "Couldn't commit output named {}", m_output->name);

    Vector2D xfmd     = m_transform % 2 == 1 ? Vector2D{m_pixelSize.y, m_pixelSize.x} : m_pixelSize;
    m_size            = (xfmd / m_scale).round();
    m_transformedSize = xfmd;

    if (m_createdByUser) {
        CBox transformedBox = {0, 0, m_transformedSize.x, m_transformedSize.y};
        transformedBox.transform(wlTransformToHyprutils(invertTransform(m_transform)), m_transformedSize.x, m_transformedSize.y);

        m_pixelSize = Vector2D(transformedBox.width, transformedBox.height);
    }

    updateMatrix();

    if (WAS10B != m_enabled10bit || OLDRES != m_pixelSize)
        g_pHyprOpenGL->destroyMonitorResources(m_self);

    g_pCompositor->arrangeMonitors();

    m_damage.setSize(m_transformedSize);

    // Set scale for all surfaces on this monitor, needed for some clients
    // but not on unsafe state to avoid crashes
    if (!g_pCompositor->m_unsafeState) {
        for (auto const& w : g_pCompositor->m_windows) {
            w->updateSurfaceScaleTransformDetails();
        }
    }
    // updato us
    g_pHyprRenderer->arrangeLayersForMonitor(m_id);

    // reload to fix mirrors
    g_pConfigManager->m_wantsMonitorReload = true;

    Debug::log(LOG, "Monitor {} data dump: res {:X}@{:.2f}Hz, scale {:.2f}, transform {}, pos {:X}, 10b {}", m_name, m_pixelSize, m_refreshRate, m_scale, (int)m_transform,
               m_position, (int)m_enabled10bit);

    EMIT_HOOK_EVENT("monitorLayoutChanged", nullptr);

    m_events.modeChanged.emit();

    return true;
}

void CMonitor::addDamage(const pixman_region32_t* rg) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == m_self) {
        m_damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    } else if (m_damage.damage(rg))
        g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

void CMonitor::addDamage(const CRegion& rg) {
    addDamage(const_cast<CRegion*>(&rg)->pixman());
}

void CMonitor::addDamage(const CBox& box) {
    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    if (*PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == m_self) {
        m_damage.damageEntire();
        g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    }

    if (m_damage.damage(box))
        g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
}

bool CMonitor::shouldSkipScheduleFrameOnMouseEvent() {
    static auto PNOBREAK = CConfigValue<Hyprlang::INT>("cursor:no_break_fs_vrr");
    static auto PMINRR   = CConfigValue<Hyprlang::INT>("cursor:min_refresh_rate");

    // skip scheduling extra frames for fullsreen apps with vrr
    const bool shouldSkip = m_activeWorkspace && m_activeWorkspace->m_hasFullscreenWindow && m_activeWorkspace->m_fullscreenMode == FSMODE_FULLSCREEN &&
        (*PNOBREAK == 1 || (*PNOBREAK == 2 && m_activeWorkspace->getFullscreenWindow()->getContentType() == CONTENT_TYPE_GAME)) && m_output->state->state().adaptiveSync;

    // keep requested minimum refresh rate
    if (shouldSkip && *PMINRR && m_lastPresentationTimer.getMillis() > 1000.0f / *PMINRR) {
        // damage whole screen because some previous cursor box damages were skipped
        m_damage.damageEntire();
        return false;
    }

    return shouldSkip;
}

bool CMonitor::isMirror() {
    return m_mirrorOf != nullptr;
}

bool CMonitor::matchesStaticSelector(const std::string& selector) const {
    if (selector.starts_with("desc:")) {
        // match by description
        const auto DESCRIPTIONSELECTOR = trim(selector.substr(5));

        return m_description.starts_with(DESCRIPTIONSELECTOR) || m_shortDescription.starts_with(DESCRIPTIONSELECTOR);
    } else {
        // match by selector
        return m_name == selector;
    }
}

WORKSPACEID CMonitor::findAvailableDefaultWS() {
    for (WORKSPACEID i = 1; i < LONG_MAX; ++i) {
        if (g_pCompositor->getWorkspaceByID(i))
            continue;

        if (const auto BOUND = g_pConfigManager->getBoundMonitorStringForWS(std::to_string(i)); !BOUND.empty() && BOUND != m_name)
            continue;

        return i;
    }

    return LONG_MAX; // shouldn't be reachable
}

void CMonitor::setupDefaultWS(const SMonitorRule& monitorRule) {
    // Workspace
    std::string newDefaultWorkspaceName = "";
    int64_t     wsID                    = WORKSPACE_INVALID;
    if (g_pConfigManager->getDefaultWorkspaceFor(m_name).empty())
        wsID = findAvailableDefaultWS();
    else {
        const auto ws           = getWorkspaceIDNameFromString(g_pConfigManager->getDefaultWorkspaceFor(m_name));
        wsID                    = ws.id;
        newDefaultWorkspaceName = ws.name;
    }

    if (wsID == WORKSPACE_INVALID || (wsID >= SPECIAL_WORKSPACE_START && wsID <= -2)) {
        wsID                    = g_pCompositor->m_workspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(wsID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"{}\" is invalid.", g_pConfigManager->getDefaultWorkspaceFor(m_name));
    }

    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(wsID);

    Debug::log(LOG, "New monitor: WORKSPACEID {}, exists: {}", wsID, (int)(PNEWWORKSPACE != nullptr));

    if (PNEWWORKSPACE) {
        // workspace exists, move it to the newly connected monitor
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, m_self.lock());
        m_activeWorkspace = PNEWWORKSPACE;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_id);
        PNEWWORKSPACE->startAnim(true, true, true);
    } else {
        if (newDefaultWorkspaceName.empty())
            newDefaultWorkspaceName = std::to_string(wsID);

        PNEWWORKSPACE = g_pCompositor->m_workspaces.emplace_back(CWorkspace::create(wsID, m_self.lock(), newDefaultWorkspaceName));
    }

    m_activeWorkspace = PNEWWORKSPACE;

    PNEWWORKSPACE->setActive(true);
    PNEWWORKSPACE->m_visible     = true;
    PNEWWORKSPACE->m_lastMonitor = "";
}

void CMonitor::setMirror(const std::string& mirrorOf) {
    const auto PMIRRORMON = g_pCompositor->getMonitorFromString(mirrorOf);

    if (PMIRRORMON == m_mirrorOf)
        return;

    if (PMIRRORMON && PMIRRORMON->isMirror()) {
        Debug::log(ERR, "Cannot mirror a mirror!");
        return;
    }

    if (PMIRRORMON == m_self) {
        Debug::log(ERR, "Cannot mirror self!");
        return;
    }

    if (!PMIRRORMON) {
        // disable mirroring

        if (m_mirrorOf) {
            m_mirrorOf->m_mirrors.erase(std::ranges::find_if(m_mirrorOf->m_mirrors, [&](const auto& other) { return other == m_self; }));

            // unlock software for mirrored monitor
            g_pPointerManager->unlockSoftwareForMonitor(m_mirrorOf.lock());
        }

        m_mirrorOf.reset();

        // set rule
        const auto RULE = g_pConfigManager->getMonitorRuleFor(m_self.lock());

        m_position = RULE.offset;

        // push to mvmonitors

        PHLMONITOR* thisWrapper = nullptr;

        // find the wrap
        for (auto& m : g_pCompositor->m_realMonitors) {
            if (m->m_id == m_id) {
                thisWrapper = &m;
                break;
            }
        }

        RASSERT(thisWrapper->get(), "CMonitor::setMirror: Had no wrapper???");

        if (std::ranges::find_if(g_pCompositor->m_monitors, [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_monitors.end()) {
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
            if (w->m_monitor == m_self || !w->m_monitor)
                wspToMove.push_back(w);
        }

        for (auto const& w : wspToMove) {
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }

        m_activeWorkspace.reset();

        m_position = PMIRRORMON->m_position;

        m_mirrorOf = PMIRRORMON;

        m_mirrorOf->m_mirrors.push_back(m_self);

        // remove from mvmonitors
        std::erase_if(g_pCompositor->m_monitors, [&](const auto& other) { return other == m_self; });

        g_pCompositor->arrangeMonitors();

        g_pCompositor->setActiveMonitor(g_pCompositor->m_monitors.front());

        g_pCompositor->sanityCheckWorkspaces();

        // Software lock mirrored monitor
        g_pPointerManager->lockSoftwareForMonitor(PMIRRORMON);
    }

    m_events.modeChanged.emit();
}

float CMonitor::getDefaultScale() {
    if (!m_enabled)
        return 1;

    static constexpr double MMPERINCH = 25.4;

    const auto              DIAGONALPX = sqrt(pow(m_pixelSize.x, 2) + pow(m_pixelSize.y, 2));
    const auto              DIAGONALIN = sqrt(pow(m_output->physicalSize.x / MMPERINCH, 2) + pow(m_output->physicalSize.y / MMPERINCH, 2));

    const auto              PPI = DIAGONALPX / DIAGONALIN;

    if (PPI > 200 /* High PPI, 2x*/)
        return 2;
    else if (PPI > 140 /* Medium PPI, 1.5x*/)
        return 1.5;
    return 1;
}

static bool shouldWraparound(const WORKSPACEID id1, const WORKSPACEID id2) {
    static auto PWORKSPACEWRAPAROUND = CConfigValue<Hyprlang::INT>("animations:workspace_wraparound");

    if (!*PWORKSPACEWRAPAROUND)
        return false;

    WORKSPACEID lowestID  = INT64_MAX;
    WORKSPACEID highestID = INT64_MIN;

    for (auto const& w : g_pCompositor->m_workspaces) {
        if (w->m_id < 0 || w->m_isSpecialWorkspace)
            continue;
        lowestID  = std::min(w->m_id, lowestID);
        highestID = std::max(w->m_id, highestID);
    }

    return std::min(id1, id2) == lowestID && std::max(id1, id2) == highestID;
}

void CMonitor::changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    if (!pWorkspace)
        return;

    if (pWorkspace->m_isSpecialWorkspace) {
        if (m_activeSpecialWorkspace != pWorkspace) {
            Debug::log(LOG, "changeworkspace on special, togglespecialworkspace to id {}", pWorkspace->m_id);
            setSpecialWorkspace(pWorkspace);
        }
        return;
    }

    if (pWorkspace == m_activeWorkspace)
        return;

    const auto POLDWORKSPACE = m_activeWorkspace;
    if (POLDWORKSPACE)
        POLDWORKSPACE->m_visible = false;
    pWorkspace->m_visible = true;

    m_activeWorkspace = pWorkspace;

    if (!internal) {
        const auto ANIMTOLEFT = POLDWORKSPACE && (shouldWraparound(pWorkspace->m_id, POLDWORKSPACE->m_id) ^ (pWorkspace->m_id > POLDWORKSPACE->m_id));
        if (POLDWORKSPACE)
            POLDWORKSPACE->startAnim(false, ANIMTOLEFT);
        pWorkspace->startAnim(true, ANIMTOLEFT);

        // move pinned windows
        for (auto const& w : g_pCompositor->m_windows) {
            if (w->m_workspace == POLDWORKSPACE && w->m_pinned)
                w->moveToWorkspace(pWorkspace);
        }

        if (!noFocus && !g_pCompositor->m_lastMonitor->m_activeSpecialWorkspace &&
            !(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == m_self)) {
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

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_id);

        g_pEventManager->postEvent(SHyprIPCEvent{"workspace", pWorkspace->m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{"workspacev2", std::format("{},{}", pWorkspace->m_id, pWorkspace->m_name)});
        EMIT_HOOK_EVENT("workspace", pWorkspace);
    }

    g_pHyprRenderer->damageMonitor(m_self.lock());

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(m_self.lock());

    g_pCompositor->updateSuspendedStates();

    if (m_activeSpecialWorkspace)
        g_pCompositor->updateFullscreenFadeOnWorkspace(m_activeSpecialWorkspace);
}

void CMonitor::changeWorkspace(const WORKSPACEID& id, bool internal, bool noMouseMove, bool noFocus) {
    changeWorkspace(g_pCompositor->getWorkspaceByID(id), internal, noMouseMove, noFocus);
}

void CMonitor::setSpecialWorkspace(const PHLWORKSPACE& pWorkspace) {
    if (m_activeSpecialWorkspace == pWorkspace)
        return;

    m_specialFade->setConfig(g_pConfigManager->getAnimationPropertyConfig(pWorkspace ? "specialWorkspaceIn" : "specialWorkspaceOut"));
    *m_specialFade = pWorkspace ? 1.F : 0.F;

    g_pHyprRenderer->damageMonitor(m_self.lock());

    if (!pWorkspace) {
        // remove special if exists
        if (m_activeSpecialWorkspace) {
            m_activeSpecialWorkspace->m_visible = false;
            m_activeSpecialWorkspace->startAnim(false, false);
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + m_name});
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", ",," + m_name});
        }
        m_activeSpecialWorkspace.reset();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_id);

        if (!(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == m_self)) {
            if (const auto PLAST = m_activeWorkspace->getLastFocusedWindow(); PLAST)
                g_pCompositor->focusWindow(PLAST);
            else
                g_pInputManager->refocus();
        }

        g_pCompositor->updateFullscreenFadeOnWorkspace(m_activeWorkspace);

        g_pConfigManager->ensureVRR(m_self.lock());

        g_pCompositor->updateSuspendedStates();

        return;
    }

    if (m_activeSpecialWorkspace) {
        m_activeSpecialWorkspace->m_visible = false;
        m_activeSpecialWorkspace->startAnim(false, false);
    }

    bool animate = true;
    //close if open elsewhere
    const auto PMONITORWORKSPACEOWNER = pWorkspace->m_monitor.lock();
    if (const auto PMWSOWNER = pWorkspace->m_monitor.lock(); PMWSOWNER && PMWSOWNER->m_activeSpecialWorkspace == pWorkspace) {
        PMWSOWNER->m_activeSpecialWorkspace.reset();
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMWSOWNER->m_id);
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + PMWSOWNER->m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", ",," + PMWSOWNER->m_name});

        const auto PACTIVEWORKSPACE = PMWSOWNER->m_activeWorkspace;
        g_pCompositor->updateFullscreenFadeOnWorkspace(PACTIVEWORKSPACE);

        animate = false;
    }

    // open special
    pWorkspace->m_monitor               = m_self;
    m_activeSpecialWorkspace            = pWorkspace;
    m_activeSpecialWorkspace->m_visible = true;
    if (animate)
        pWorkspace->startAnim(true, true);

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == pWorkspace) {
            w->m_monitor = m_self;
            w->updateSurfaceScaleTransformDetails();
            w->setAnimationsToMove();

            const auto MIDDLE = w->middle();
            if (w->m_isFloating && VECNOTINRECT(MIDDLE, m_position.x, m_position.y, m_position.x + m_size.x, m_position.y + m_size.y) && !w->isX11OverrideRedirect()) {
                // if it's floating and the middle isnt on the current mon, move it to the center
                const auto PMONFROMMIDDLE = g_pCompositor->getMonitorFromVector(MIDDLE);
                Vector2D   pos            = w->m_realPosition->goal();
                if (VECNOTINRECT(MIDDLE, PMONFROMMIDDLE->m_position.x, PMONFROMMIDDLE->m_position.y, PMONFROMMIDDLE->m_position.x + PMONFROMMIDDLE->m_size.x,
                                 PMONFROMMIDDLE->m_position.y + PMONFROMMIDDLE->m_size.y)) {
                    // not on any monitor, center
                    pos = middle() / 2.f - w->m_realSize->goal() / 2.f;
                } else
                    pos = pos - PMONFROMMIDDLE->m_position + m_position;

                *w->m_realPosition = pos;
                w->m_position      = pos;
            }
        }
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_id);

    if (!(g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_pinned && g_pCompositor->m_lastWindow->m_monitor == m_self)) {
        if (const auto PLAST = pWorkspace->getLastFocusedWindow(); PLAST)
            g_pCompositor->focusWindow(PLAST);
        else
            g_pInputManager->refocus();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", pWorkspace->m_name + "," + m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{"activespecialv2", std::to_string(pWorkspace->m_id) + "," + pWorkspace->m_name + "," + m_name});

    g_pHyprRenderer->damageMonitor(m_self.lock());

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(m_self.lock());

    g_pCompositor->updateSuspendedStates();
}

void CMonitor::setSpecialWorkspace(const WORKSPACEID& id) {
    setSpecialWorkspace(g_pCompositor->getWorkspaceByID(id));
}

void CMonitor::moveTo(const Vector2D& pos) {
    m_position = pos;
}

SWorkspaceIDName CMonitor::getPrevWorkspaceIDName(const WORKSPACEID id) {
    while (!m_prevWorkSpaces.empty()) {
        const int PREVID = m_prevWorkSpaces.top();
        m_prevWorkSpaces.pop();
        if (PREVID == id) // skip same workspace
            continue;

        // recheck if previous workspace's was moved to another monitor
        const auto ws = g_pCompositor->getWorkspaceByID(PREVID);
        if (ws && ws->monitorID() == m_id)
            return {.id = PREVID, .name = ws->m_name};
    }

    return {.id = WORKSPACE_INVALID};
}

void CMonitor::addPrevWorkspaceID(const WORKSPACEID id) {
    if (!m_prevWorkSpaces.empty() && m_prevWorkSpaces.top() == id)
        return;

    m_prevWorkSpaces.emplace(id);
}

Vector2D CMonitor::middle() {
    return m_position + m_size / 2.f;
}

void CMonitor::updateMatrix() {
    m_projMatrix = Mat3x3::identity();
    if (m_transform != WL_OUTPUT_TRANSFORM_NORMAL)
        m_projMatrix.translate(m_pixelSize / 2.0).transform(wlTransformToHyprutils(m_transform)).translate(-m_transformedSize / 2.0);
}

WORKSPACEID CMonitor::activeWorkspaceID() {
    return m_activeWorkspace ? m_activeWorkspace->m_id : 0;
}

WORKSPACEID CMonitor::activeSpecialWorkspaceID() {
    return m_activeSpecialWorkspace ? m_activeSpecialWorkspace->m_id : 0;
}

CBox CMonitor::logicalBox() {
    return {m_position, m_size};
}

void CMonitor::scheduleDone() {
    if (m_doneScheduled)
        return;

    m_doneScheduled = true;

    g_pEventLoopManager->doLater([M = m_self] {
        if (!M) // if M is gone, we got destroyed, doesn't matter.
            return;

        if (!PROTO::outputs.contains(M->m_name))
            return;

        PROTO::outputs.at(M->m_name)->sendDone();
        M->m_doneScheduled = false;
    });
}

void CMonitor::setCTM(const Mat3x3& ctm_) {
    m_ctm        = ctm_;
    m_ctmUpdated = true;
    g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::scheduleFrameReason::AQ_SCHEDULE_NEEDS_FRAME);
}

bool CMonitor::attemptDirectScanout() {
    if (!m_mirrors.empty() || isMirror() || g_pHyprRenderer->m_directScanoutBlocked)
        return false; // do not DS if this monitor is being mirrored. Will break the functionality.

    if (g_pPointerManager->softwareLockedFor(m_self.lock()))
        return false;

    const auto PCANDIDATE = m_solitaryClient.lock();

    if (!PCANDIDATE)
        return false;

    const auto PSURFACE = g_pXWaylandManager->getWindowSurface(PCANDIDATE);

    if (!PSURFACE || !PSURFACE->m_current.texture || !PSURFACE->m_current.buffer)
        return false;

    if (PSURFACE->m_current.bufferSize != m_pixelSize || PSURFACE->m_current.transform != m_transform)
        return false;

    // we can't scanout shm buffers.
    const auto params = PSURFACE->m_current.buffer->dmabuf();
    if (!params.success || !PSURFACE->m_current.texture->m_eglImage /* dmabuf */)
        return false;

    Debug::log(TRACE, "attemptDirectScanout: surface {:x} passed, will attempt, buffer {}", (uintptr_t)PSURFACE.get(), (uintptr_t)PSURFACE->m_current.buffer.m_buffer.get());

    auto PBUFFER = PSURFACE->m_current.buffer.m_buffer;

    if (PBUFFER == m_output->state->state().buffer) {
        PSURFACE->presentFeedback(Time::steadyNow(), m_self.lock());

        if (m_scanoutNeedsCursorUpdate) {
            if (!m_state.test()) {
                Debug::log(TRACE, "attemptDirectScanout: failed basic test");
                return false;
            }

            if (!m_output->commit()) {
                Debug::log(TRACE, "attemptDirectScanout: failed to commit cursor update");
                m_lastScanout.reset();
                return false;
            }

            m_scanoutNeedsCursorUpdate = false;
        }

        return true;
    }

    // FIXME: make sure the buffer actually follows the available scanout dmabuf formats
    // and comes from the appropriate device. This may implode on multi-gpu!!

    // entering into scanout, so save monitor format
    if (m_lastScanout.expired())
        m_prevDrmFormat = m_drmFormat;

    if (m_drmFormat != params.format) {
        m_output->state->setFormat(params.format);
        m_drmFormat = params.format;
    }

    m_output->state->setBuffer(PBUFFER);
    m_output->state->setPresentationMode(m_tearingState.activelyTearing ? Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_IMMEDIATE :
                                                                          Aquamarine::eOutputPresentationMode::AQ_OUTPUT_PRESENTATION_VSYNC);

    if (!m_state.test()) {
        Debug::log(TRACE, "attemptDirectScanout: failed basic test");
        return false;
    }

    PSURFACE->presentFeedback(Time::steadyNow(), m_self.lock());

    m_output->state->addDamage(PSURFACE->m_current.accumulateBufferDamage());
    m_output->state->resetExplicitFences();

    // no need to do explicit sync here as surface current can only ever be ready to read

    bool ok = m_output->commit();

    if (!ok) {
        Debug::log(TRACE, "attemptDirectScanout: failed to scanout surface");
        m_lastScanout.reset();
        return false;
    }

    if (m_lastScanout.expired()) {
        m_lastScanout = PCANDIDATE;
        Debug::log(LOG, "Entered a direct scanout to {:x}: \"{}\"", (uintptr_t)PCANDIDATE.get(), PCANDIDATE->m_title);
    }

    m_scanoutNeedsCursorUpdate = false;

    if (!PBUFFER->lockedByBackend || PBUFFER->m_hlEvents.backendRelease)
        return true;

    // lock buffer while DRM/KMS is using it, then release it when page flip happens since DRM/KMS should be done by then
    // btw buffer's syncReleaser will take care of signaling release point, so we don't do that here
    PBUFFER->lock();
    PBUFFER->onBackendRelease([PBUFFER]() { PBUFFER->unlock(); });

    return true;
}

void CMonitor::debugLastPresentation(const std::string& message) {
    Debug::log(TRACE, "{} (last presentation {} - {} fps)", message, m_lastPresentationTimer.getMillis(),
               m_lastPresentationTimer.getMillis() > 0 ? 1000.0f / m_lastPresentationTimer.getMillis() : 0.0f);
}

void CMonitor::onMonitorFrame() {
    if ((g_pCompositor->m_aqBackend->hasSession() && !g_pCompositor->m_aqBackend->session->active) || !g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");

        if (g_pCompositor->m_unsafeState && std::ranges::any_of(g_pCompositor->m_monitors.begin(), g_pCompositor->m_monitors.end(), [&](auto& m) {
                return m->m_output != g_pCompositor->m_unsafeOutput->m_output;
            })) {
            // restore from unsafe state
            g_pCompositor->leaveUnsafeState();
        }

        return; // cannot draw on session inactive (different tty)
    }

    if (!m_enabled)
        return;

    g_pHyprRenderer->recheckSolitaryForMonitor(m_self.lock());

    m_tearingState.busy = false;

    if (m_tearingState.activelyTearing && m_solitaryClient.lock() /* can be invalidated by a recheck */) {

        if (!m_tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        m_tearingState.nextRenderTorn          = true;
        m_tearingState.frameScheduledWhileBusy = false;
    }

    static auto PENABLERAT = CConfigValue<Hyprlang::INT>("misc:render_ahead_of_time");
    static auto PRATSAFE   = CConfigValue<Hyprlang::INT>("misc:render_ahead_safezone");

    m_lastPresentationTimer.reset();

    if (*PENABLERAT && !m_tearingState.nextRenderTorn) {
        if (!m_ratsScheduled) {
            // render
            g_pHyprRenderer->renderMonitor(m_self.lock());
        }

        m_ratsScheduled = false;

        const auto& [avg, max, min] = g_pHyprRenderer->getRenderTimes(m_self.lock());

        if (max + *PRATSAFE > 1000.0 / m_refreshRate)
            return;

        const auto MSLEFT = (1000.0 / m_refreshRate) - m_lastPresentationTimer.getMillis();

        m_ratsScheduled = true;

        const auto ESTRENDERTIME = std::ceil(avg + *PRATSAFE);
        const auto TIMETOSLEEP   = std::floor(MSLEFT - ESTRENDERTIME);

        if (MSLEFT < 1 || MSLEFT < ESTRENDERTIME || TIMETOSLEEP < 1)
            g_pHyprRenderer->renderMonitor(m_self.lock());
        else
            wl_event_source_timer_update(m_renderTimer, TIMETOSLEEP);
    } else
        g_pHyprRenderer->renderMonitor(m_self.lock());
}

void CMonitor::onCursorMovedOnMonitor() {
    if (!m_tearingState.activelyTearing || !m_solitaryClient || !g_pHyprRenderer->shouldRenderCursor())
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

    m_tearingState.frameScheduledWhileBusy = true;
}

CMonitorState::CMonitorState(CMonitor* owner) : m_owner(owner) {
    ;
}

void CMonitorState::ensureBufferPresent() {
    const auto STATE = m_owner->m_output->state->state();
    if (!STATE.enabled) {
        Debug::log(TRACE, "CMonitorState::ensureBufferPresent: Ignoring, monitor is not enabled");
        return;
    }

    if (STATE.buffer) {
        if (const auto params = STATE.buffer->dmabuf(); params.success && params.format == m_owner->m_drmFormat)
            return;
    }

    // this is required for modesetting being possible and might be missing in case of first tests in the renderer
    // where we test modes and buffers
    Debug::log(LOG, "CMonitorState::ensureBufferPresent: no buffer or mismatched format, attaching one from the swapchain for modeset being possible");
    m_owner->m_output->state->setBuffer(m_owner->m_output->swapchain->next(nullptr));
    m_owner->m_output->swapchain->rollback(); // restore the counter, don't advance the swapchain
}

bool CMonitorState::commit() {
    if (!updateSwapchain())
        return false;

    EMIT_HOOK_EVENT("preMonitorCommit", m_owner->m_self.lock());

    ensureBufferPresent();

    bool ret = m_owner->m_output->commit();
    return ret;
}

bool CMonitorState::test() {
    if (!updateSwapchain())
        return false;

    ensureBufferPresent();

    return m_owner->m_output->test();
}

bool CMonitorState::updateSwapchain() {
    auto        options = m_owner->m_output->swapchain->currentOptions();
    const auto& STATE   = m_owner->m_output->state->state();
    const auto& MODE    = STATE.mode ? STATE.mode : STATE.customMode;
    if (!MODE) {
        Debug::log(WARN, "updateSwapchain: No mode?");
        return true;
    }
    options.format  = m_owner->m_drmFormat;
    options.scanout = true;
    options.length  = 2;
    options.size    = MODE->pixelSize;
    return m_owner->m_output->swapchain->reconfigure(options);
}
