#include "PrimarySelection.hpp"
#include <algorithm>
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "../config/ConfigValue.hpp"
using namespace Hyprutils::OS;

CPrimarySelectionOffer::CPrimarySelectionOffer(SP<CZwpPrimarySelectionOfferV1> resource_, SP<IDataSource> source_) : m_source(source_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpPrimarySelectionOfferV1* r) { PROTO::primarySelection->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpPrimarySelectionOfferV1* r) { PROTO::primarySelection->destroyResource(this); });

    m_resource->setReceive([this](CZwpPrimarySelectionOfferV1* r, const char* mime, int32_t fd) {
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

bool CPrimarySelectionOffer::good() {
    return m_resource->resource();
}

void CPrimarySelectionOffer::sendData() {
    if UNLIKELY (!m_source)
        return;

    for (auto const& m : m_source->mimes()) {
        m_resource->sendOffer(m.c_str());
    }
}

CPrimarySelectionSource::CPrimarySelectionSource(SP<CZwpPrimarySelectionSourceV1> resource_, SP<CPrimarySelectionDevice> device_) : m_device(device_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CZwpPrimarySelectionSourceV1* r) {
        m_events.destroy.emit();
        PROTO::primarySelection->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CZwpPrimarySelectionSourceV1* r) {
        m_events.destroy.emit();
        PROTO::primarySelection->destroyResource(this);
    });

    m_resource->setOffer([this](CZwpPrimarySelectionSourceV1* r, const char* mime) { m_mimeTypes.emplace_back(mime); });
}

CPrimarySelectionSource::~CPrimarySelectionSource() {
    m_events.destroy.emit();
}

SP<CPrimarySelectionSource> CPrimarySelectionSource::fromResource(wl_resource* res) {
    auto data = (CPrimarySelectionSource*)(((CZwpPrimarySelectionSourceV1*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CPrimarySelectionSource::good() {
    return m_resource->resource();
}

std::vector<std::string> CPrimarySelectionSource::mimes() {
    return m_mimeTypes;
}

void CPrimarySelectionSource::send(const std::string& mime, CFileDescriptor fd) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CPrimarySelectionSource::sendAskSend with non-existent mime");
        return;
    }

    m_resource->sendSend(mime.c_str(), fd.get());
}

void CPrimarySelectionSource::accepted(const std::string& mime) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end())
        LOGM(ERR, "Compositor/App bug: CPrimarySelectionSource::sendAccepted with non-existent mime");

    // primary sel has no accepted
}

void CPrimarySelectionSource::cancelled() {
    m_resource->sendCancelled();
}

void CPrimarySelectionSource::error(uint32_t code, const std::string& msg) {
    m_resource->error(code, msg);
}

