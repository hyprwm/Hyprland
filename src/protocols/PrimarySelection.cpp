#include "PrimarySelection.hpp"
#include <algorithm>
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "../config/ConfigValue.hpp"

CPrimarySelectionOffer::CPrimarySelectionOffer(SP<CZwpPrimarySelectionOfferV1> resource_, SP<IDataSource> source_) : source(source_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpPrimarySelectionOfferV1* r) { PROTO::primarySelection->destroyResource(this); });
    resource->setOnDestroy([this](CZwpPrimarySelectionOfferV1* r) { PROTO::primarySelection->destroyResource(this); });

    resource->setReceive([this](CZwpPrimarySelectionOfferV1* r, const char* mime, int32_t fd) {
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

bool CPrimarySelectionOffer::good() {
    return resource->resource();
}

void CPrimarySelectionOffer::sendData() {
    if (!source)
        return;

    for (auto& m : source->mimes()) {
        resource->sendOffer(m.c_str());
    }
}

CPrimarySelectionSource::CPrimarySelectionSource(SP<CZwpPrimarySelectionSourceV1> resource_, SP<CPrimarySelectionDevice> device_) : device(device_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CZwpPrimarySelectionSourceV1* r) {
        events.destroy.emit();
        PROTO::primarySelection->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwpPrimarySelectionSourceV1* r) {
        events.destroy.emit();
        PROTO::primarySelection->destroyResource(this);
    });

    resource->setOffer([this](CZwpPrimarySelectionSourceV1* r, const char* mime) { mimeTypes.push_back(mime); });
}

CPrimarySelectionSource::~CPrimarySelectionSource() {
    events.destroy.emit();
}

SP<CPrimarySelectionSource> CPrimarySelectionSource::fromResource(wl_resource* res) {
    auto data = (CPrimarySelectionSource*)(((CZwpPrimarySelectionSourceV1*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CPrimarySelectionSource::good() {
    return resource->resource();
}

std::vector<std::string> CPrimarySelectionSource::mimes() {
    return mimeTypes;
}

void CPrimarySelectionSource::send(const std::string& mime, uint32_t fd) {
    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CPrimarySelectionSource::sendAskSend with non-existent mime");
        close(fd);
        return;
    }

    resource->sendSend(mime.c_str(), fd);
    close(fd);
}

void CPrimarySelectionSource::accepted(const std::string& mime) {
    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end())
        LOGM(ERR, "Compositor/App bug: CPrimarySelectionSource::sendAccepted with non-existent mime");

    // primary sel has no accepted
}

void CPrimarySelectionSource::cancelled() {
    resource->sendCancelled();
}

void CPrimarySelectionSource::error(uint32_t code, const std::string& msg) {
    resource->error(code, msg);
}

CPrimarySelectionDevice::CPrimarySelectionDevice(SP<CZwpPrimarySelectionDeviceV1> resource_) : resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CZwpPrimarySelectionDeviceV1* r) { PROTO::primarySelection->destroyResource(this); });
    resource->setOnDestroy([this](CZwpPrimarySelectionDeviceV1* r) { PROTO::primarySelection->destroyResource(this); });

    resource->setSetSelection([this](CZwpPrimarySelectionDeviceV1* r, wl_resource* sourceR, uint32_t serial) {
        static auto PPRIMARYSEL = CConfigValue<Hyprlang::INT>("misc:middle_click_paste");

        if (!*PPRIMARYSEL) {
            LOGM(LOG, "Ignoring primary selection: disabled in config");
            g_pSeatManager->setCurrentPrimarySelection(nullptr);
            return;
        }

        auto source = sourceR ? CPrimarySelectionSource::fromResource(sourceR) : CSharedPointer<CPrimarySelectionSource>{};
        if (!source) {
            LOGM(LOG, "wlr reset selection received");
            g_pSeatManager->setCurrentPrimarySelection(nullptr);
            return;
        }

        if (source && source->used())
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        LOGM(LOG, "wlr manager requests selection to {:x}", (uintptr_t)source.get());
        g_pSeatManager->setCurrentPrimarySelection(source);
    });
}

bool CPrimarySelectionDevice::good() {
    return resource->resource();
}

wl_client* CPrimarySelectionDevice::client() {
    return pClient;
}

void CPrimarySelectionDevice::sendDataOffer(SP<CPrimarySelectionOffer> offer) {
    resource->sendDataOffer(offer->resource.get());
}

