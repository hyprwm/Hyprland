#include "GammaControl.hpp"
#include <fcntl.h>
#include <unistd.h>
#include "../helpers/Monitor.hpp"
#include "../Compositor.hpp"

#define LOGM PROTO::gamma->protoLog

CGammaControl::CGammaControl(SP<CZwlrGammaControlV1> resource_, wl_resource* output) : resource(resource_) {
    if (!resource_->resource())
        return;

    wlr_output* wlrOutput = wlr_output_from_resource(output);

    if (!wlrOutput) {
        LOGM(ERR, "No wlr_output in CGammaControl");
        resource->sendFailed();
        return;
    }

    pMonitor = g_pCompositor->getRealMonitorFromOutput(wlrOutput);

    if (!pMonitor) {
        LOGM(ERR, "No CMonitor");
        resource->sendFailed();
        return;
    }

    for (auto& g : PROTO::gamma->m_vGammaControllers) {
        if (g->pMonitor == pMonitor) {
            resource->sendFailed();
            return;
        }
    }

    gammaSize = wlr_output_get_gamma_size(wlrOutput);

    if (gammaSize <= 0) {
        LOGM(ERR, "Output {} doesn't support gamma", pMonitor->szName);
        resource->sendFailed();
        return;
    }

    gammaTable.resize(gammaSize * 3);

    resource->setDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });
    resource->setOnDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });

    resource->setSetGamma([this](CZwlrGammaControlV1* gamma, int32_t fd) {
        LOGM(LOG, "setGamma for {}", pMonitor->szName);

        int fdFlags = fcntl(fd, F_GETFL, 0);
        if (fdFlags < 0) {
            LOGM(ERR, "Failed to get fd flags");
            resource->sendFailed();
            close(fd);
            return;
        }

        if (fcntl(fd, F_SETFL, fdFlags | O_NONBLOCK) < 0) {
            LOGM(ERR, "Failed to set fd flags");
            resource->sendFailed();
            close(fd);
            return;
        }

        ssize_t readBytes = pread(fd, gammaTable.data(), gammaTable.size() * sizeof(uint16_t), 0);
        if (readBytes < 0 || (size_t)readBytes != gammaTable.size() * sizeof(uint16_t)) {
            LOGM(ERR, "Failed to read bytes");
            close(fd);

            if ((size_t)readBytes != gammaTable.size() * sizeof(uint16_t)) {
                wl_resource_post_error(gamma->resource(), ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA, "Gamma ramps size mismatch");
                return;
            }

            resource->sendFailed();
            return;
        }

        gammaTableSet = true;
        close(fd);
        applyToMonitor();
    });

    resource->sendGammaSize(gammaSize);

    listeners.monitorDestroy    = pMonitor->events.destroy.registerListener([this](std::any) { this->onMonitorDestroy(); });
    listeners.monitorDisconnect = pMonitor->events.destroy.registerListener([this](std::any) { this->onMonitorDestroy(); });
}

CGammaControl::~CGammaControl() {
    if (!gammaTableSet || !pMonitor)
        return;

    // reset the LUT if the client dies for whatever reason and doesn't unset the gamma
    wlr_output_state_set_gamma_lut(pMonitor->state.wlr(), 0, nullptr, nullptr, nullptr);
}

bool CGammaControl::good() {
    return resource->resource();
}

void CGammaControl::applyToMonitor() {
    if (!pMonitor)
        return; // ??

    LOGM(LOG, "setting to monitor {}", pMonitor->szName);

    if (!gammaTableSet) {
        wlr_output_state_set_gamma_lut(pMonitor->state.wlr(), 0, nullptr, nullptr, nullptr);
        return;
    }

    uint16_t* red   = &gammaTable.at(0);
    uint16_t* green = &gammaTable.at(gammaSize);
    uint16_t* blue  = &gammaTable.at(gammaSize * 2);

    wlr_output_state_set_gamma_lut(pMonitor->state.wlr(), gammaSize, red, green, blue);

    if (!pMonitor->state.test()) {
        LOGM(LOG, "setting to monitor {} failed", pMonitor->szName);
        wlr_output_state_set_gamma_lut(pMonitor->state.wlr(), 0, nullptr, nullptr, nullptr);
    }

    g_pHyprRenderer->damageMonitor(pMonitor);
}

CMonitor* CGammaControl::getMonitor() {
    return pMonitor;
}

void CGammaControl::onMonitorDestroy() {
    resource->sendFailed();
    pMonitor = nullptr;
}

CGammaControlProtocol::CGammaControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGammaControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwlrGammaControlManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrGammaControlManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrGammaControlManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetGammaControl([this](CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output) { this->onGetGammaControl(pMgr, id, output); });
}

void CGammaControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CGammaControlProtocol::destroyGammaControl(CGammaControl* gamma) {
    std::erase_if(m_vGammaControllers, [&](const auto& other) { return other.get() == gamma; });
}

void CGammaControlProtocol::onGetGammaControl(CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output) {
    const auto CLIENT = wl_resource_get_client(pMgr->resource());
    const auto RESOURCE =
        m_vGammaControllers.emplace_back(std::make_unique<CGammaControl>(std::make_shared<CZwlrGammaControlV1>(CLIENT, wl_resource_get_version(pMgr->resource()), id), output))
            .get();

    if (!RESOURCE->good()) {
        wl_resource_post_no_memory(pMgr->resource());
        m_vGammaControllers.pop_back();
        return;
    }
}

void CGammaControlProtocol::applyGammaToState(CMonitor* pMonitor) {
    for (auto& g : m_vGammaControllers) {
        if (g->getMonitor() != pMonitor)
            continue;

        g->applyToMonitor();
        break;
    }
}