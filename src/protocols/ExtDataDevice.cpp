#include "ExtDataDevice.hpp"
#include <algorithm>
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
using namespace Hyprutils::OS;

CExtDataOffer::CExtDataOffer(SP<CExtDataControlOfferV1> resource_, SP<IDataSource> source_) : m_source(source_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CExtDataControlOfferV1* r) { PROTO::extDataDevice->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtDataControlOfferV1* r) { PROTO::extDataDevice->destroyResource(this); });

    m_resource->setReceive([this](CExtDataControlOfferV1* r, const char* mime, int32_t fd) {
        CFileDescriptor sendFd{fd};
        if (!m_source) {
            LOGM(WARN, "Possible bug: Receive on an offer w/o a source");
            return;
        }

        if (m_dead) {
            LOGM(WARN, "Possible bug: Receive on an offer that's dead");
            return;
        }

        LOGM(LOG, "Offer {:x} asks to send data from source {:x}", (uintptr_t)this, (uintptr_t)m_source.get());

        m_source->send(mime, std::move(sendFd));
    });
}

bool CExtDataOffer::good() {
    return m_resource->resource();
}

void CExtDataOffer::sendData() {
    if UNLIKELY (!m_source)
        return;

    for (auto const& m : m_source->mimes()) {
        m_resource->sendOffer(m.c_str());
    }
}

CExtDataSource::CExtDataSource(SP<CExtDataControlSourceV1> resource_, SP<CExtDataDevice> device_) : m_device(device_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CExtDataControlSourceV1* r) {
        m_events.destroy.emit();
        PROTO::extDataDevice->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CExtDataControlSourceV1* r) {
        m_events.destroy.emit();
        PROTO::extDataDevice->destroyResource(this);
    });

    m_resource->setOffer([this](CExtDataControlSourceV1* r, const char* mime) { m_mimeTypes.emplace_back(mime); });
}

CExtDataSource::~CExtDataSource() {
    m_events.destroy.emit();
}

SP<CExtDataSource> CExtDataSource::fromResource(wl_resource* res) {
    auto data = (CExtDataSource*)(((CExtDataControlSourceV1*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CExtDataSource::good() {
    return m_resource->resource();
}

std::vector<std::string> CExtDataSource::mimes() {
    return m_mimeTypes;
}

void CExtDataSource::send(const std::string& mime, CFileDescriptor fd) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CExtDataSource::sendAskSend with non-existent mime");
        return;
    }

    m_resource->sendSend(mime.c_str(), fd.get());
}

void CExtDataSource::accepted(const std::string& mime) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end())
        LOGM(ERR, "Compositor/App bug: CExtDataSource::sendAccepted with non-existent mime");

    // ext has no accepted
}

void CExtDataSource::cancelled() {
    m_resource->sendCancelled();
}

void CExtDataSource::error(uint32_t code, const std::string& msg) {
    m_resource->error(code, msg);
}

CExtDataDevice::CExtDataDevice(SP<CExtDataControlDeviceV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CExtDataControlDeviceV1* r) { PROTO::extDataDevice->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtDataControlDeviceV1* r) { PROTO::extDataDevice->destroyResource(this); });

    m_resource->setSetSelection([](CExtDataControlDeviceV1* r, wl_resource* sourceR) {
        auto source = sourceR ? CExtDataSource::fromResource(sourceR) : CSharedPointer<CExtDataSource>{};
        if (!source) {
            LOGM(LOG, "ext reset selection received");
            g_pSeatManager->setCurrentSelection(nullptr);
            return;
        }

        if (source && source->used())
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        LOGM(LOG, "ext manager requests selection to {:x}", (uintptr_t)source.get());
        g_pSeatManager->setCurrentSelection(source);
    });

    m_resource->setSetPrimarySelection([](CExtDataControlDeviceV1* r, wl_resource* sourceR) {
        auto source = sourceR ? CExtDataSource::fromResource(sourceR) : CSharedPointer<CExtDataSource>{};
        if (!source) {
            LOGM(LOG, "ext reset primary selection received");
            g_pSeatManager->setCurrentPrimarySelection(nullptr);
            return;
        }

        if (source && source->used())
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        LOGM(LOG, "ext manager requests primary selection to {:x}", (uintptr_t)source.get());
        g_pSeatManager->setCurrentPrimarySelection(source);
    });
}

bool CExtDataDevice::good() {
    return m_resource->resource();
}

wl_client* CExtDataDevice::client() {
    return m_client;
}

void CExtDataDevice::sendInitialSelections() {
    PROTO::extDataDevice->sendSelectionToDevice(self.lock(), g_pSeatManager->m_selection.currentSelection.lock(), false);
    PROTO::extDataDevice->sendSelectionToDevice(self.lock(), g_pSeatManager->m_selection.currentPrimarySelection.lock(), true);
}

