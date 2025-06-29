#include "OutputManagement.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../config/ConfigManager.hpp"

using namespace Aquamarine;

COutputManager::COutputManager(SP<CZwlrOutputManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    LOGM(LOG, "New OutputManager registered");

    m_resource->setOnDestroy([this](CZwlrOutputManagerV1* r) { PROTO::outputManagement->destroyResource(this); });

    m_resource->setStop([this](CZwlrOutputManagerV1* r) { m_stopped = true; });

    m_resource->setCreateConfiguration([this](CZwlrOutputManagerV1* r, uint32_t id, uint32_t serial) {
        LOGM(LOG, "Creating new configuration");

        const auto RESOURCE = PROTO::outputManagement->m_configurations.emplace_back(
            makeShared<COutputConfiguration>(makeShared<CZwlrOutputConfigurationV1>(m_resource->client(), m_resource->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::outputManagement->m_configurations.pop_back();
            return;
        }
    });

    // send all heads at start
    for (auto const& m : g_pCompositor->m_realMonitors) {
        if (m == g_pCompositor->m_unsafeOutput)
            continue;

        LOGM(LOG, " | sending output head for {}", m->m_name);

        makeAndSendNewHead(m);
    }

    sendDone();
}

bool COutputManager::good() {
    return m_resource->resource();
}

void COutputManager::makeAndSendNewHead(PHLMONITOR pMonitor) {
    if UNLIKELY (m_stopped)
        return;

    const auto RESOURCE =
        PROTO::outputManagement->m_heads.emplace_back(makeShared<COutputHead>(makeShared<CZwlrOutputHeadV1>(m_resource->client(), m_resource->version(), 0), pMonitor));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::outputManagement->m_heads.pop_back();
        return;
    }

    m_heads.emplace_back(RESOURCE);

    m_resource->sendHead(RESOURCE->m_resource.get());
    RESOURCE->sendAllData();
}

void COutputManager::ensureMonitorSent(PHLMONITOR pMonitor) {
    if (pMonitor == g_pCompositor->m_unsafeOutput)
        return;

    for (auto const& hw : m_heads) {
        auto h = hw.lock();

        if (!h)
            continue;

        if (h->m_monitor == pMonitor)
            return;
    }

    makeAndSendNewHead(pMonitor);

    sendDone();
}

void COutputManager::sendDone() {
    m_resource->sendDone(wl_display_next_serial(g_pCompositor->m_wlDisplay));
}

COutputHead::COutputHead(SP<CZwlrOutputHeadV1> resource_, PHLMONITOR pMonitor_) : m_resource(resource_), m_monitor(pMonitor_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](CZwlrOutputHeadV1* r) { PROTO::outputManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrOutputHeadV1* r) { PROTO::outputManagement->destroyResource(this); });

    m_listeners.monitorDestroy = m_monitor->m_events.destroy.listen([this] {
        m_resource->sendFinished();

        for (auto const& mw : m_modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            m->m_resource->sendFinished();
        }

        m_monitor.reset();
        for (auto const& m : PROTO::outputManagement->m_managers) {
            m->sendDone();
        }
    });

    m_listeners.monitorModeChange = m_monitor->m_events.modeChanged.listen([this] { updateMode(); });
}

bool COutputHead::good() {
    return m_resource->resource();
}

