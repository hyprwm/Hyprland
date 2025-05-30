#include "DataDeviceWlr.hpp"
#include <algorithm>
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
using namespace Hyprutils::OS;

CWLRDataOffer::CWLRDataOffer(SP<CZwlrDataControlOfferV1> resource_, SP<IDataSource> source_) : m_source(source_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrDataControlOfferV1* r) { PROTO::dataWlr->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrDataControlOfferV1* r) { PROTO::dataWlr->destroyResource(this); });

    m_resource->setReceive([this](CZwlrDataControlOfferV1* r, const char* mime, int32_t fd) {
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

bool CWLRDataOffer::good() {
    return m_resource->resource();
}

void CWLRDataOffer::sendData() {
    if UNLIKELY (!m_source)
        return;

    for (auto const& m : m_source->mimes()) {
        m_resource->sendOffer(m.c_str());
    }
}

CWLRDataSource::CWLRDataSource(SP<CZwlrDataControlSourceV1> resource_, SP<CWLRDataDevice> device_) : m_device(device_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CZwlrDataControlSourceV1* r) {
        m_events.destroy.emit();
        PROTO::dataWlr->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CZwlrDataControlSourceV1* r) {
        m_events.destroy.emit();
        PROTO::dataWlr->destroyResource(this);
    });

    m_resource->setOffer([this](CZwlrDataControlSourceV1* r, const char* mime) { m_mimeTypes.emplace_back(mime); });
}

CWLRDataSource::~CWLRDataSource() {
    m_events.destroy.emit();
}

SP<CWLRDataSource> CWLRDataSource::fromResource(wl_resource* res) {
    auto data = (CWLRDataSource*)(((CZwlrDataControlSourceV1*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CWLRDataSource::good() {
    return m_resource->resource();
}

std::vector<std::string> CWLRDataSource::mimes() {
    return m_mimeTypes;
}

void CWLRDataSource::send(const std::string& mime, CFileDescriptor fd) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLRDataSource::sendAskSend with non-existent mime");
        return;
    }

    m_resource->sendSend(mime.c_str(), fd.get());
}

void CWLRDataSource::accepted(const std::string& mime) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end())
        LOGM(ERR, "Compositor/App bug: CWLRDataSource::sendAccepted with non-existent mime");

    // wlr has no accepted
}

void CWLRDataSource::cancelled() {
    m_resource->sendCancelled();
}

void CWLRDataSource::error(uint32_t code, const std::string& msg) {
    m_resource->error(code, msg);
}

CWLRDataDevice::CWLRDataDevice(SP<CZwlrDataControlDeviceV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CZwlrDataControlDeviceV1* r) { PROTO::dataWlr->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrDataControlDeviceV1* r) { PROTO::dataWlr->destroyResource(this); });

    m_resource->setSetSelection([](CZwlrDataControlDeviceV1* r, wl_resource* sourceR) {
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

    m_resource->setSetPrimarySelection([](CZwlrDataControlDeviceV1* r, wl_resource* sourceR) {
        auto source = sourceR ? CWLRDataSource::fromResource(sourceR) : CSharedPointer<CWLRDataSource>{};
        if (!source) {
            LOGM(LOG, "wlr reset primary selection received");
            g_pSeatManager->setCurrentPrimarySelection(nullptr);
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
    return m_resource->resource();
}

wl_client* CWLRDataDevice::client() {
    return m_client;
}

void CWLRDataDevice::sendInitialSelections() {
    PROTO::dataWlr->sendSelectionToDevice(self.lock(), g_pSeatManager->m_selection.currentSelection.lock(), false);
    PROTO::dataWlr->sendSelectionToDevice(self.lock(), g_pSeatManager->m_selection.currentPrimarySelection.lock(), true);
}

void CWLRDataDevice::sendDataOffer(SP<CWLRDataOffer> offer) {
    m_resource->sendDataOffer(offer->m_resource.get());
}

void CWLRDataDevice::sendSelection(SP<CWLRDataOffer> selection) {
    m_resource->sendSelection(selection->m_resource.get());
}

void CWLRDataDevice::sendPrimarySelection(SP<CWLRDataOffer> selection) {
    m_resource->sendPrimarySelection(selection->m_resource.get());
}

CWLRDataControlManagerResource::CWLRDataControlManagerResource(SP<CZwlrDataControlManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrDataControlManagerV1* r) { PROTO::dataWlr->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrDataControlManagerV1* r) { PROTO::dataWlr->destroyResource(this); });

    m_resource->setGetDataDevice([this](CZwlrDataControlManagerV1* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE = PROTO::dataWlr->m_devices.emplace_back(makeShared<CWLRDataDevice>(makeShared<CZwlrDataControlDeviceV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::dataWlr->m_devices.pop_back();
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

        LOGM(LOG, "New wlr data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setCreateDataSource([this](CZwlrDataControlManagerV1* r, uint32_t id) {
        std::erase_if(m_sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE =
            PROTO::dataWlr->m_sources.emplace_back(makeShared<CWLRDataSource>(makeShared<CZwlrDataControlSourceV1>(r->client(), r->version(), id), m_device.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::dataWlr->m_sources.pop_back();
            return;
        }

        if (!m_device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->m_self = RESOURCE;

        m_sources.emplace_back(RESOURCE);

        LOGM(LOG, "New wlr data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CWLRDataControlManagerResource::good() {
    return m_resource->resource();
}

CDataDeviceWLRProtocol::CDataDeviceWLRProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CDataDeviceWLRProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CWLRDataControlManagerResource>(makeShared<CZwlrDataControlManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(LOG, "New wlr_data_control_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataControlManagerResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataSource* resource) {
    std::erase_if(m_sources, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataDevice* resource) {
    std::erase_if(m_devices, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::destroyResource(CWLRDataOffer* resource) {
    std::erase_if(m_offers, [&](const auto& other) { return other.get() == resource; });
}

void CDataDeviceWLRProtocol::sendSelectionToDevice(SP<CWLRDataDevice> dev, SP<IDataSource> sel, bool primary) {
    if (!sel) {
        if (primary)
            dev->m_resource->sendPrimarySelectionRaw(nullptr);
        else
            dev->m_resource->sendSelectionRaw(nullptr);
        return;
    }

    const auto OFFER = m_offers.emplace_back(makeShared<CWLRDataOffer>(makeShared<CZwlrDataControlOfferV1>(dev->m_resource->client(), dev->m_resource->version(), 0), sel));

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

void CDataDeviceWLRProtocol::setSelection(SP<IDataSource> source, bool primary) {
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

SP<CWLRDataDevice> CDataDeviceWLRProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::ranges::find_if(m_devices, [c](const auto& e) { return e->client() == c; });
    if (it == m_devices.end())
        return nullptr;
    return *it;
}