CPrimarySelectionDevice::CPrimarySelectionDevice(SP<CZwpPrimarySelectionDeviceV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CZwpPrimarySelectionDeviceV1* r) { PROTO::primarySelection->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpPrimarySelectionDeviceV1* r) { PROTO::primarySelection->destroyResource(this); });

    m_resource->setSetSelection([](CZwpPrimarySelectionDeviceV1* r, wl_resource* sourceR, uint32_t serial) {
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
    return m_resource->resource();
}

wl_client* CPrimarySelectionDevice::client() {
    return m_client;
}

void CPrimarySelectionDevice::sendDataOffer(SP<CPrimarySelectionOffer> offer) {
    m_resource->sendDataOffer(offer->m_resource.get());
}

void CPrimarySelectionDevice::sendSelection(SP<CPrimarySelectionOffer> selection) {
    if (!selection)
        m_resource->sendSelectionRaw(nullptr);
    else
        m_resource->sendSelection(selection->m_resource.get());
}

CPrimarySelectionManager::CPrimarySelectionManager(SP<CZwpPrimarySelectionDeviceManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwpPrimarySelectionDeviceManagerV1* r) { PROTO::primarySelection->destroyResource(this); });

    m_resource->setGetDevice([this](CZwpPrimarySelectionDeviceManagerV1* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE =
            PROTO::primarySelection->m_devices.emplace_back(makeShared<CPrimarySelectionDevice>(makeShared<CZwpPrimarySelectionDeviceV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::primarySelection->m_devices.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
        m_device         = RESOURCE;

        for (auto const& s : m_sources) {
            if (!s)
                continue;
            s->m_device = RESOURCE;
        }

        LOGM(LOG, "New primary selection data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setCreateSource([this](CZwpPrimarySelectionDeviceManagerV1* r, uint32_t id) {
        std::erase_if(m_sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE = PROTO::primarySelection->m_sources.emplace_back(
            makeShared<CPrimarySelectionSource>(makeShared<CZwpPrimarySelectionSourceV1>(r->client(), r->version(), id), m_device.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::primarySelection->m_sources.pop_back();
            return;
        }

        if (!m_device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->m_self = RESOURCE;

        m_sources.emplace_back(RESOURCE);

        LOGM(LOG, "New primary selection data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CPrimarySelectionManager::good() {
    return m_resource->resource();
}

CPrimarySelectionProtocol::CPrimarySelectionProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPrimarySelectionProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CPrimarySelectionManager>(makeShared<CZwpPrimarySelectionDeviceManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(LOG, "New primary_seletion_manager at {:x}", (uintptr_t)RESOURCE.get());

    // we need to do it here because protocols come before seatMgr
    if (!m_listeners.onPointerFocusChange)
        m_listeners.onPointerFocusChange = g_pSeatManager->m_events.pointerFocusChange.registerListener([this](std::any d) { this->onPointerFocus(); });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionManager* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionSource* resource) {
    std::erase_if(m_sources, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionDevice* resource) {
    std::erase_if(m_devices, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::destroyResource(CPrimarySelectionOffer* resource) {
    std::erase_if(m_offers, [&](const auto& other) { return other.get() == resource; });
}

void CPrimarySelectionProtocol::sendSelectionToDevice(SP<CPrimarySelectionDevice> dev, SP<IDataSource> sel) {
    if (!sel) {
        dev->sendSelection(nullptr);
        return;
    }

    const auto OFFER =
        m_offers.emplace_back(makeShared<CPrimarySelectionOffer>(makeShared<CZwpPrimarySelectionOfferV1>(dev->m_resource->client(), dev->m_resource->version(), 0), sel));

    if (!OFFER->good()) {
        dev->m_resource->noMemory();
        m_offers.pop_back();
        return;
    }

    LOGM(LOG, "New offer {:x} for data source {:x}", (uintptr_t)OFFER.get(), (uintptr_t)sel.get());

    dev->sendDataOffer(OFFER);
    OFFER->sendData();
    dev->sendSelection(OFFER);
}

void CPrimarySelectionProtocol::setSelection(SP<IDataSource> source) {
    for (auto const& o : m_offers) {
        if (o->m_source && o->m_source->hasDnd())
            continue;
        o->m_dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting selection");

        if (!g_pSeatManager->m_state.pointerFocusResource)
            return;

        auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.pointerFocusResource->client());
        if (DESTDEVICE)
            sendSelectionToDevice(DESTDEVICE, nullptr);

        return;
    }

    LOGM(LOG, "New selection for data source {:x}", (uintptr_t)source.get());

    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.pointerFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::setSelection: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, source);
}

void CPrimarySelectionProtocol::updateSelection() {
    if (!g_pSeatManager->m_state.pointerFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.pointerFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CPrimarySelectionProtocol::updateSelection: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, g_pSeatManager->m_selection.currentPrimarySelection.lock());
}

void CPrimarySelectionProtocol::onPointerFocus() {
    for (auto const& o : m_offers) {
        o->m_dead = true;
    }

    updateSelection();
}

SP<CPrimarySelectionDevice> CPrimarySelectionProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::ranges::find_if(m_devices, [c](const auto& e) { return e->client() == c; });
    if (it == m_devices.end())
        return nullptr;
    return *it;
}
