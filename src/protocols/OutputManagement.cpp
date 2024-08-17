#include "OutputManagement.hpp"
#include <algorithm>
#include "../Compositor.hpp"

using namespace Aquamarine;

COutputManager::COutputManager(SP<CZwlrOutputManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    LOGM(LOG, "New OutputManager registered");

    resource->setOnDestroy([this](CZwlrOutputManagerV1* r) { PROTO::outputManagement->destroyResource(this); });

    resource->setStop([this](CZwlrOutputManagerV1* r) { stopped = true; });

    resource->setCreateConfiguration([this](CZwlrOutputManagerV1* r, uint32_t id, uint32_t serial) {
        LOGM(LOG, "Creating new configuration");

        const auto RESOURCE = PROTO::outputManagement->m_vConfigurations.emplace_back(
            makeShared<COutputConfiguration>(makeShared<CZwlrOutputConfigurationV1>(resource->client(), resource->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::outputManagement->m_vConfigurations.pop_back();
            return;
        }
    });

    // send all heads at start
    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (m.get() == g_pCompositor->m_pUnsafeOutput)
            continue;

        LOGM(LOG, " | sending output head for {}", m->szName);

        makeAndSendNewHead(m.get());
    }

    sendDone();
}

bool COutputManager::good() {
    return resource->resource();
}

void COutputManager::makeAndSendNewHead(CMonitor* pMonitor) {
    if (stopped)
        return;

    const auto RESOURCE =
        PROTO::outputManagement->m_vHeads.emplace_back(makeShared<COutputHead>(makeShared<CZwlrOutputHeadV1>(resource->client(), resource->version(), 0), pMonitor));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::outputManagement->m_vHeads.pop_back();
        return;
    }

    heads.push_back(RESOURCE);

    resource->sendHead(RESOURCE->resource.get());
    RESOURCE->sendAllData();
}

void COutputManager::ensureMonitorSent(CMonitor* pMonitor) {
    if (pMonitor == g_pCompositor->m_pUnsafeOutput)
        return;

    for (auto& hw : heads) {
        auto h = hw.lock();

        if (!h)
            continue;

        if (h->pMonitor == pMonitor)
            return;
    }

    makeAndSendNewHead(pMonitor);

    sendDone();
}

void COutputManager::sendDone() {
    resource->sendDone(wl_display_next_serial(g_pCompositor->m_sWLDisplay));
}

COutputHead::COutputHead(SP<CZwlrOutputHeadV1> resource_, CMonitor* pMonitor_) : resource(resource_), pMonitor(pMonitor_) {
    if (!good())
        return;

    resource->setRelease([this](CZwlrOutputHeadV1* r) { PROTO::outputManagement->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrOutputHeadV1* r) { PROTO::outputManagement->destroyResource(this); });

    listeners.monitorDestroy = pMonitor->events.destroy.registerListener([this](std::any d) {
        resource->sendFinished();

        for (auto& mw : modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            m->resource->sendFinished();
        }

        pMonitor = nullptr;
        for (auto& m : PROTO::outputManagement->m_vManagers) {
            m->sendDone();
        }
    });

    listeners.monitorModeChange = pMonitor->events.modeChanged.registerListener([this](std::any d) { updateMode(); });
}

bool COutputHead::good() {
    return resource->resource();
}

