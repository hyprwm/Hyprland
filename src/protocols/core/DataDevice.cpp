#include "DataDevice.hpp"
#include <algorithm>
#include "../../managers/SeatManager.hpp"
#include "../../managers/PointerManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../Compositor.hpp"
#include "../../render/pass/TexPassElement.hpp"
#include "Seat.hpp"
#include "Compositor.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../xwayland/Server.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../render/Renderer.hpp"
#include "../../xwayland/Dnd.hpp"
using namespace Hyprutils::OS;

CWLDataOfferResource::CWLDataOfferResource(SP<CWlDataOffer> resource_, SP<IDataSource> source_) : m_source(source_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWlDataOffer* r) { PROTO::data->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlDataOffer* r) { PROTO::data->destroyResource(this); });

    m_resource->setAccept([this](CWlDataOffer* r, uint32_t serial, const char* mime) {
        if (!m_source) {
            LOGM(WARN, "Possible bug: Accept on an offer w/o a source");
            return;
        }

        if (m_dead) {
            LOGM(WARN, "Possible bug: Accept on an offer that's dead");
            return;
        }

        LOGM(LOG, "Offer {:x} accepts data from source {:x} with mime {}", (uintptr_t)this, (uintptr_t)m_source.get(), mime ? mime : "null");

        m_source->accepted(mime ? mime : "");
        m_accepted = mime;
    });

    m_resource->setReceive([this](CWlDataOffer* r, const char* mime, int fd) {
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

        if (!m_accepted) {
            LOGM(WARN, "Offer was never accepted, sending accept first");
            m_source->accepted(mime ? mime : "");
        }

        m_source->send(mime ? mime : "", std::move(sendFd));

        m_recvd = true;

        // if (source->hasDnd())
        //     PROTO::data->completeDrag();
    });

    m_resource->setFinish([this](CWlDataOffer* r) {
        m_dead = true;
        if (!m_source || !m_recvd || !m_accepted)
            PROTO::data->abortDrag();
        else
            PROTO::data->completeDrag();
    });
}

CWLDataOfferResource::~CWLDataOfferResource() {
    if (!m_source || !m_source->hasDnd() || m_dead)
        return;

    m_source->sendDndFinished();
}

bool CWLDataOfferResource::good() {
    return m_resource->resource();
}

void CWLDataOfferResource::sendData() {
    if (!m_source)
        return;

    const auto SOURCEACTIONS = m_source->actions();

    if (m_resource->version() >= 3 && SOURCEACTIONS > 0) {
        m_resource->sendSourceActions(SOURCEACTIONS);
        if (SOURCEACTIONS & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
            m_resource->sendAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
        else if (SOURCEACTIONS & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
            m_resource->sendAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
        else {
            LOGM(ERR, "Client bug? dnd source has no action move or copy. Sending move, f this.");
            m_resource->sendAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
        }
    }

    for (auto const& m : m_source->mimes()) {
        LOGM(LOG, " | offer {:x} supports mime {}", (uintptr_t)this, m);
        m_resource->sendOffer(m.c_str());
    }
}

eDataSourceType CWLDataOfferResource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

SP<CWLDataOfferResource> CWLDataOfferResource::getWayland() {
    return m_self.lock();
}

SP<CX11DataOffer> CWLDataOfferResource::getX11() {
    return nullptr;
}

SP<IDataSource> CWLDataOfferResource::getSource() {
    return m_source.lock();
}

CWLDataSourceResource::CWLDataSourceResource(SP<CWlDataSource> resource_, SP<CWLDataDeviceResource> device_) : m_device(device_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CWlDataSource* r) {
        m_events.destroy.emit();
        PROTO::data->onDestroyDataSource(m_self);
        PROTO::data->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CWlDataSource* r) {
        m_events.destroy.emit();
        PROTO::data->onDestroyDataSource(m_self);
        PROTO::data->destroyResource(this);
    });

    m_resource->setOffer([this](CWlDataSource* r, const char* mime) { m_mimeTypes.emplace_back(mime); });
    m_resource->setSetActions([this](CWlDataSource* r, uint32_t a) {
        LOGM(LOG, "DataSource {:x} actions {}", (uintptr_t)this, a);
        m_supportedActions = a;
    });
}

CWLDataSourceResource::~CWLDataSourceResource() {
    m_events.destroy.emit();
    PROTO::data->onDestroyDataSource(m_self);
}

