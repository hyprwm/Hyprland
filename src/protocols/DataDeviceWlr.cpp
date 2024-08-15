#include "DataDeviceWlr.hpp"
#include <algorithm>
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"

CWLRDataOffer::CWLRDataOffer(SP<CZwlrDataControlOfferV1> resource_, SP<IDataSource> source_) : source(source_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwlrDataControlOfferV1* r) { PROTO::dataWlr->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrDataControlOfferV1* r) { PROTO::dataWlr->destroyResource(this); });

    resource->setReceive([this](CZwlrDataControlOfferV1* r, const char* mime, int32_t fd) {
        if (!source) {
            LOGM(WARN, "Possible bug: Receive on an offer w/o a source");
            close(fd);
            return;
        }

        if (dead) {
            LOGM(WARN, "Possible bug: Receive on an offer that's dead");
            close(fd);
            return;
        }

        LOGM(LOG, "Offer {:x} asks to send data from source {:x}", (uintptr_t)this, (uintptr_t)source.get());

        source->send(mime, fd);
    });
}

bool CWLRDataOffer::good() {
    return resource->resource();
}

void CWLRDataOffer::sendData() {
    if (!source)
        return;

    for (auto& m : source->mimes()) {
        resource->sendOffer(m.c_str());
    }
}

CWLRDataSource::CWLRDataSource(SP<CZwlrDataControlSourceV1> resource_, SP<CWLRDataDevice> device_) : device(device_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CZwlrDataControlSourceV1* r) {
        events.destroy.emit();
        PROTO::dataWlr->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwlrDataControlSourceV1* r) {
        events.destroy.emit();
        PROTO::dataWlr->destroyResource(this);
    });

    resource->setOffer([this](CZwlrDataControlSourceV1* r, const char* mime) { mimeTypes.push_back(mime); });
}

CWLRDataSource::~CWLRDataSource() {
    events.destroy.emit();
}

SP<CWLRDataSource> CWLRDataSource::fromResource(wl_resource* res) {
    auto data = (CWLRDataSource*)(((CZwlrDataControlSourceV1*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CWLRDataSource::good() {
    return resource->resource();
}

std::vector<std::string> CWLRDataSource::mimes() {
    return mimeTypes;
}

void CWLRDataSource::send(const std::string& mime, uint32_t fd) {
    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLRDataSource::sendAskSend with non-existent mime");
        close(fd);
        return;
    }

    resource->sendSend(mime.c_str(), fd);
    close(fd);
}

void CWLRDataSource::accepted(const std::string& mime) {
    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end())
        LOGM(ERR, "Compositor/App bug: CWLRDataSource::sendAccepted with non-existent mime");

    // wlr has no accepted
}

void CWLRDataSource::cancelled() {
    resource->sendCancelled();
}

void CWLRDataSource::error(uint32_t code, const std::string& msg) {
    resource->error(code, msg);
}

CWLRDataDevice::CWLRDataDevice(SP<CZwlrDataControlDeviceV1> resource_) : resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CZwlrDataControlDeviceV1* r) { PROTO::dataWlr->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrDataControlDeviceV1* r) { PROTO::dataWlr->destroyResource(this); });

    resource->setSetSelection([this](CZwlrDataControlDeviceV1* r, wl_resource* sourceR) {
        auto source = sourceR ? CWLRDataSource::fromResource(sourceR) : CSharedPointer<CWLRDataSource>{};
        if (!source) {
            LOGM(LOG, "wlr reset selection received");
            g_pSeatManager->setCurrentSelection(nullptr);
            return;
        }

        if (source && source->used())
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        LOGM(LOG, "wlr manager requests selection to {:x}", (uintptr_t)source.get());
        g_pSeatManager->setCurrentSelection(source);
    });

    resource->setSetPrimarySelection([this](CZwlrDataControlDeviceV1* r, wl_resource* sourceR) {
        auto source = sourceR ? CWLRDataSource::fromResource(sourceR) : CSharedPointer<CWLRDataSource>{};
        if (!source) {
            LOGM(LOG, "wlr reset primary selection received");
            g_pSeatManager->setCurrentSelection(nullptr);
            return;
        }

        if (source && source->used())
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        LOGM(LOG, "wlr manager requests primary selection to {:x}", (uintptr_t)source.get());
        g_pSeatManager->setCurrentPrimarySelection(source);
    });
}

bool CWLRDataDevice::good() {
    return resource->resource();
}

wl_client* CWLRDataDevice::client() {
    return pClient;
}