void CExtDataDevice::sendDataOffer(SP<CExtDataOffer> offer) {
    m_resource->sendDataOffer(offer->m_resource.get());
}

void CExtDataDevice::sendSelection(SP<CExtDataOffer> selection) {
    m_resource->sendSelection(selection->m_resource.get());
}

void CExtDataDevice::sendPrimarySelection(SP<CExtDataOffer> selection) {
    m_resource->sendPrimarySelection(selection->m_resource.get());
}

CExtDataControlManagerResource::CExtDataControlManagerResource(SP<CExtDataControlManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CExtDataControlManagerV1* r) { PROTO::extDataDevice->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtDataControlManagerV1* r) { PROTO::extDataDevice->destroyResource(this); });

    m_resource->setGetDataDevice([this](CExtDataControlManagerV1* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE = PROTO::extDataDevice->m_devices.emplace_back(makeShared<CExtDataDevice>(makeShared<CExtDataControlDeviceV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::extDataDevice->m_devices.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
        m_device       = RESOURCE;

        for (auto const& s : m_sources) {
            if (!s)
                continue;
            s->m_device = RESOURCE;
        }

        RESOURCE->sendInitialSelections();

        LOGM(LOG, "New ext data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setCreateDataSource([this](CExtDataControlManagerV1* r, uint32_t id) {
        std::erase_if(m_sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE =
            PROTO::extDataDevice->m_sources.emplace_back(makeShared<CExtDataSource>(makeShared<CExtDataControlSourceV1>(r->client(), r->version(), id), m_device.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::extDataDevice->m_sources.pop_back();
            return;
        }

        if (!m_device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->m_self = RESOURCE;

        m_sources.emplace_back(RESOURCE);

        LOGM(LOG, "New ext data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CExtDataControlManagerResource::good() {
    return m_resource->resource();
}

CExtDataDeviceProtocol::CExtDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CExtDataDeviceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CExtDataControlManagerResource>(makeShared<CExtDataControlManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(LOG, "New ext_data_control_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CExtDataDeviceProtocol::destroyResource(CExtDataControlManagerResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CExtDataDeviceProtocol::destroyResource(CExtDataSource* resource) {
    std::erase_if(m_sources, [&](const auto& other) { return other.get() == resource; });
}

void CExtDataDeviceProtocol::destroyResource(CExtDataDevice* resource) {
    std::erase_if(m_devices, [&](const auto& other) { return other.get() == resource; });
}

void CExtDataDeviceProtocol::destroyResource(CExtDataOffer* resource) {
    std::erase_if(m_offers, [&](const auto& other) { return other.get() == resource; });
}

void CExtDataDeviceProtocol::sendSelectionToDevice(SP<CExtDataDevice> dev, SP<IDataSource> sel, bool primary) {
    if (!sel) {
        if (primary)
            dev->m_resource->sendPrimarySelectionRaw(nullptr);
        else
            dev->m_resource->sendSelectionRaw(nullptr);
        return;
    }

    const auto OFFER = m_offers.emplace_back(makeShared<CExtDataOffer>(makeShared<CExtDataControlOfferV1>(dev->m_resource->client(), dev->m_resource->version(), 0), sel));

    if (!OFFER->good()) {
        dev->m_resource->noMemory();
        m_offers.pop_back();
        return;
    }

    OFFER->m_primary = primary;

    LOGM(LOG, "New {}offer {:x} for data source {:x}", primary ? "primary " : " ", (uintptr_t)OFFER.get(), (uintptr_t)sel.get());

    dev->sendDataOffer(OFFER);
    OFFER->sendData();
    if (primary)
        dev->sendPrimarySelection(OFFER);
    else
        dev->sendSelection(OFFER);
}

void CExtDataDeviceProtocol::setSelection(SP<IDataSource> source, bool primary) {
    for (auto const& o : m_offers) {
        if (o->m_source && o->m_source->hasDnd())
            continue;
        if (o->m_primary != primary)
            continue;
        o->m_dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting {}selection", primary ? "primary " : " ");

        for (auto const& d : m_devices) {
            sendSelectionToDevice(d, nullptr, primary);
        }

        return;
    }

    LOGM(LOG, "New {}selection for data source {:x}", primary ? "primary" : "", (uintptr_t)source.get());

    for (auto const& d : m_devices) {
        sendSelectionToDevice(d, source, primary);
    }
}

SP<CExtDataDevice> CExtDataDeviceProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::ranges::find_if(m_devices, [c](const auto& e) { return e->client() == c; });
    if (it == m_devices.end())
        return nullptr;
    return *it;
}