void CPrimarySelectionDevice::sendSelection(SP<CPrimarySelectionOffer> selection) {
    if (!selection)
        resource->sendSelectionRaw(nullptr);
    else
        resource->sendSelection(selection->resource.get());
}

CPrimarySelectionManager::CPrimarySelectionManager(SP<CZwpPrimarySelectionDeviceManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwpPrimarySelectionDeviceManagerV1* r) { PROTO::primarySelection->destroyResource(this); });

    resource->setGetDevice([this](CZwpPrimarySelectionDeviceManagerV1* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE =
            PROTO::primarySelection->m_vDevices.emplace_back(makeShared<CPrimarySelectionDevice>(makeShared<CZwpPrimarySelectionDeviceV1>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::primarySelection->m_vDevices.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
        device         = RESOURCE;

        for (auto& s : sources) {
            if (!s)
                continue;
            s->device = RESOURCE;
        }

        LOGM(LOG, "New primary selection data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    resource->setCreateSource([this](CZwpPrimarySelectionDeviceManagerV1* r, uint32_t id) {
        std::erase_if(sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE = PROTO::primarySelection->m_vSources.emplace_back(
            makeShared<CPrimarySelectionSource>(makeShared<CZwpPrimarySelectionSourceV1>(r->client(), r->version(), id), device.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::primarySelection->m_vSources.pop_back();
            return;
        }

        if (!device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->self = RESOURCE;

        sources.push_back(RESOURCE);

        LOGM(LOG, "New primary selection data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CPrimarySelectionManager::good() {
    return resource->resource();
}

CPrimarySelectionProtocol::CPrimarySelectionProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPrimarySelectionProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CPrimarySelectionManager>(makeShared<CZwpPrimarySelectionDeviceManagerV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(LOG, "New primary_seletion_manager at {:x}", (uintptr_t)RESOURCE.get());

    // we need to do it here because protocols come before seatMgr
    if (!listeners.onPointerFocusChange)
        listeners.onPointerFocusChange = g_pSeatManager->events.pointerFocusChange.registerListener([this](std::any d) { this->onPointerFocus(); });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionSource* resource) {
    std::erase_if(m_vSources, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionDevice* resource) {
    std::erase_if(m_vDevices, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionOffer* resource) {
    std::erase_if(m_vOffers, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::sendSelectionToDevice(SP<CPrimarySelectionDevice> dev, SP<IDataSource> sel) {
    if (!sel) {
        dev->sendSelection(nullptr);
        return;
    }

    const auto OFFER =
        m_vOffers.emplace_back(makeShared<CPrimarySelectionOffer>(makeShared<CZwpPrimarySelectionOfferV1>(dev->resource->client(), dev->resource->version(), 0), sel));

    if (!OFFER->good()) {
        dev->resource->noMemory();
        m_vOffers.pop_back();
        return;
    }

    LOGM(LOG, "New offer {:x} for data source {:x}", (uintptr_t)OFFER.get(), (uintptr_t)sel.get());

    dev->sendDataOffer(OFFER);
    OFFER->sendData();
    dev->sendSelection(OFFER);
}

void CPrimarySelectionProtocol::setSelection(SP<IDataSource> source) {
    for (auto& o : m_vOffers) {
        if (o->source && o->source->hasDnd())
            continue;
        o->dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting selection");

        if (!g_pSeatManager->state.pointerFocusResource)
            return;

        auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.pointerFocusResource->client());
        if (DESTDEVICE)
            sendSelectionToDevice(DESTDEVICE, nullptr);

        return;
    }

    LOGM(LOG, "New selection for data source {:x}", (uintptr_t)source.get());

    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.pointerFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::setSelection: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, source);
}

void CPrimarySelectionProtocol::updateSelection() {
    if (!g_pSeatManager->state.pointerFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.pointerFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CPrimarySelectionProtocol::updateSelection: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, g_pSeatManager->selection.currentPrimarySelection.lock());
}

void CPrimarySelectionProtocol::onPointerFocus() {
    for (auto& o : m_vOffers) {
        o->dead = true;
    }

    updateSelection();
}

SP<CPrimarySelectionDevice> CPrimarySelectionProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::find_if(m_vDevices.begin(), m_vDevices.end(), [c](const auto& e) { return e->client() == c; });
    if (it == m_vDevices.end())
        return nullptr;
    return *it;
}