SP<CWLDataSourceResource> CWLDataSourceResource::fromResource(wl_resource* res) {
    auto data = (CWLDataSourceResource*)(((CWlDataSource*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CWLDataSourceResource::good() {
    return m_resource->resource();
}

void CWLDataSourceResource::accepted(const std::string& mime) {
    if (mime.empty()) {
        m_resource->sendTarget(nullptr);
        return;
    }

    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLDataSourceResource::sendAccepted with non-existent mime");
        return;
    }

    m_resource->sendTarget(mime.c_str());
}

std::vector<std::string> CWLDataSourceResource::mimes() {
    return m_mimeTypes;
}

void CWLDataSourceResource::send(const std::string& mime, CFileDescriptor fd) {
    if (std::ranges::find(m_mimeTypes, mime) == m_mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLDataSourceResource::sendAskSend with non-existent mime");
        return;
    }

    m_resource->sendSend(mime.c_str(), fd.get());
}

void CWLDataSourceResource::cancelled() {
    m_resource->sendCancelled();
}

bool CWLDataSourceResource::hasDnd() {
    return m_dnd;
}

bool CWLDataSourceResource::dndDone() {
    return m_dndSuccess;
}

void CWLDataSourceResource::error(uint32_t code, const std::string& msg) {
    m_resource->error(code, msg);
}

void CWLDataSourceResource::sendDndDropPerformed() {
    if (m_resource->version() < 3)
        return;
    m_resource->sendDndDropPerformed();
    m_dropped = true;
}

void CWLDataSourceResource::sendDndFinished() {
    if (m_resource->version() < 3)
        return;
    m_resource->sendDndFinished();
}

void CWLDataSourceResource::sendDndAction(wl_data_device_manager_dnd_action a) {
    if (m_resource->version() < 3)
        return;
    m_resource->sendAction(a);
}

uint32_t CWLDataSourceResource::actions() {
    return m_supportedActions;
}

eDataSourceType CWLDataSourceResource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

CWLDataDeviceResource::CWLDataDeviceResource(SP<CWlDataDevice> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](CWlDataDevice* r) { PROTO::data->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlDataDevice* r) { PROTO::data->destroyResource(this); });

    m_client = m_resource->client();

    m_resource->setSetSelection([](CWlDataDevice* r, wl_resource* sourceR, uint32_t serial) {
        auto source = sourceR ? CWLDataSourceResource::fromResource(sourceR) : CSharedPointer<CWLDataSourceResource>{};
        if (!source) {
            LOGM(LOG, "Reset selection received");
            g_pSeatManager->setCurrentSelection(nullptr);
            return;
        }

        if (source && source->m_used)
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        g_pSeatManager->setCurrentSelection(source);
    });

    m_resource->setStartDrag([](CWlDataDevice* r, wl_resource* sourceR, wl_resource* origin, wl_resource* icon, uint32_t serial) {
        auto source = CWLDataSourceResource::fromResource(sourceR);
        if (!source) {
            LOGM(ERR, "No source in drag");
            return;
        }

        if (source && source->m_used)
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        source->m_dnd = true;

        PROTO::data->initiateDrag(source, icon ? CWLSurfaceResource::fromResource(icon) : nullptr, CWLSurfaceResource::fromResource(origin));
    });
}

bool CWLDataDeviceResource::good() {
    return m_resource->resource();
}

wl_client* CWLDataDeviceResource::client() {
    return m_client;
}

void CWLDataDeviceResource::sendDataOffer(SP<IDataOffer> offer) {
    if (!offer)
        m_resource->sendDataOfferRaw(nullptr);
    else if (const auto WL = offer->getWayland(); WL)
        m_resource->sendDataOffer(WL->m_resource.get());
    //FIXME: X11
}

void CWLDataDeviceResource::sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer) {
    if (const auto WL = offer->getWayland(); WL)
        m_resource->sendEnterRaw(serial, surf->getResource()->resource(), wl_fixed_from_double(local.x), wl_fixed_from_double(local.y), WL->m_resource->resource());
    // FIXME: X11
}

void CWLDataDeviceResource::sendLeave() {
    m_resource->sendLeave();
}