void COutputHead::sendAllData() {
    const auto VERSION = m_resource->version();

    m_resource->sendName(m_monitor->m_name.c_str());
    m_resource->sendDescription(m_monitor->m_description.c_str());
    if (m_monitor->m_output->physicalSize.x > 0 && m_monitor->m_output->physicalSize.y > 0)
        m_resource->sendPhysicalSize(m_monitor->m_output->physicalSize.x, m_monitor->m_output->physicalSize.y);
    m_resource->sendEnabled(m_monitor->m_enabled);

    if (m_monitor->m_enabled) {
        m_resource->sendPosition(m_monitor->m_position.x, m_monitor->m_position.y);
        m_resource->sendTransform(m_monitor->m_transform);
        m_resource->sendScale(wl_fixed_from_double(m_monitor->m_scale));
    }

    if (!m_monitor->m_output->make.empty() && VERSION >= 2)
        m_resource->sendMake(m_monitor->m_output->make.c_str());
    if (!m_monitor->m_output->model.empty() && VERSION >= 2)
        m_resource->sendModel(m_monitor->m_output->model.c_str());
    if (!m_monitor->m_output->serial.empty() && VERSION >= 2)
        m_resource->sendSerialNumber(m_monitor->m_output->serial.c_str());

    if (VERSION >= 4)
        m_resource->sendAdaptiveSync(m_monitor->m_vrrActive ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);

    // send all available modes

    if (m_modes.empty()) {
        if (!m_monitor->m_output->modes.empty()) {
            for (auto const& m : m_monitor->m_output->modes) {
                makeAndSendNewMode(m);
            }
        } else if (m_monitor->m_output->state->state().customMode) {
            makeAndSendNewMode(m_monitor->m_output->state->state().customMode);
        } else
            makeAndSendNewMode(nullptr);
    }

    // send current mode
    if (m_monitor->m_enabled) {
        for (auto const& mw : m_modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            if (m->m_mode == m_monitor->m_output->state->state().mode) {
                if (m->m_mode)
                    LOGM(LOG, "  | sending current mode for {}: {}x{}@{}", m_monitor->m_name, m->m_mode->pixelSize.x, m->m_mode->pixelSize.y, m->m_mode->refreshRate);
                else
                    LOGM(LOG, "  | sending current mode for {}: null (fake)", m_monitor->m_name);
                m_resource->sendCurrentMode(m->m_resource.get());
                break;
            }
        }
    }
}

void COutputHead::updateMode() {
    m_resource->sendEnabled(m_monitor->m_enabled);

    if (m_monitor->m_enabled) {
        m_resource->sendPosition(m_monitor->m_position.x, m_monitor->m_position.y);
        m_resource->sendTransform(m_monitor->m_transform);
        m_resource->sendScale(wl_fixed_from_double(m_monitor->m_scale));
    }

    if (m_resource->version() >= 4)
        m_resource->sendAdaptiveSync(m_monitor->m_vrrActive ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);

    if (m_monitor->m_enabled) {
        for (auto const& mw : m_modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            if (m->m_mode == m_monitor->m_currentMode) {
                if (m->m_mode)
                    LOGM(LOG, "  | sending current mode for {}: {}x{}@{}", m_monitor->m_name, m->m_mode->pixelSize.x, m->m_mode->pixelSize.y, m->m_mode->refreshRate);
                else
                    LOGM(LOG, "  | sending current mode for {}: null (fake)", m_monitor->m_name);
                m_resource->sendCurrentMode(m->m_resource.get());
                break;
            }
        }
    }
}

void COutputHead::makeAndSendNewMode(SP<Aquamarine::SOutputMode> mode) {
    const auto RESOURCE =
        PROTO::outputManagement->m_modes.emplace_back(makeShared<COutputMode>(makeShared<CZwlrOutputModeV1>(m_resource->client(), m_resource->version(), 0), mode));

    if UNLIKELY (!RESOURCE->good()) {
        m_resource->noMemory();
        PROTO::outputManagement->m_modes.pop_back();
        return;
    }

    m_modes.emplace_back(RESOURCE);
    m_resource->sendMode(RESOURCE->m_resource.get());
    RESOURCE->sendAllData();
}

PHLMONITOR COutputHead::monitor() {
    return m_monitor.lock();
}

COutputMode::COutputMode(SP<CZwlrOutputModeV1> resource_, SP<Aquamarine::SOutputMode> mode_) : m_resource(resource_), m_mode(mode_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](CZwlrOutputModeV1* r) { PROTO::outputManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrOutputModeV1* r) { PROTO::outputManagement->destroyResource(this); });
}