void COutputHead::sendAllData() {
    const auto VERSION = resource->version();

    resource->sendName(pMonitor->szName.c_str());
    resource->sendDescription(pMonitor->szDescription.c_str());
    if (pMonitor->output->physicalSize.x > 0 && pMonitor->output->physicalSize.y > 0)
        resource->sendPhysicalSize(pMonitor->output->physicalSize.x, pMonitor->output->physicalSize.y);
    resource->sendEnabled(pMonitor->m_bEnabled);

    if (pMonitor->m_bEnabled) {
        resource->sendPosition(pMonitor->vecPosition.x, pMonitor->vecPosition.y);
        resource->sendTransform(pMonitor->transform);
        resource->sendScale(wl_fixed_from_double(pMonitor->scale));
    }

    if (!pMonitor->output->make.empty() && VERSION >= 2)
        resource->sendMake(pMonitor->output->make.c_str());
    if (!pMonitor->output->model.empty() && VERSION >= 2)
        resource->sendModel(pMonitor->output->model.c_str());
    if (!pMonitor->output->serial.empty() && VERSION >= 2)
        resource->sendSerialNumber(pMonitor->output->serial.c_str());

    if (VERSION >= 4)
        resource->sendAdaptiveSync(pMonitor->vrrActive ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);

    // send all available modes

    if (modes.empty()) {
        if (!pMonitor->output->modes.empty()) {
            for (auto& m : pMonitor->output->modes) {
                makeAndSendNewMode(m);
            }
        } else if (pMonitor->output->state->state().customMode) {
            makeAndSendNewMode(pMonitor->output->state->state().customMode);
        } else
            makeAndSendNewMode(nullptr);
    }

    // send current mode
    if (pMonitor->m_bEnabled) {
        for (auto& mw : modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            if (m->mode == pMonitor->output->state->state().mode) {
                if (m->mode)
                    LOGM(LOG, "  | sending current mode for {}: {}x{}@{}", pMonitor->szName, m->mode->pixelSize.x, m->mode->pixelSize.y, m->mode->refreshRate);
                else
                    LOGM(LOG, "  | sending current mode for {}: null (fake)", pMonitor->szName);
                resource->sendCurrentMode(m->resource.get());
                break;
            }
        }
    }
}

void COutputHead::updateMode() {
    resource->sendEnabled(pMonitor->m_bEnabled);

    if (pMonitor->m_bEnabled) {
        resource->sendPosition(pMonitor->vecPosition.x, pMonitor->vecPosition.y);
        resource->sendTransform(pMonitor->transform);
        resource->sendScale(wl_fixed_from_double(pMonitor->scale));
    }

    if (resource->version() >= 4)
        resource->sendAdaptiveSync(pMonitor->vrrActive ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);

    if (pMonitor->m_bEnabled) {
        for (auto& mw : modes) {
            auto m = mw.lock();

            if (!m)
                continue;

            if (m->mode == pMonitor->currentMode) {
                if (m->mode)
                    LOGM(LOG, "  | sending current mode for {}: {}x{}@{}", pMonitor->szName, m->mode->pixelSize.x, m->mode->pixelSize.y, m->mode->refreshRate);
                else
                    LOGM(LOG, "  | sending current mode for {}: null (fake)", pMonitor->szName);
                resource->sendCurrentMode(m->resource.get());
                break;
            }
        }
    }
}

void COutputHead::makeAndSendNewMode(SP<Aquamarine::SOutputMode> mode) {
    const auto RESOURCE = PROTO::outputManagement->m_vModes.emplace_back(makeShared<COutputMode>(makeShared<CZwlrOutputModeV1>(resource->client(), resource->version(), 0), mode));

    if (!RESOURCE->good()) {
        resource->noMemory();
        PROTO::outputManagement->m_vModes.pop_back();
        return;
    }

    modes.push_back(RESOURCE);
    resource->sendMode(RESOURCE->resource.get());
    RESOURCE->sendAllData();
}

CMonitor* COutputHead::monitor() {
    return pMonitor;
}

COutputMode::COutputMode(SP<CZwlrOutputModeV1> resource_, SP<Aquamarine::SOutputMode> mode_) : resource(resource_), mode(mode_) {
    if (!good())
        return;

    resource->setRelease([this](CZwlrOutputModeV1* r) { PROTO::outputManagement->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrOutputModeV1* r) { PROTO::outputManagement->destroyResource(this); });
}

void COutputMode::sendAllData() {
    if (!mode)
        return;

    LOGM(LOG, "  | sending mode {}x{}@{}mHz, pref: {}", mode->pixelSize.x, mode->pixelSize.y, mode->refreshRate, mode->preferred);

    resource->sendSize(mode->pixelSize.x, mode->pixelSize.y);
    if (mode->refreshRate > 0)
        resource->sendRefresh(mode->refreshRate);
    if (mode->preferred)
        resource->sendPreferred();
}

bool COutputMode::good() {
    return resource->resource();
}

SP<Aquamarine::SOutputMode> COutputMode::getMode() {
    return mode.lock();
}

