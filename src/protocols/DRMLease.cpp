#include "DRMLease.hpp"
#include "../Compositor.hpp"
#include "../helpers/Monitor.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include <aquamarine/backend/DRM.hpp>
#include <fcntl.h>
using namespace Hyprutils::OS;

CDRMLeaseResource::CDRMLeaseResource(SP<CWpDrmLeaseV1> resource_, SP<CDRMLeaseRequestResource> request) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWpDrmLeaseV1* r) { PROTO::lease->destroyResource(this); });
    resource->setDestroy([this](CWpDrmLeaseV1* r) { PROTO::lease->destroyResource(this); });

    parent    = request->parent;
    requested = request->requested;

    for (auto const& m : requested) {
        if (!m->monitor || m->monitor->isBeingLeased) {
            LOGM(ERR, "Rejecting lease: no monitor or monitor is being leased for {}", (m->monitor ? m->monitor->szName : "null"));
            resource->sendFinished();
            return;
        }
    }

    // grant the lease if it is seemingly valid

    LOGM(LOG, "Leasing outputs: {}", [this]() {
        std::string roll;
        for (auto const& o : requested) {
            roll += std::format("{} ", o->monitor->szName);
        }
        return roll;
    }());

    std::vector<SP<Aquamarine::IOutput>> outputs;
    // reserve to avoid reallocations
    outputs.reserve(requested.size());

    for (auto const& m : requested) {
        outputs.emplace_back(m->monitor->output);
    }

    auto aqlease = Aquamarine::CDRMLease::create(outputs);
    if (!aqlease) {
        LOGM(ERR, "Rejecting lease: backend failed to alloc a lease");
        resource->sendFinished();
        return;
    }

    lease = aqlease;

    for (auto const& m : requested) {
        m->monitor->isBeingLeased = true;
    }

    m_listeners.destroyLease = lease->events.destroy.registerListener([this](std::any d) {
        for (auto const& m : requested) {
            if (m && m->monitor)
                m->monitor->isBeingLeased = false;
        }

        resource->sendFinished();
        LOGM(LOG, "Revoking lease for fd {}", lease->leaseFD);
    });

    LOGM(LOG, "Granting lease, sending fd {}", lease->leaseFD);

    resource->sendLeaseFd(lease->leaseFD);

    close(lease->leaseFD);
}

bool CDRMLeaseResource::good() {
    return resource->resource();
}

CDRMLeaseResource::~CDRMLeaseResource() {
    // destroy in this order to ensure listener gets called
    lease.reset();
    m_listeners.destroyLease.reset();
}

CDRMLeaseRequestResource::CDRMLeaseRequestResource(SP<CWpDrmLeaseRequestV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWpDrmLeaseRequestV1* r) { PROTO::lease->destroyResource(this); });

    resource->setRequestConnector([this](CWpDrmLeaseRequestV1* r, wl_resource* conn) {
        if (!conn) {
            resource->error(-1, "Null connector");
            return;
        }

        auto CONNECTOR = CDRMLeaseConnectorResource::fromResource(conn);

        if (std::find(requested.begin(), requested.end(), CONNECTOR) != requested.end()) {
            resource->error(WP_DRM_LEASE_REQUEST_V1_ERROR_DUPLICATE_CONNECTOR, "Connector already requested");
            return;
        }

        // TODO: when (if) we add multi, make sure this is from the correct device.

        requested.emplace_back(CONNECTOR);
    });

    resource->setSubmit([this](CWpDrmLeaseRequestV1* r, uint32_t id) {
        if (requested.empty()) {
            resource->error(WP_DRM_LEASE_REQUEST_V1_ERROR_EMPTY_LEASE, "No connectors added");
            return;
        }

        auto RESOURCE = makeShared<CDRMLeaseResource>(makeShared<CWpDrmLeaseV1>(resource->client(), resource->version(), id), self.lock());
        if UNLIKELY (!RESOURCE) {
            resource->noMemory();
            return;
        }

        PROTO::lease->m_vLeases.emplace_back(RESOURCE);

        // per protcol, after submit, this is dead.
        PROTO::lease->destroyResource(this);
    });
}

bool CDRMLeaseRequestResource::good() {
    return resource->resource();
}