void COutputMode::sendAllData() {
    if (!m_mode)
        return;

    LOGM(LOG, "  | sending mode {}x{}@{}mHz, pref: {}", m_mode->pixelSize.x, m_mode->pixelSize.y, m_mode->refreshRate, m_mode->preferred);

    m_resource->sendSize(m_mode->pixelSize.x, m_mode->pixelSize.y);
    if (m_mode->refreshRate > 0)
        m_resource->sendRefresh(m_mode->refreshRate);
    if (m_mode->preferred)
        m_resource->sendPreferred();
}

bool COutputMode::good() {
    return m_resource->resource();
}

SP<Aquamarine::SOutputMode> COutputMode::getMode() {
    return m_mode.lock();
}

COutputConfiguration::COutputConfiguration(SP<CZwlrOutputConfigurationV1> resource_, SP<COutputManager> owner_) : m_resource(resource_), m_owner(owner_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrOutputConfigurationV1* r) { PROTO::outputManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrOutputConfigurationV1* r) { PROTO::outputManagement->destroyResource(this); });

    m_resource->setEnableHead([this](CZwlrOutputConfigurationV1* r, uint32_t id, wl_resource* outputHead) {
        const auto HEAD = PROTO::outputManagement->headFromResource(outputHead);

        if (!HEAD) {
            LOGM(ERR, "No head in setEnableHead??");
            return;
        }

        const auto PMONITOR = HEAD->monitor();

        if (!PMONITOR) {
            LOGM(ERR, "No monitor in setEnableHead??");
            return;
        }

        const auto RESOURCE = PROTO::outputManagement->m_configurationHeads.emplace_back(
            makeShared<COutputConfigurationHead>(makeShared<CZwlrOutputConfigurationHeadV1>(m_resource->client(), m_resource->version(), id), PMONITOR));

        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::outputManagement->m_configurationHeads.pop_back();
            return;
        }

        m_heads.emplace_back(RESOURCE);

        LOGM(LOG, "enableHead on {}. For now, doing nothing. Waiting for apply().", PMONITOR->m_name);
    });

    m_resource->setDisableHead([this](CZwlrOutputConfigurationV1* r, wl_resource* outputHead) {
        const auto HEAD = PROTO::outputManagement->headFromResource(outputHead);

        if (!HEAD) {
            LOGM(ERR, "No head in setDisableHead??");
            return;
        }

        const auto PMONITOR = HEAD->monitor();

        if (!PMONITOR) {
            LOGM(ERR, "No monitor in setDisableHead??");
            return;
        }

        LOGM(LOG, "disableHead on {}", PMONITOR->m_name);

        SWlrManagerSavedOutputState newState;
        if (m_owner->m_monitorStates.contains(PMONITOR->m_name))
            newState = m_owner->m_monitorStates.at(PMONITOR->m_name);

        newState.enabled = false;

        g_pConfigManager->m_wantsMonitorReload = true;

        m_owner->m_monitorStates[PMONITOR->m_name] = newState;
    });

    m_resource->setTest([this](CZwlrOutputConfigurationV1* r) {
        const auto SUCCESS = applyTestConfiguration(true);

        if (SUCCESS)
            m_resource->sendSucceeded();
        else
            m_resource->sendFailed();
    });

    m_resource->setApply([this](CZwlrOutputConfigurationV1* r) {
        const auto SUCCESS = applyTestConfiguration(false);

        if (SUCCESS)
            m_resource->sendSucceeded();
        else
            m_resource->sendFailed();

        m_owner->sendDone();
    });
}

bool COutputConfiguration::good() {
    return m_resource->resource();
}