void CWLDataDeviceResource::sendMotion(uint32_t timeMs, const Vector2D& local) {
    m_resource->sendMotion(timeMs, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLDataDeviceResource::sendDrop() {
    m_resource->sendDrop();
}

void CWLDataDeviceResource::sendSelection(SP<IDataOffer> offer) {
    if (!offer)
        m_resource->sendSelectionRaw(nullptr);
    else if (const auto WL = offer->getWayland(); WL)
        m_resource->sendSelection(WL->m_resource.get());
}

eDataSourceType CWLDataDeviceResource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

SP<CWLDataDeviceResource> CWLDataDeviceResource::getWayland() {
    return m_self.lock();
}

SP<CX11DataDevice> CWLDataDeviceResource::getX11() {
    return nullptr;
}

CWLDataDeviceManagerResource::CWLDataDeviceManagerResource(SP<CWlDataDeviceManager> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlDataDeviceManager* r) { PROTO::data->destroyResource(this); });

    m_resource->setCreateDataSource([this](CWlDataDeviceManager* r, uint32_t id) {
        std::erase_if(m_sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE = PROTO::data->m_sources.emplace_back(makeShared<CWLDataSourceResource>(makeShared<CWlDataSource>(r->client(), r->version(), id), m_device.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::data->m_sources.pop_back();
            return;
        }

        if (!m_device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->m_self = RESOURCE;

        m_sources.emplace_back(RESOURCE);

        LOGM(LOG, "New data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setGetDataDevice([this](CWlDataDeviceManager* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE = PROTO::data->m_devices.emplace_back(makeShared<CWLDataDeviceResource>(makeShared<CWlDataDevice>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::data->m_devices.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        for (auto const& s : m_sources) {
            if (!s)
                continue;
            s->m_device = RESOURCE;
        }

        LOGM(LOG, "New data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CWLDataDeviceManagerResource::good() {
    return m_resource->resource();
}

CWLDataDeviceProtocol::CWLDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    g_pEventLoopManager->doLater([this]() {
        m_listeners.onKeyboardFocusChange   = g_pSeatManager->m_events.keyboardFocusChange.registerListener([this](std::any d) { onKeyboardFocus(); });
        m_listeners.onDndPointerFocusChange = g_pSeatManager->m_events.dndPointerFocusChange.registerListener([this](std::any d) { onDndPointerFocus(); });
    });
}

void CWLDataDeviceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CWLDataDeviceManagerResource>(makeShared<CWlDataDeviceManager>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(LOG, "New datamgr resource bound at {:x}", (uintptr_t)RESOURCE.get());
}

void CWLDataDeviceProtocol::destroyResource(CWLDataDeviceManagerResource* seat) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == seat; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataDeviceResource* resource) {
    std::erase_if(m_devices, [&](const auto& other) { return other.get() == resource; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataSourceResource* resource) {
    std::erase_if(m_sources, [&](const auto& other) { return other.get() == resource; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataOfferResource* resource) {
    std::erase_if(m_offers, [&](const auto& other) { return other.get() == resource; });
}

SP<IDataDevice> CWLDataDeviceProtocol::dataDeviceForClient(wl_client* c) {
#ifndef NO_XWAYLAND
    if (g_pXWayland && g_pXWayland->m_server && c == g_pXWayland->m_server->m_xwaylandClient)
        return g_pXWayland->m_wm->getDataDevice();
#endif

    auto it = std::ranges::find_if(m_devices, [c](const auto& e) { return e->client() == c; });
    if (it == m_devices.end())
        return nullptr;
    return *it;
}

void CWLDataDeviceProtocol::sendSelectionToDevice(SP<IDataDevice> dev, SP<IDataSource> sel) {
    if (!sel) {
        dev->sendSelection(nullptr);
        return;
    }

    SP<IDataOffer> offer;

    if (const auto WL = dev->getWayland(); WL) {
        const auto OFFER = m_offers.emplace_back(makeShared<CWLDataOfferResource>(makeShared<CWlDataOffer>(WL->m_resource->client(), WL->m_resource->version(), 0), sel));
        if UNLIKELY (!OFFER->good()) {
            WL->m_resource->noMemory();
            m_offers.pop_back();
            return;
        }
        OFFER->m_source = sel;
        OFFER->m_self   = OFFER;
        offer           = OFFER;
    }
#ifndef NO_XWAYLAND
    else if (const auto X11 = dev->getX11(); X11)
        offer = g_pXWayland->m_wm->createX11DataOffer(g_pSeatManager->m_state.keyboardFocus.lock(), sel);
#endif

    if UNLIKELY (!offer) {
        LOGM(ERR, "No offer could be created in sendSelectionToDevice");
        return;
    }

    LOGM(LOG, "New {} offer {:x} for data source {:x}", offer->type() == DATA_SOURCE_TYPE_WAYLAND ? "wayland" : "X11", (uintptr_t)offer.get(), (uintptr_t)sel.get());

    dev->sendDataOffer(offer);
    if (const auto WL = offer->getWayland(); WL)
        WL->sendData();
    dev->sendSelection(offer);
}

void CWLDataDeviceProtocol::onDestroyDataSource(WP<CWLDataSourceResource> source) {
    if (m_dnd.currentSource == source)
        abortDrag();
}

void CWLDataDeviceProtocol::setSelection(SP<IDataSource> source) {
    for (auto const& o : m_offers) {
        if (o->m_source && o->m_source->hasDnd())
            continue;
        o->m_dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting selection");

        if (!g_pSeatManager->m_state.keyboardFocusResource)
            return;

        auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.keyboardFocusResource->client());
        if (DESTDEVICE && DESTDEVICE->type() == DATA_SOURCE_TYPE_WAYLAND)
            sendSelectionToDevice(DESTDEVICE, nullptr);

        return;
    }

    LOGM(LOG, "New selection for data source {:x}", (uintptr_t)source.get());

    if (!g_pSeatManager->m_state.keyboardFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.keyboardFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::setSelection: cannot send selection to a client without a data_device");
        return;
    }

    if (DESTDEVICE->type() != DATA_SOURCE_TYPE_WAYLAND) {
        LOGM(LOG, "CWLDataDeviceProtocol::setSelection: ignoring X11 data device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, source);
}

void CWLDataDeviceProtocol::updateSelection() {
    if (!g_pSeatManager->m_state.keyboardFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->m_state.keyboardFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::onKeyboardFocus: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, g_pSeatManager->m_selection.currentSelection.lock());
}

void CWLDataDeviceProtocol::onKeyboardFocus() {
    for (auto const& o : m_offers) {
        if (o->m_source && o->m_source->hasDnd())
            continue;
        o->m_dead = true;
    }

    updateSelection();
}

void CWLDataDeviceProtocol::onDndPointerFocus() {
    for (auto const& o : m_offers) {
        if (o->m_source && !o->m_source->hasDnd())
            continue;
        o->m_dead = true;
    }

    updateDrag();
}

void CWLDataDeviceProtocol::initiateDrag(WP<CWLDataSourceResource> currentSource, SP<CWLSurfaceResource> dragSurface, SP<CWLSurfaceResource> origin) {

    if (m_dnd.currentSource) {
        LOGM(WARN, "New drag started while old drag still active??");
        abortDrag();
    }

    g_pInputManager->setCursorImageUntilUnset("grabbing");
    m_dnd.overriddenCursor = true;

    LOGM(LOG, "initiateDrag: source {:x}, surface: {:x}, origin: {:x}", (uintptr_t)currentSource.get(), (uintptr_t)dragSurface, (uintptr_t)origin);

    currentSource->m_used = true;

    m_dnd.currentSource = currentSource;
    m_dnd.originSurface = origin;
    m_dnd.dndSurface    = dragSurface;
    if (dragSurface) {
        m_dnd.dndSurfaceDestroy = dragSurface->m_events.destroy.registerListener([this](std::any d) { abortDrag(); });
        m_dnd.dndSurfaceCommit  = dragSurface->m_events.commit.registerListener([this](std::any d) {
            if (m_dnd.dndSurface->m_current.texture && !m_dnd.dndSurface->m_mapped) {
                m_dnd.dndSurface->map();
                return;
            }

            if (m_dnd.dndSurface->m_current.texture <= 0 && m_dnd.dndSurface->m_mapped) {
                m_dnd.dndSurface->unmap();
                return;
            }
        });
    }

    m_dnd.mouseButton = g_pHookSystem->hookDynamic("mouseButton", [this](void* self, SCallbackInfo& info, std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);
        if (E.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            LOGM(LOG, "Dropping drag on mouseUp");
            dropDrag();
        }
    });

    m_dnd.touchUp = g_pHookSystem->hookDynamic("touchUp", [this](void* self, SCallbackInfo& info, std::any e) {
        LOGM(LOG, "Dropping drag on touchUp");
        dropDrag();
    });

    m_dnd.mouseMove = g_pHookSystem->hookDynamic("mouseMove", [this](void* self, SCallbackInfo& info, std::any e) {
        auto V = std::any_cast<const Vector2D>(e);
        if (m_dnd.focusedDevice && g_pSeatManager->m_state.dndPointerFocus) {
            auto surf = CWLSurface::fromResource(g_pSeatManager->m_state.dndPointerFocus.lock());

            if (!surf)
                return;

            const auto box = surf->getSurfaceBoxGlobal();

            if (!box.has_value())
                return;

            m_dnd.focusedDevice->sendMotion(Time::millis(Time::steadyNow()), V - box->pos());
            LOGM(LOG, "Drag motion {}", V - box->pos());
        }
    });

    m_dnd.touchMove = g_pHookSystem->hookDynamic("touchMove", [this](void* self, SCallbackInfo& info, std::any e) {
        auto E = std::any_cast<ITouch::SMotionEvent>(e);
        if (m_dnd.focusedDevice && g_pSeatManager->m_state.dndPointerFocus) {
            auto surf = CWLSurface::fromResource(g_pSeatManager->m_state.dndPointerFocus.lock());

            if (!surf)
                return;

            const auto box = surf->getSurfaceBoxGlobal();

            if (!box.has_value())
                return;

            m_dnd.focusedDevice->sendMotion(E.timeMs, E.pos);
            LOGM(LOG, "Drag motion {}", E.pos);
        }
    });

    // unfocus the pointer from the surface, this is part of """standard""" wayland procedure and gtk will freak out if this isn't happening.
    // BTW, the spec does NOT require this explicitly...
    // Fuck you gtk.
    const auto LASTDNDFOCUS = g_pSeatManager->m_state.dndPointerFocus;
    g_pSeatManager->setPointerFocus(nullptr, {});
    g_pSeatManager->m_state.dndPointerFocus = LASTDNDFOCUS;

    // make a new offer, etc
    updateDrag();
}

void CWLDataDeviceProtocol::updateDrag() {
    if (!dndActive())
        return;

    if (m_dnd.focusedDevice)
        m_dnd.focusedDevice->sendLeave();

    if (!g_pSeatManager->m_state.dndPointerFocus)
        return;

    m_dnd.focusedDevice = dataDeviceForClient(g_pSeatManager->m_state.dndPointerFocus->client());

    if (!m_dnd.focusedDevice)
        return;

    SP<IDataOffer> offer;

    if (const auto WL = m_dnd.focusedDevice->getWayland(); WL) {
        const auto OFFER =
            m_offers.emplace_back(makeShared<CWLDataOfferResource>(makeShared<CWlDataOffer>(WL->m_resource->client(), WL->m_resource->version(), 0), m_dnd.currentSource.lock()));
        if (!OFFER->good()) {
            WL->m_resource->noMemory();
            m_offers.pop_back();
            return;
        }
        OFFER->m_source = m_dnd.currentSource;
        OFFER->m_self   = OFFER;
        offer           = OFFER;
    }
#ifndef NO_XWAYLAND
    else if (const auto X11 = m_dnd.focusedDevice->getX11(); X11)
        offer = g_pXWayland->m_wm->createX11DataOffer(g_pSeatManager->m_state.keyboardFocus.lock(), m_dnd.currentSource.lock());
#endif

    if (!offer) {
        LOGM(ERR, "No offer could be created in updateDrag");
        return;
    }

    LOGM(LOG, "New {} dnd offer {:x} for data source {:x}", offer->type() == DATA_SOURCE_TYPE_WAYLAND ? "wayland" : "X11", (uintptr_t)offer.get(),
         (uintptr_t)m_dnd.currentSource.get());

    m_dnd.focusedDevice->sendDataOffer(offer);
    if (const auto WL = offer->getWayland(); WL)
        WL->sendData();
    m_dnd.focusedDevice->sendEnter(wl_display_next_serial(g_pCompositor->m_wlDisplay), g_pSeatManager->m_state.dndPointerFocus.lock(),
                                   g_pSeatManager->m_state.dndPointerFocus->m_current.size / 2.F, offer);
}

void CWLDataDeviceProtocol::cleanupDndState(bool resetDevice, bool resetSource, bool simulateInput) {
    m_dnd.dndSurface.reset();
    m_dnd.dndSurfaceCommit.reset();
    m_dnd.dndSurfaceDestroy.reset();
    m_dnd.mouseButton.reset();
    m_dnd.mouseMove.reset();
    m_dnd.touchUp.reset();
    m_dnd.touchMove.reset();

    if (resetDevice)
        m_dnd.focusedDevice.reset();
    if (resetSource)
        m_dnd.currentSource.reset();

    if (simulateInput) {
        g_pInputManager->simulateMouseMovement();
        g_pSeatManager->resendEnterEvents();
    }
}

void CWLDataDeviceProtocol::dropDrag() {
    if (!m_dnd.focusedDevice || !m_dnd.currentSource) {
        if (m_dnd.currentSource)
            abortDrag();
        return;
    }

    if (!wasDragSuccessful()) {
        abortDrag();
        return;
    }

    m_dnd.focusedDevice->sendDrop();

#ifndef NO_XWAYLAND
    if (m_dnd.focusedDevice->getX11()) {
        m_dnd.focusedDevice->sendLeave();
        if (m_dnd.overriddenCursor)
            g_pInputManager->unsetCursorImage();
        m_dnd.overriddenCursor = false;
        cleanupDndState(true, true, true);
        return;
    }
#endif

    m_dnd.focusedDevice->sendLeave();
    if (m_dnd.overriddenCursor)
        g_pInputManager->unsetCursorImage();
    m_dnd.overriddenCursor = false;
    cleanupDndState(false, false, false);
}

bool CWLDataDeviceProtocol::wasDragSuccessful() {
    if (!m_dnd.currentSource)
        return false;

    for (auto const& o : m_offers) {
        if (o->m_dead || o->m_source != m_dnd.currentSource)
            continue;

        if (o->m_recvd || o->m_accepted)
            return true;
    }

#ifndef NO_XWAYLAND
    if (m_dnd.focusedDevice->getX11())
        return true;
#endif

    return false;
}

void CWLDataDeviceProtocol::completeDrag() {
    if (!m_dnd.focusedDevice && !m_dnd.currentSource)
        return;

    if (m_dnd.currentSource) {
        m_dnd.currentSource->sendDndDropPerformed();
        m_dnd.currentSource->sendDndFinished();
    }

    cleanupDndState(true, true, true);
}

void CWLDataDeviceProtocol::abortDrag() {
    cleanupDndState(false, false, false);

    if (m_dnd.overriddenCursor)
        g_pInputManager->unsetCursorImage();
    m_dnd.overriddenCursor = false;

    if (!m_dnd.focusedDevice && !m_dnd.currentSource)
        return;

    if (m_dnd.focusedDevice) {
#ifndef NO_XWAYLAND
        if (auto x11Device = m_dnd.focusedDevice->getX11(); x11Device)
            x11Device->forceCleanupDnd();
#endif
        m_dnd.focusedDevice->sendLeave();
    }

    if (m_dnd.currentSource)
        m_dnd.currentSource->cancelled();

    m_dnd.focusedDevice.reset();
    m_dnd.currentSource.reset();

    g_pInputManager->simulateMouseMovement();
    g_pSeatManager->resendEnterEvents();
}

void CWLDataDeviceProtocol::renderDND(PHLMONITOR pMonitor, const Time::steady_tp& when) {
    if (!m_dnd.dndSurface || !m_dnd.dndSurface->m_current.texture)
        return;

    const auto POS = g_pInputManager->getMouseCoordsInternal();

    Vector2D   surfacePos = POS;

    surfacePos += m_dnd.dndSurface->m_current.offset;

    CBox                         box = CBox{surfacePos, m_dnd.dndSurface->m_current.size}.translate(-pMonitor->m_position).scale(pMonitor->m_scale);

    CTexPassElement::SRenderData data;
    data.tex = m_dnd.dndSurface->m_current.texture;
    data.box = box;
    g_pHyprRenderer->m_renderPass.add(makeShared<CTexPassElement>(std::move(data)));

    CBox damageBox = CBox{surfacePos, m_dnd.dndSurface->m_current.size}.expand(5);
    g_pHyprRenderer->damageBox(damageBox);

    m_dnd.dndSurface->frame(when);
}

bool CWLDataDeviceProtocol::dndActive() {
    return m_dnd.currentSource;
}

void CWLDataDeviceProtocol::abortDndIfPresent() {
    if (!dndActive())
        return;
    abortDrag();
}
