#include "DRMLease.hpp"
#include "../Compositor.hpp"
#include "../helpers/Monitor.hpp"
#include "drm-lease-v1.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "protocols/WaylandProtocol.hpp"
#include <algorithm>
#include <aquamarine/backend/DRM.hpp>
#include <fcntl.h>
using namespace Hyprutils::OS;

CDRMLeaseResource::CDRMLeaseResource(SP<CWpDrmLeaseV1> resource_, SP<CDRMLeaseRequestResource> request) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_parent    = request->m_parent;
    m_requested = request->m_requested;

    m_resource->setOnDestroy([this](CWpDrmLeaseV1* r) {
        if (m_parent && PROTO::lease.contains(m_parent->m_deviceName))
            PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });
    m_resource->setDestroy([this](CWpDrmLeaseV1* r) {
        if (m_parent && PROTO::lease.contains(m_parent->m_deviceName))
            PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });

    for (auto const& m : m_requested) {
        if (!m->m_monitor || m->m_monitor->m_isBeingLeased) {
            LOGM(ERR, "Rejecting lease: no monitor or monitor is being leased for {}", (m->m_monitor ? m->m_monitor->m_name : "null"));
            m_resource->sendFinished();
            return;
        }
    }

    // grant the lease if it is seemingly valid

    LOGM(LOG, "Leasing outputs: {}", [this]() {
        std::string roll;
        for (auto const& o : m_requested) {
            roll += std::format("{} ", o->m_monitor->m_name);
        }
        return roll;
    }());

    std::vector<SP<Aquamarine::IOutput>> outputs;
    // reserve to avoid reallocations
    outputs.reserve(m_requested.size());

    for (auto const& m : m_requested) {
        outputs.emplace_back(m->m_monitor->m_output);
    }

    auto aqlease = Aquamarine::CDRMLease::create(outputs);
    if (!aqlease) {
        LOGM(ERR, "Rejecting lease: backend failed to alloc a lease");
        m_resource->sendFinished();
        return;
    }

    m_lease = aqlease;

    for (auto const& m : m_requested) {
        m->m_monitor->m_isBeingLeased = true;
    }

    m_listeners.destroyLease = m_lease->events.destroy.registerListener([this](std::any d) {
        for (auto const& m : m_requested) {
            if (m && m->m_monitor)
                m->m_monitor->m_isBeingLeased = false;
        }

        m_resource->sendFinished();
        LOGM(LOG, "Revoking lease for fd {}", m_lease->leaseFD);
    });

    LOGM(LOG, "Granting lease, sending fd {}", m_lease->leaseFD);

    m_resource->sendLeaseFd(m_lease->leaseFD);

    close(m_lease->leaseFD);
}

bool CDRMLeaseResource::good() {
    return m_resource->resource();
}

CDRMLeaseResource::~CDRMLeaseResource() {
    // destroy in this order to ensure listener gets called
    m_lease.reset();
    m_listeners.destroyLease.reset();
}

CDRMLeaseRequestResource::CDRMLeaseRequestResource(WP<CDRMLeaseDeviceResource> parent_, SP<CWpDrmLeaseRequestV1> resource_) : m_parent(parent_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWpDrmLeaseRequestV1* r) {
        if (m_parent && PROTO::lease.contains(m_parent->m_deviceName))
            PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });

    m_resource->setRequestConnector([this](CWpDrmLeaseRequestV1* r, wl_resource* conn) {
        if (!conn) {
            m_resource->error(-1, "Null connector");
            return;
        }

        auto CONNECTOR = CDRMLeaseConnectorResource::fromResource(conn);

        if (std::ranges::find(m_requested, CONNECTOR) != m_requested.end()) {
            m_resource->error(WP_DRM_LEASE_REQUEST_V1_ERROR_DUPLICATE_CONNECTOR, "Connector already requested");
            return;
        }

        auto& lease = PROTO::lease.at(m_parent->m_deviceName);

        if (std::ranges::find(lease->m_connectors.begin(), lease->m_connectors.end(), CONNECTOR) == lease->m_connectors.end()) {
            m_resource->error(WP_DRM_LEASE_REQUEST_V1_ERROR_WRONG_DEVICE, "Connector requested for wrong device");
            return;
        }

        m_requested.emplace_back(CONNECTOR);
    });

    m_resource->setSubmit([this](CWpDrmLeaseRequestV1* r, uint32_t id) {
        if (m_requested.empty()) {
            m_resource->error(WP_DRM_LEASE_REQUEST_V1_ERROR_EMPTY_LEASE, "No connectors added");
            return;
        }

        auto RESOURCE = makeShared<CDRMLeaseResource>(makeShared<CWpDrmLeaseV1>(m_resource->client(), m_resource->version(), id), m_self.lock());
        if UNLIKELY (!RESOURCE) {
            m_resource->noMemory();
            return;
        }

        PROTO::lease.at(m_parent->m_deviceName)->m_leases.emplace_back(RESOURCE);

        // per protcol, after submit, this is dead.
        PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });
}