bool COutputConfiguration::applyTestConfiguration(bool test) {
    if (test) {
        LOGM(WARN, "TODO: STUB: applyTestConfiguration for test not implemented, returning true.");
        return true;
    }

    LOGM(LOG, "Applying configuration");

    if (!m_owner) {
        LOGM(ERR, "applyTestConfiguration: no owner?!");
        return false;
    }

    for (auto const& headw : m_heads) {
        auto head = headw.lock();

        if (!head)
            continue;

        const auto PMONITOR = head->m_monitor;

        if (!PMONITOR)
            continue;

        LOGM(LOG, "Saving config for monitor {}", PMONITOR->m_name);

        SWlrManagerSavedOutputState newState;
        if (m_owner->m_monitorStates.contains(PMONITOR->m_name))
            newState = m_owner->m_monitorStates.at(PMONITOR->m_name);

        newState.enabled = true;

        if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_MODE) {
            newState.resolution = head->m_state.mode->getMode()->pixelSize;
            newState.refresh    = head->m_state.mode->getMode()->refreshRate;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_MODE;
            LOGM(LOG, " > Mode: {:.0f}x{:.0f}@{}mHz", newState.resolution.x, newState.resolution.y, newState.refresh);
        } else if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_CUSTOM_MODE) {
            newState.resolution = head->m_state.customMode.size;
            newState.refresh    = head->m_state.customMode.refresh;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_CUSTOM_MODE;
            LOGM(LOG, " > Custom mode: {:.0f}x{:.0f}@{}mHz", newState.resolution.x, newState.resolution.y, newState.refresh);
        }

        if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_POSITION) {
            newState.position = head->m_state.position;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_POSITION;
            LOGM(LOG, " > Position: {:.0f}, {:.0f}", head->m_state.position.x, head->m_state.position.y);
        }

        if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC) {
            newState.adaptiveSync = head->m_state.adaptiveSync;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC;
            LOGM(LOG, " > vrr: {}", newState.adaptiveSync);
        }

        if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_SCALE) {
            newState.scale = head->m_state.scale;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_SCALE;
            LOGM(LOG, " > scale: {:.2f}", newState.scale);
        }

        if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_TRANSFORM) {
            newState.transform = head->m_state.transform;
            newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_TRANSFORM;
            LOGM(LOG, " > transform: {}", (uint8_t)newState.transform);
        }

        // reset properties for next set.
        head->m_state.committedProperties = 0;

        g_pConfigManager->m_wantsMonitorReload = true;

        m_owner->m_monitorStates[PMONITOR->m_name] = newState;
    }

    LOGM(LOG, "Saved configuration");

    return true;
}

