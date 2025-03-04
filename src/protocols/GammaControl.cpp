#include "GammaControl.hpp"
#include <fcntl.h>
#include <unistd.h>
#include "../helpers/Monitor.hpp"
#include "../protocols/core/Output.hpp"
#include "../render/Renderer.hpp"
using namespace Hyprutils::OS;

CGammaControl::CGammaControl(SP<CZwlrGammaControlV1> resource_, wl_resource* output) : resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    auto OUTPUTRES = CWLOutputResource::fromResource(output);

    if UNLIKELY (!OUTPUTRES) {
        LOGM(ERR, "No output in CGammaControl");
        resource->sendFailed();
        return;
    }

    pMonitor = OUTPUTRES->monitor;

    if UNLIKELY (!pMonitor || !pMonitor->output) {
        LOGM(ERR, "No CMonitor");
        resource->sendFailed();
        return;
    }

    for (auto const& g : PROTO::gamma->m_vGammaControllers) {
        if UNLIKELY (g->pMonitor == pMonitor) {
            resource->sendFailed();
            return;
        }
    }

    gammaSize = pMonitor->output->getGammaSize();

    if UNLIKELY (gammaSize <= 0) {
        LOGM(ERR, "Output {} doesn't support gamma", pMonitor->szName);
        resource->sendFailed();
        return;
    }

    gammaTable.resize(gammaSize * 3);

    resource->setDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });
    resource->setOnDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });

    resource->setSetGamma([this](CZwlrGammaControlV1* gamma, int32_t fd) {
        CFileDescriptor gammaFd{fd};
        if UNLIKELY (!pMonitor) {
            LOGM(ERR, "setGamma for a dead monitor");
            resource->sendFailed();
            return;
        }

        LOGM(LOG, "setGamma for {}", pMonitor->szName);

        // TODO: make CFileDescriptor getflags use F_GETFL
        int fdFlags = fcntl(gammaFd.get(), F_GETFL, 0);
        if UNLIKELY (fdFlags < 0) {
            LOGM(ERR, "Failed to get fd flags");
            resource->sendFailed();
            return;
        }

        // TODO: make CFileDescriptor setflags use F_SETFL
        if UNLIKELY (fcntl(gammaFd.get(), F_SETFL, fdFlags | O_NONBLOCK) < 0) {
            LOGM(ERR, "Failed to set fd flags");
            resource->sendFailed();
            return;
        }

        ssize_t readBytes = pread(gammaFd.get(), gammaTable.data(), gammaTable.size() * sizeof(uint16_t), 0);
        if (readBytes < 0 || (size_t)readBytes != gammaTable.size() * sizeof(uint16_t)) {
            LOGM(ERR, "Failed to read bytes");

            if ((size_t)readBytes != gammaTable.size() * sizeof(uint16_t)) {
                gamma->error(ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA, "Gamma ramps size mismatch");
                return;
            }

            resource->sendFailed();
            return;
        }

        gammaTableSet = true;

        // translate the table to AQ format
        std::vector<uint16_t> red, green, blue;
        red.resize(gammaTable.size() / 3);
        green.resize(gammaTable.size() / 3);
        blue.resize(gammaTable.size() / 3);
        for (size_t i = 0; i < gammaTable.size() / 3; ++i) {
            red.at(i)   = gammaTable.at(i);
            green.at(i) = gammaTable.at(gammaTable.size() / 3 + i);
            blue.at(i)  = gammaTable.at((gammaTable.size() / 3) * 2 + i);
        }

        for (size_t i = 0; i < gammaTable.size() / 3; ++i) {
            gammaTable.at(i * 3)     = red.at(i);
            gammaTable.at(i * 3 + 1) = green.at(i);
            gammaTable.at(i * 3 + 2) = blue.at(i);
        }

        applyToMonitor();
    });

    resource->sendGammaSize(gammaSize);

    m_listeners.monitorDestroy    = pMonitor->events.destroy.registerListener([this](std::any) { this->onMonitorDestroy(); });
    m_listeners.monitorDisconnect = pMonitor->events.disconnect.registerListener([this](std::any) { this->onMonitorDestroy(); });
}

CGammaControl::~CGammaControl() {
    if (!gammaTableSet || !pMonitor || !pMonitor->output)
        return;

    // reset the LUT if the client dies for whatever reason and doesn't unset the gamma
    pMonitor->output->state->setGammaLut({});
}

bool CGammaControl::good() {
    return resource->resource();
}

void CGammaControl::applyToMonitor() {
    if UNLIKELY (!pMonitor || !pMonitor->output)
        return; // ??

    LOGM(LOG, "setting to monitor {}", pMonitor->szName);

    if (!gammaTableSet) {
        pMonitor->output->state->setGammaLut({});
        return;
    }

    pMonitor->output->state->setGammaLut(gammaTable);

    if (!pMonitor->state.test()) {
        LOGM(LOG, "setting to monitor {} failed", pMonitor->szName);
        pMonitor->output->state->setGammaLut({});
    }

    g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

PHLMONITOR CGammaControl::getMonitor() {
    return pMonitor ? pMonitor.lock() : nullptr;
}

void CGammaControl::onMonitorDestroy() {
    LOGM(LOG, "Destroying gamma control for {}", pMonitor->szName);
    resource->sendFailed();
}

CGammaControlProtocol::CGammaControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGammaControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CZwlrGammaControlManagerV1>(client, ver, id)).get();
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
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vGammaControllers.emplace_back(makeUnique<CGammaControl>(makeShared<CZwlrGammaControlV1>(CLIENT, pMgr->version(), id), output)).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vGammaControllers.pop_back();
        return;
    }
}

void CGammaControlProtocol::applyGammaToState(PHLMONITOR pMonitor) {
    for (auto const& g : m_vGammaControllers) {
        if (g->getMonitor() != pMonitor)
            continue;

        g->applyToMonitor();
        break;
    }
}
