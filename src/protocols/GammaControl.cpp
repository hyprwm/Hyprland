#include "GammaControl.hpp"
#include <fcntl.h>
#include <unistd.h>
#include "../helpers/Monitor.hpp"
#include "../protocols/core/Output.hpp"
#include "../render/Renderer.hpp"
using namespace Hyprutils::OS;

CGammaControl::CGammaControl(SP<CZwlrGammaControlV1> resource_, wl_resource* output) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    auto OUTPUTRES = CWLOutputResource::fromResource(output);

    if UNLIKELY (!OUTPUTRES) {
        LOGM(ERR, "No output in CGammaControl");
        m_resource->sendFailed();
        return;
    }

    m_monitor = OUTPUTRES->m_monitor;

    if UNLIKELY (!m_monitor || !m_monitor->m_output) {
        LOGM(ERR, "No CMonitor");
        m_resource->sendFailed();
        return;
    }

    for (auto const& g : PROTO::gamma->m_gammaControllers) {
        if UNLIKELY (g->m_monitor == m_monitor) {
            m_resource->sendFailed();
            return;
        }
    }

    m_gammaSize = m_monitor->m_output->getGammaSize();

    if UNLIKELY (m_gammaSize <= 0) {
        LOGM(ERR, "Output {} doesn't support gamma", m_monitor->m_name);
        m_resource->sendFailed();
        return;
    }

    m_gammaTable.resize(m_gammaSize * 3);

    m_resource->setDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });
    m_resource->setOnDestroy([this](CZwlrGammaControlV1* gamma) { PROTO::gamma->destroyGammaControl(this); });

    m_resource->setSetGamma([this](CZwlrGammaControlV1* gamma, int32_t fd) {
        CFileDescriptor gammaFd{fd};
        if UNLIKELY (!m_monitor) {
            LOGM(ERR, "setGamma for a dead monitor");
            m_resource->sendFailed();
            return;
        }

        LOGM(LOG, "setGamma for {}", m_monitor->m_name);

        // TODO: make CFileDescriptor getflags use F_GETFL
        int fdFlags = fcntl(gammaFd.get(), F_GETFL, 0);
        if UNLIKELY (fdFlags < 0) {
            LOGM(ERR, "Failed to get fd flags");
            m_resource->sendFailed();
            return;
        }

        // TODO: make CFileDescriptor setflags use F_SETFL
        if UNLIKELY (fcntl(gammaFd.get(), F_SETFL, fdFlags | O_NONBLOCK) < 0) {
            LOGM(ERR, "Failed to set fd flags");
            m_resource->sendFailed();
            return;
        }

        ssize_t readBytes = read(gammaFd.get(), m_gammaTable.data(), m_gammaTable.size() * sizeof(uint16_t));

        ssize_t moreBytes = 0;
        {
            const size_t BUF_SIZE      = 1;
            char         buf[BUF_SIZE] = {};
            moreBytes                  = read(gammaFd.get(), buf, BUF_SIZE);
        }

        if (readBytes < 0 || (size_t)readBytes != m_gammaTable.size() * sizeof(uint16_t) || moreBytes != 0) {
            LOGM(ERR, "Failed to read bytes");

            if ((size_t)readBytes != m_gammaTable.size() * sizeof(uint16_t), moreBytes > 0) {
                gamma->error(ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA, "Gamma ramps size mismatch");
                return;
            }

            m_resource->sendFailed();
            return;
        }

        m_gammaTableSet = true;

        // translate the table to AQ format
        std::vector<uint16_t> red, green, blue;
        red.resize(m_gammaTable.size() / 3);
        green.resize(m_gammaTable.size() / 3);
        blue.resize(m_gammaTable.size() / 3);
        for (size_t i = 0; i < m_gammaTable.size() / 3; ++i) {
            red.at(i)   = m_gammaTable.at(i);
            green.at(i) = m_gammaTable.at(m_gammaTable.size() / 3 + i);
            blue.at(i)  = m_gammaTable.at((m_gammaTable.size() / 3) * 2 + i);
        }

        for (size_t i = 0; i < m_gammaTable.size() / 3; ++i) {
            m_gammaTable.at(i * 3)     = red.at(i);
            m_gammaTable.at(i * 3 + 1) = green.at(i);
            m_gammaTable.at(i * 3 + 2) = blue.at(i);
        }

        applyToMonitor();
    });

    m_resource->sendGammaSize(m_gammaSize);

    m_listeners.monitorDestroy    = m_monitor->m_events.destroy.listen([this] { this->onMonitorDestroy(); });
    m_listeners.monitorDisconnect = m_monitor->m_events.disconnect.listen([this] { this->onMonitorDestroy(); });
}

CGammaControl::~CGammaControl() {
    if (!m_gammaTableSet || !m_monitor || !m_monitor->m_output)
        return;

    // reset the LUT if the client dies for whatever reason and doesn't unset the gamma
    m_monitor->m_output->state->setGammaLut({});
}

bool CGammaControl::good() {
    return m_resource->resource();
}

void CGammaControl::applyToMonitor() {
    if UNLIKELY (!m_monitor || !m_monitor->m_output)
        return; // ??

    LOGM(LOG, "setting to monitor {}", m_monitor->m_name);

    if (!m_gammaTableSet) {
        m_monitor->m_output->state->setGammaLut({});
        return;
    }

    m_monitor->m_output->state->setGammaLut(m_gammaTable);

    if (!m_monitor->m_state.test()) {
        LOGM(LOG, "setting to monitor {} failed", m_monitor->m_name);
        m_monitor->m_output->state->setGammaLut({});
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());
}

PHLMONITOR CGammaControl::getMonitor() {
    return m_monitor ? m_monitor.lock() : nullptr;
}

void CGammaControl::onMonitorDestroy() {
    LOGM(LOG, "Destroying gamma control for {}", m_monitor->m_name);
    m_resource->sendFailed();
}

CGammaControlProtocol::CGammaControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGammaControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwlrGammaControlManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrGammaControlManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrGammaControlManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetGammaControl([this](CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output) { this->onGetGammaControl(pMgr, id, output); });
}

void CGammaControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CGammaControlProtocol::destroyGammaControl(CGammaControl* gamma) {
    std::erase_if(m_gammaControllers, [&](const auto& other) { return other.get() == gamma; });
}

void CGammaControlProtocol::onGetGammaControl(CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_gammaControllers.emplace_back(makeUnique<CGammaControl>(makeShared<CZwlrGammaControlV1>(CLIENT, pMgr->version(), id), output)).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_gammaControllers.pop_back();
        return;
    }
}

void CGammaControlProtocol::applyGammaToState(PHLMONITOR pMonitor) {
    for (auto const& g : m_gammaControllers) {
        if (g->getMonitor() != pMonitor)
            continue;

        g->applyToMonitor();
        break;
    }
}