COutputConfiguration::COutputConfiguration(SP<CZwlrOutputConfigurationV1> resource_, SP<COutputManager> owner_) : resource(resource_), owner(owner_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwlrOutputConfigurationV1* r) { PROTO::outputManagement->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrOutputConfigurationV1* r) { PROTO::outputManagement->destroyResource(this); });

    resource->setEnableHead([this](CZwlrOutputConfigurationV1* r, uint32_t id, wl_resource* outputHead) {
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

        const auto RESOURCE = PROTO::outputManagement->m_vConfigurationHeads.emplace_back(
            makeShared<COutputConfigurationHead>(makeShared<CZwlrOutputConfigurationHeadV1>(resource->client(), resource->version(), id), PMONITOR));

        if (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::outputManagement->m_vConfigurationHeads.pop_back();
            return;
        }

        heads.push_back(RESOURCE);

        LOGM(LOG, "enableHead on {}. For now, doing nothing. Waiting for apply().", PMONITOR->szName);
    });

    resource->setDisableHead([this](CZwlrOutputConfigurationV1* r, wl_resource* outputHead) {
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

        LOGM(LOG, "disableHead on {}", PMONITOR->szName);

        PMONITOR->activeMonitorRule.disabled = true;
        if (!g_pConfigManager->replaceMonitorRule(PMONITOR->activeMonitorRule))
            g_pConfigManager->appendMonitorRule(PMONITOR->activeMonitorRule);
        g_pHyprRenderer->applyMonitorRule(PMONITOR, &PMONITOR->activeMonitorRule, false);
    });

    resource->setTest([this](CZwlrOutputConfigurationV1* r) {
        const auto SUCCESS = applyTestConfiguration(true);

        if (SUCCESS)
            resource->sendSucceeded();
        else
            resource->sendFailed();
    });

    resource->setApply([this](CZwlrOutputConfigurationV1* r) {
        const auto SUCCESS = applyTestConfiguration(false);

        if (SUCCESS)
            resource->sendSucceeded();
        else
            resource->sendFailed();

        owner->sendDone();
    });
}

bool COutputConfiguration::good() {
    return resource->resource();
}

bool COutputConfiguration::applyTestConfiguration(bool test) {
    if (test) {
        LOGM(WARN, "TODO: STUB: applyTestConfiguration for test not implemented, returning true.");
        return true;
    }

    LOGM(LOG, "Applying configuration");

    for (auto& headw : heads) {
        auto head = headw.lock();

        if (!head)
            continue;

        const auto PMONITOR = head->pMonitor;

        if (!PMONITOR)
            continue;

        LOGM(LOG, "Applying config for monitor {}", PMONITOR->szName);

        SMonitorRule newRule = PMONITOR->activeMonitorRule;
        newRule.name         = PMONITOR->szName;
        newRule.disabled     = false;

        if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_MODE) {
            newRule.resolution  = head->state.mode->getMode()->pixelSize;
            newRule.refreshRate = head->state.mode->getMode()->refreshRate / 1000.F;
        } else if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_CUSTOM_MODE) {
            newRule.resolution  = head->state.customMode.size;
            newRule.refreshRate = head->state.customMode.refresh / 1000.F;
        }

        if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_POSITION)
            newRule.offset = head->state.position;

        if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC)
            newRule.vrr = head->state.adaptiveSync;

        if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_SCALE)
            newRule.scale = head->state.scale;

        if (head->committedProperties & COutputConfigurationHead::eCommittedProperties::OUTPUT_HEAD_COMMITTED_TRANSFORM)
            newRule.transform = head->state.transform;

        // reset properties for next set.
        head->committedProperties = 0;

        if (!g_pConfigManager->replaceMonitorRule(newRule))
            g_pConfigManager->appendMonitorRule(newRule);
        g_pConfigManager->m_bWantsMonitorReload = true;
    }

    LOGM(LOG, "Applied configuration");

    return true;
}