SP<CDRMLeaseConnectorResource> CDRMLeaseConnectorResource::fromResource(wl_resource* res) {
    auto data = (CDRMLeaseConnectorResource*)(((CWpDrmLeaseConnectorV1*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

CDRMLeaseConnectorResource::CDRMLeaseConnectorResource(SP<CWpDrmLeaseConnectorV1> resource_, PHLMONITOR monitor_) : monitor(monitor_), resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWpDrmLeaseConnectorV1* r) { PROTO::lease->destroyResource(this); });
    resource->setDestroy([this](CWpDrmLeaseConnectorV1* r) { PROTO::lease->destroyResource(this); });

    resource->setData(this);

    m_listeners.destroyMonitor = monitor->events.destroy.registerListener([this](std::any d) {
        resource->sendWithdrawn();
        dead = true;
    });
}

bool CDRMLeaseConnectorResource::good() {
    return resource->resource();
}

void CDRMLeaseConnectorResource::sendData() {
    resource->sendName(monitor->szName.c_str());
    resource->sendDescription(monitor->szDescription.c_str());

    auto AQDRMOutput = (Aquamarine::CDRMOutput*)monitor->output.get();
    resource->sendConnectorId(AQDRMOutput->getConnectorID());

    resource->sendDone();
}

CDRMLeaseDeviceResource::CDRMLeaseDeviceResource(SP<CWpDrmLeaseDeviceV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWpDrmLeaseDeviceV1* r) { PROTO::lease->destroyResource(this); });
    resource->setRelease([this](CWpDrmLeaseDeviceV1* r) { PROTO::lease->destroyResource(this); });

    resource->setCreateLeaseRequest([this](CWpDrmLeaseDeviceV1* r, uint32_t id) {
        auto RESOURCE = makeShared<CDRMLeaseRequestResource>(makeShared<CWpDrmLeaseRequestV1>(resource->client(), resource->version(), id));
        if UNLIKELY (!RESOURCE) {
            resource->noMemory();
            return;
        }

        RESOURCE->self = RESOURCE;

        PROTO::lease->m_vRequests.emplace_back(RESOURCE);

        LOGM(LOG, "New lease request {}", id);

        RESOURCE->parent = self;
    });

    CFileDescriptor fd{((Aquamarine::CDRMBackend*)PROTO::lease->primaryDevice->backend.get())->getNonMasterFD()};
    if (!fd.isValid()) {
        LOGM(ERR, "Failed to dup fd in lease");
        return;
    }

    LOGM(LOG, "Sending DRMFD {} to new lease device", fd.get());
    resource->sendDrmFd(fd.get());

    for (auto const& m : PROTO::lease->primaryDevice->offeredOutputs) {
        if (m)
            sendConnector(m.lock());
    }

    resource->sendDone();
}

bool CDRMLeaseDeviceResource::good() {
    return resource->resource();
}

void CDRMLeaseDeviceResource::sendConnector(PHLMONITOR monitor) {
    if (std::find_if(connectorsSent.begin(), connectorsSent.end(), [monitor](const auto& e) { return e && !e->dead && e->monitor == monitor; }) != connectorsSent.end())
        return;

    auto RESOURCE = makeShared<CDRMLeaseConnectorResource>(makeShared<CWpDrmLeaseConnectorV1>(resource->client(), resource->version(), 0), monitor);
    if UNLIKELY (!RESOURCE) {
        resource->noMemory();
        return;
    }

    RESOURCE->parent = self;
    RESOURCE->self   = RESOURCE;

    LOGM(LOG, "Sending new connector {}", monitor->szName);

    connectorsSent.emplace_back(RESOURCE);
    PROTO::lease->m_vConnectors.emplace_back(RESOURCE);

    resource->sendConnector(RESOURCE->resource.get());

    RESOURCE->sendData();
}

CDRMLeaseDevice::CDRMLeaseDevice(SP<Aquamarine::CDRMBackend> drmBackend) : backend(drmBackend) {
    auto            drm = (Aquamarine::CDRMBackend*)drmBackend.get();

    CFileDescriptor fd{drm->getNonMasterFD()};

    if (!fd.isValid()) {
        LOGM(ERR, "Failed to dup fd for drm node {}", drm->gpuName);
        return;
    }

    success = true;
    name    = drm->gpuName;
}

CDRMLeaseProtocol::CDRMLeaseProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    for (auto const& b : g_pCompositor->m_pAqBackend->getImplementations()) {
        if (b->type() != Aquamarine::AQ_BACKEND_DRM)
            continue;

        auto drm = ((Aquamarine::CDRMBackend*)b.get())->self.lock();

        primaryDevice = makeShared<CDRMLeaseDevice>(drm);

        if (primaryDevice->success)
            break;
    }

    if (!primaryDevice || !primaryDevice->success)
        g_pEventLoopManager->doLater([]() { PROTO::lease.reset(); });
}

void CDRMLeaseProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CDRMLeaseDeviceResource>(makeShared<CWpDrmLeaseDeviceV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseDeviceResource* resource) {
    std::erase_if(m_vManagers, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseConnectorResource* resource) {
    for (const auto& m : m_vManagers) {
        std::erase_if(m->connectorsSent, [resource](const auto& e) { return e.expired() || e->dead || e.get() == resource; });
    }
    std::erase_if(m_vConnectors, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseRequestResource* resource) {
    std::erase_if(m_vRequests, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseResource* resource) {
    std::erase_if(m_vLeases, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::offer(PHLMONITOR monitor) {
    std::erase_if(primaryDevice->offeredOutputs, [](const auto& e) { return e.expired(); });
    if (std::find(primaryDevice->offeredOutputs.begin(), primaryDevice->offeredOutputs.end(), monitor) != primaryDevice->offeredOutputs.end())
        return;

    if (monitor->output->getBackend()->type() != Aquamarine::AQ_BACKEND_DRM)
        return;

    if (monitor->output->getBackend() != primaryDevice->backend) {
        LOGM(ERR, "Monitor {} cannot be leased: primaryDevice lease is for a different device", monitor->szName);
        return;
    }

    primaryDevice->offeredOutputs.emplace_back(monitor);

    for (auto const& m : m_vManagers) {
        m->sendConnector(monitor);
        m->resource->sendDone();
    }
}