COutputConfigurationHead::COutputConfigurationHead(SP<CZwlrOutputConfigurationHeadV1> resource_, PHLMONITOR pMonitor_) : m_resource(resource_), m_monitor(pMonitor_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwlrOutputConfigurationHeadV1* r) { PROTO::outputManagement->destroyResource(this); });

    m_resource->setSetMode([this](CZwlrOutputConfigurationHeadV1* r, wl_resource* outputMode) {
        const auto MODE = PROTO::outputManagement->modeFromResource(outputMode);

        if (!MODE || !MODE->getMode()) {
            LOGM(ERR, "No mode in setMode??");
            return;
        }

        if (!m_monitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_MODE) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_MODE;
        m_state.mode = MODE;

        LOGM(LOG, " | configHead for {}: set mode to {}x{}@{}", m_monitor->m_name, MODE->getMode()->pixelSize.x, MODE->getMode()->pixelSize.y, MODE->getMode()->refreshRate);
    });

    m_resource->setSetCustomMode([this](CZwlrOutputConfigurationHeadV1* r, int32_t w, int32_t h, int32_t refresh) {
        if (!m_monitor) {
            LOGM(ERR, "setCustomMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_CUSTOM_MODE) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (w <= 0 || h <= 0 || refresh < 0) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_CUSTOM_MODE, "Invalid mode");
            return;
        }

        if (refresh == 0) {
            LOGM(LOG, " | configHead for {}: refreshRate 0, using old refresh rate of {:.2f}Hz", m_monitor->m_name, m_monitor->m_refreshRate);
            refresh = std::round(m_monitor->m_refreshRate * 1000.F);
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_CUSTOM_MODE;
        m_state.customMode = {{w, h}, (uint32_t)refresh};

        LOGM(LOG, " | configHead for {}: set custom mode to {}x{}@{}", m_monitor->m_name, w, h, refresh);
    });

    m_resource->setSetPosition([this](CZwlrOutputConfigurationHeadV1* r, int32_t x, int32_t y) {
        if (!m_monitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_POSITION) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_POSITION;
        m_state.position = {x, y};

        LOGM(LOG, " | configHead for {}: set pos to {}, {}", m_monitor->m_name, x, y);
    });

    m_resource->setSetTransform([this](CZwlrOutputConfigurationHeadV1* r, int32_t transform) {
        if (!m_monitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_TRANSFORM) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (transform < 0 || transform > 7) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_TRANSFORM, "Invalid transform");
            return;
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_TRANSFORM;
        m_state.transform = (wl_output_transform)transform;

        LOGM(LOG, " | configHead for {}: set transform to {}", m_monitor->m_name, transform);
    });

    m_resource->setSetScale([this](CZwlrOutputConfigurationHeadV1* r, wl_fixed_t scale_) {
        if (!m_monitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        double scale = wl_fixed_to_double(scale_);

        if (scale < 0.1 || scale > 10.0) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_SCALE, "Invalid scale");
            return;
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_SCALE;
        m_state.scale = scale;

        LOGM(LOG, " | configHead for {}: set scale to {:.2f}", m_monitor->m_name, scale);
    });

    m_resource->setSetAdaptiveSync([this](CZwlrOutputConfigurationHeadV1* r, uint32_t as) {
        if (!m_monitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (m_state.committedProperties & OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (as > 1) {
            m_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_ADAPTIVE_SYNC_STATE, "Invalid adaptive sync state");
            return;
        }

        m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC;
        m_state.adaptiveSync = as;

        LOGM(LOG, " | configHead for {}: set adaptiveSync to {}", m_monitor->m_name, as);
    });
}

bool COutputConfigurationHead::good() {
    return m_resource->resource();
}

COutputManagementProtocol::COutputManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
}

void COutputManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<COutputManager>(makeShared<CZwlrOutputManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;
}

void COutputManagementProtocol::destroyResource(COutputManager* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputHead* resource) {
    std::erase_if(m_heads, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputMode* resource) {
    std::erase_if(m_modes, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputConfiguration* resource) {
    std::erase_if(m_configurations, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputConfigurationHead* resource) {
    std::erase_if(m_configurationHeads, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::updateAllOutputs() {
    for (auto const& m : g_pCompositor->m_realMonitors) {
        for (auto const& mgr : m_managers) {
            mgr->ensureMonitorSent(m);
        }
    }
}

SP<COutputHead> COutputManagementProtocol::headFromResource(wl_resource* r) {
    for (auto const& h : m_heads) {
        if (h->m_resource->resource() == r)
            return h;
    }

    return nullptr;
}

SP<COutputMode> COutputManagementProtocol::modeFromResource(wl_resource* r) {
    for (auto const& h : m_modes) {
        if (h->m_resource->resource() == r)
            return h;
    }

    return nullptr;
}

SP<SWlrManagerSavedOutputState> COutputManagementProtocol::getOutputStateFor(PHLMONITOR pMonitor) {
    for (auto const& m : m_managers) {
        if (!m->m_monitorStates.contains(pMonitor->m_name))
            continue;

        return makeShared<SWlrManagerSavedOutputState>(m->m_monitorStates.at(pMonitor->m_name));
    }

    return nullptr;
}