bool CDRMLeaseRequestResource::good() {
    return m_resource->resource();
}

SP<CDRMLeaseConnectorResource> CDRMLeaseConnectorResource::fromResource(wl_resource* res) {
    auto data = (CDRMLeaseConnectorResource*)(((CWpDrmLeaseConnectorV1*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

CDRMLeaseConnectorResource::CDRMLeaseConnectorResource(WP<CDRMLeaseDeviceResource> parent_, SP<CWpDrmLeaseConnectorV1> resource_, PHLMONITOR monitor_) :
    m_parent(parent_), m_monitor(monitor_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWpDrmLeaseConnectorV1* r) {
        if (m_parent && PROTO::lease.contains(m_parent->m_deviceName))
            PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });
    m_resource->setDestroy([this](CWpDrmLeaseConnectorV1* r) {
        if (m_parent && PROTO::lease.contains(m_parent->m_deviceName))
            PROTO::lease.at(m_parent->m_deviceName)->destroyResource(this);
    });

    m_resource->setData(this);

    m_listeners.destroyMonitor = m_monitor->m_events.destroy.registerListener([this](std::any d) {
        m_resource->sendWithdrawn();
        m_dead = true;
    });
}

bool CDRMLeaseConnectorResource::good() {
    return m_resource->resource();
}

void CDRMLeaseConnectorResource::sendData() {
    m_resource->sendName(m_monitor->m_name.c_str());
    m_resource->sendDescription(m_monitor->m_description.c_str());

    auto AQDRMOutput = (Aquamarine::CDRMOutput*)m_monitor->m_output.get();
    m_resource->sendConnectorId(AQDRMOutput->getConnectorID());

    m_resource->sendDone();
}

CDRMLeaseDeviceResource::CDRMLeaseDeviceResource(std::string deviceName_, SP<CWpDrmLeaseDeviceV1> resource_) : m_deviceName(deviceName_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWpDrmLeaseDeviceV1* r) {
        if (PROTO::lease.contains(m_deviceName))
            PROTO::lease.at(m_deviceName)->destroyResource(this);
    });
    m_resource->setRelease([this](CWpDrmLeaseDeviceV1* r) {
        if (PROTO::lease.contains(m_deviceName))
            PROTO::lease.at(m_deviceName)->destroyResource(this);
    });

    m_resource->setCreateLeaseRequest([this](CWpDrmLeaseDeviceV1* r, uint32_t id) {
        auto RESOURCE = makeShared<CDRMLeaseRequestResource>(m_self, makeShared<CWpDrmLeaseRequestV1>(m_resource->client(), m_resource->version(), id));
        if UNLIKELY (!RESOURCE) {
            m_resource->noMemory();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        PROTO::lease.at(m_deviceName)->m_requests.emplace_back(RESOURCE);

        LOGM(LOG, "New lease request {}", id);

        RESOURCE->m_parent = m_self;
    });

    CFileDescriptor fd{PROTO::lease.at(m_deviceName)->m_backend.get()->getNonMasterFD()};
    if (!fd.isValid()) {
        LOGM(ERR, "Failed to dup fd in lease");
        return;
    }

    LOGM(LOG, "Sending DRMFD {} to new lease device", fd.get());
    m_resource->sendDrmFd(fd.get());

    for (auto const& m : PROTO::lease.at(m_deviceName)->m_offeredOutputs) {
        if (m)
            sendConnector(m.lock());
    }

    m_resource->sendDone();
}

bool CDRMLeaseDeviceResource::good() {
    return m_resource->resource();
}

void CDRMLeaseDeviceResource::sendConnector(PHLMONITOR monitor) {
    if (std::ranges::find_if(m_connectorsSent, [monitor](const auto& e) { return e && !e->m_dead && e->m_monitor == monitor; }) != m_connectorsSent.end())
        return;

    auto RESOURCE = makeShared<CDRMLeaseConnectorResource>(m_self, makeShared<CWpDrmLeaseConnectorV1>(m_resource->client(), m_resource->version(), 0), monitor);
    if UNLIKELY (!RESOURCE) {
        m_resource->noMemory();
        return;
    }

    RESOURCE->m_parent = m_self;
    RESOURCE->m_self   = RESOURCE;

    LOGM(LOG, "Sending new connector {}", monitor->m_name);

    m_connectorsSent.emplace_back(RESOURCE);
    PROTO::lease.at(m_deviceName)->m_connectors.emplace_back(RESOURCE);

    m_resource->sendConnector(RESOURCE->m_resource.get());

    RESOURCE->sendData();
}

CDRMLeaseProtocol::CDRMLeaseProtocol(const wl_interface* iface, const int& ver, const std::string& name, SP<Aquamarine::IBackendImplementation> backend_) :
    IWaylandProtocol(iface, ver, name) {
    if (backend_->type() != Aquamarine::AQ_BACKEND_DRM)
        return;

    m_backend    = ((Aquamarine::CDRMBackend*)backend_.get())->self.lock();
    m_deviceName = m_backend->gpuName;

    CFileDescriptor fd{m_backend->getNonMasterFD()};

    if (!fd.isValid()) {
        LOGM(ERR, "Failed to dup fd for drm node {}", m_deviceName);
        return;
    }

    m_success = true;
}

void CDRMLeaseProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CDRMLeaseDeviceResource>(m_deviceName, makeShared<CWpDrmLeaseDeviceV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseDeviceResource* resource) {
    std::erase_if(m_managers, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseConnectorResource* resource) {
    for (const auto& m : m_managers) {
        std::erase_if(m->m_connectorsSent, [resource](const auto& e) { return e.expired() || e->m_dead || e.get() == resource; });
    }
    std::erase_if(m_connectors, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseRequestResource* resource) {
    std::erase_if(m_requests, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::destroyResource(CDRMLeaseResource* resource) {
    std::erase_if(m_leases, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMLeaseProtocol::offer(PHLMONITOR monitor) {
    std::erase_if(m_offeredOutputs, [](const auto& e) { return e.expired(); });
    if (std::ranges::find(m_offeredOutputs.begin(), m_offeredOutputs.end(), monitor) != m_offeredOutputs.end())
        return;

    if (monitor->m_output->getBackend()->type() != Aquamarine::AQ_BACKEND_DRM)
        return;

    if (monitor->m_output->getBackend() != m_backend) {
        LOGM(ERR, "Monitor {} cannot be leased: lease is for a different device", monitor->m_name);
        return;
    }

    m_offeredOutputs.emplace_back(monitor);

    for (auto const& m : m_managers) {
        m->sendConnector(monitor);
        m->m_resource->sendDone();
    }
}

std::string CDRMLeaseProtocol::getDeviceName() {
    return m_deviceName;
}

SP<Aquamarine::IBackendImplementation> CDRMLeaseProtocol::getBackend() {
    return m_backend;
}

bool CDRMLeaseProtocol::good() {
    return m_success;
}