void CWLRDataDevice::sendInitialSelections() {
    PROTO::dataWlr->sendSelectionToDevice(self.lock(), g_pSeatManager->selection.currentSelection.lock(), false);
    PROTO::dataWlr->sendSelectionToDevice(self.lock(), g_pSeatManager->selection.currentPrimarySelection.lock(), true);
}

void CWLRDataDevice::sendDataOffer(SP<CWLRDataOffer> offer) {
    resource->sendDataOffer(offer->resource.get());
}

void CWLRDataDevice::sendSelection(SP<CWLRDataOffer> selection) {
    resource->sendSelection(selection->resource.get());
}

void CWLRDataDevice::sendPrimarySelection(SP<CWLRDataOffer> selection) {
    resource->sendPrimarySelection(selection->resource.get());
}

CWLRDataControlManagerResource::CWLRDataControlManagerResource(SP<CZwlrDataControlManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwlrDataControlManagerV1* r) { PROTO::dataWlr->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrDataControlManagerV1* r) { PROTO::dataWlr->destroyResource(this); });

    resource->setGetDataDevice([this](CZwlrDataControlManagerV1* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE = PROTO::dataWlr->m_vDevices.emplace_back(makeShared<CWLRDataDevice>(makeShared<CZwlrDataControlDeviceV1>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::dataWlr->m_vDevices.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
        device         = RESOURCE;

        for (auto& s : sources) {
            if (!s)
                continue;
            s->device = RESOURCE;
        }

        RESOURCE->sendInitialSelections();

        LOGM(LOG, "New wlr data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    resource->setCreateDataSource([this](CZwlrDataControlManagerV1* r, uint32_t id) {
        std::erase_if(sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE =
            PROTO::dataWlr->m_vSources.emplace_back(makeShared<CWLRDataSource>(makeShared<CZwlrDataControlSourceV1>(r->client(), r->version(), id), device.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::dataWlr->m_vSources.pop_back();
            return;
        }

        if (!device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->self = RESOURCE;

        sources.push_back(RESOURCE);

        LOGM(LOG, "New wlr data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CWLRDataControlManagerResource::good() {
    return resource->resource();
}

CDataDeviceWLRProtocol::CDataDeviceWLRProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CDataDeviceWLRProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CWLRDataControlManagerResource>(makeShared<CZwlrDataControlManagerV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(LOG, "New wlr_data_control_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataControlManagerResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataSource* resource) {
    std::erase_if(m_vSources, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataDevice* resource) {
    std::erase_if(m_vDevices, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataOffer* resource) {
    std::erase_if(m_vOffers, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::sendSelectionToDevice(SP<CWLRDataDevice> dev, SP<IDataSource> sel, bool primary) {
    if (!sel) {
        if (primary)
            dev->resource->sendPrimarySelectionRaw(nullptr);
        else
            dev->resource->sendSelectionRaw(nullptr);
        return;
    }

    const auto OFFER = m_vOffers.emplace_back(makeShared<CWLRDataOffer>(makeShared<CZwlrDataControlOfferV1>(dev->resource->client(), dev->resource->version(), 0), sel));

    if (!OFFER->good()) {
        dev->resource->noMemory();
        m_vOffers.pop_back();
        return;
    }

    OFFER->primary = primary;

    LOGM(LOG, "New {}offer {:x} for data source {:x}", primary ? "primary " : " ", (uintptr_t)OFFER.get(), (uintptr_t)sel.get());

    dev->sendDataOffer(OFFER);
    OFFER->sendData();
    if (primary)
        dev->sendPrimarySelection(OFFER);
    else
        dev->sendSelection(OFFER);
}

void CDataDeviceWLRProtocol::setSelection(SP<IDataSource> source, bool primary) {
    for (auto& o : m_vOffers) {
        if (o->source && o->source->hasDnd())
            continue;
        if (o->primary != primary)
            continue;
        o->dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting {}selection", primary ? "primary " : " ");

        for (auto& d : m_vDevices) {
            sendSelectionToDevice(d, nullptr, primary);
        }

        return;
    }

    LOGM(LOG, "New {}selection for data source {:x}", primary ? "primary" : "", (uintptr_t)source.get());

    for (auto& d : m_vDevices) {
        sendSelectionToDevice(d, source, primary);
    }
}

SP<CWLRDataDevice> CDataDeviceWLRProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::find_if(m_vDevices.begin(), m_vDevices.end(), [c](const auto& e) { return e->client() == c; });
    if (it == m_vDevices.end())
        return nullptr;
    return *it;
}