COutputConfigurationHead::COutputConfigurationHead(SP<CZwlrOutputConfigurationHeadV1> resource_, CMonitor* pMonitor_) : resource(resource_), pMonitor(pMonitor_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwlrOutputConfigurationHeadV1* r) { PROTO::outputManagement->destroyResource(this); });

    listeners.monitorDestroy = pMonitor->events.destroy.registerListener([this](std::any d) { pMonitor = nullptr; });

    resource->setSetMode([this](CZwlrOutputConfigurationHeadV1* r, wl_resource* outputMode) {
        const auto MODE = PROTO::outputManagement->modeFromResource(outputMode);

        if (!MODE || !MODE->getMode()) {
            LOGM(ERR, "No mode in setMode??");
            return;
        }

        if (!pMonitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_MODE) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_MODE;
        state.mode = MODE;

        LOGM(LOG, " | configHead for {}: set mode to {}x{}@{}", pMonitor->szName, MODE->getMode()->pixelSize.x, MODE->getMode()->pixelSize.y, MODE->getMode()->refreshRate);
    });

    resource->setSetCustomMode([this](CZwlrOutputConfigurationHeadV1* r, int32_t w, int32_t h, int32_t refresh) {
        if (!pMonitor) {
            LOGM(ERR, "setCustomMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_CUSTOM_MODE) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (w <= 0 || h <= 0 || refresh <= 100) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_CUSTOM_MODE, "Invalid mode");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_CUSTOM_MODE;
        state.customMode = {{w, h}, (uint32_t)refresh};

        LOGM(LOG, " | configHead for {}: set custom mode to {}x{}@{}", pMonitor->szName, w, h, refresh);
    });

    resource->setSetPosition([this](CZwlrOutputConfigurationHeadV1* r, int32_t x, int32_t y) {
        if (!pMonitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_POSITION) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_POSITION;
        state.position = {x, y};

        LOGM(LOG, " | configHead for {}: set pos to {}, {}", pMonitor->szName, x, y);
    });

    resource->setSetTransform([this](CZwlrOutputConfigurationHeadV1* r, int32_t transform) {
        if (!pMonitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_TRANSFORM) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (transform < 0 || transform > 7) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_TRANSFORM, "Invalid transform");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_TRANSFORM;
        state.transform = (wl_output_transform)transform;

        LOGM(LOG, " | configHead for {}: set transform to {}", pMonitor->szName, transform);
    });

    resource->setSetScale([this](CZwlrOutputConfigurationHeadV1* r, wl_fixed_t scale_) {
        if (!pMonitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        double scale = wl_fixed_to_double(scale_);

        if (scale < 0.1 || scale > 10.0) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_SCALE, "Invalid scale");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_SCALE;
        state.scale = scale;

        LOGM(LOG, " | configHead for {}: set scale to {:.2f}", pMonitor->szName, scale);
    });

    resource->setSetAdaptiveSync([this](CZwlrOutputConfigurationHeadV1* r, uint32_t as) {
        if (!pMonitor) {
            LOGM(ERR, "setMode on inert resource");
            return;
        }

        if (committedProperties & OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_ALREADY_SET, "Property already set");
            return;
        }

        if (as > 1) {
            resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_ADAPTIVE_SYNC_STATE, "Invalid adaptive sync state");
            return;
        }

        committedProperties |= OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC;
        state.adaptiveSync = as;

        LOGM(LOG, " | configHead for {}: set adaptiveSync to {}", pMonitor->szName, as);
    });
}

bool COutputConfigurationHead::good() {
    return resource->resource();
}

COutputManagementProtocol::COutputManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
}

void COutputManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<COutputManager>(makeShared<CZwlrOutputManagerV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;
}

void COutputManagementProtocol::destroyResource(COutputManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputHead* resource) {
    std::erase_if(m_vHeads, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputMode* resource) {
    std::erase_if(m_vModes, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputConfiguration* resource) {
    std::erase_if(m_vConfigurations, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::destroyResource(COutputConfigurationHead* resource) {
    std::erase_if(m_vConfigurationHeads, [&](const auto& other) { return other.get() == resource; });
}

void COutputManagementProtocol::updateAllOutputs() {
    for (auto& m : g_pCompositor->m_vRealMonitors) {
        for (auto& mgr : m_vManagers) {
            mgr->ensureMonitorSent(m.get());
        }
    }
}

SP<COutputHead> COutputManagementProtocol::headFromResource(wl_resource* r) {
    for (auto& h : m_vHeads) {
        if (h->resource->resource() == r)
            return h;
    }

    return nullptr;
}

SP<COutputMode> COutputManagementProtocol::modeFromResource(wl_resource* r) {
    for (auto& h : m_vModes) {
        if (h->resource->resource() == r)
            return h;
    }

    return nullptr;
}
