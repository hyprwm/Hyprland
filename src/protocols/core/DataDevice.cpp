#include "DataDevice.hpp"
#include <algorithm>
#include "../../managers/SeatManager.hpp"
#include "../../managers/PointerManager.hpp"
#include "../../Compositor.hpp"
#include "Seat.hpp"
#include "Compositor.hpp"

CWLDataOfferResource::CWLDataOfferResource(SP<CWlDataOffer> resource_, SP<IDataSource> source_) : source(source_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CWlDataOffer* r) { PROTO::data->destroyResource(this); });
    resource->setOnDestroy([this](CWlDataOffer* r) { PROTO::data->destroyResource(this); });

    resource->setAccept([this](CWlDataOffer* r, uint32_t serial, const char* mime) {
        if (!source) {
            LOGM(WARN, "Possible bug: Accept on an offer w/o a source");
            return;
        }

        if (dead) {
            LOGM(WARN, "Possible bug: Accept on an offer that's dead");
            return;
        }

        LOGM(LOG, "Offer {:x} accepts data from source {:x} with mime {}", (uintptr_t)this, (uintptr_t)source.get(), mime ? mime : "null");

        source->accepted(mime ? mime : "");
        accepted = mime;
    });

    resource->setReceive([this](CWlDataOffer* r, const char* mime, uint32_t fd) {
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

        if (!accepted) {
            LOGM(WARN, "Offer was never accepted, sending accept first");
            source->accepted(mime ? mime : "");
        }

        source->send(mime ? mime : "", fd);

        recvd = true;

        // if (source->hasDnd())
        //     PROTO::data->completeDrag();
    });

    resource->setFinish([this](CWlDataOffer* r) {
        dead = true;
        if (!source || !recvd || !accepted)
            PROTO::data->abortDrag();
        else
            PROTO::data->completeDrag();
    });
}

bool CWLDataOfferResource::good() {
    return resource->resource();
}

void CWLDataOfferResource::sendData() {
    if (!source)
        return;

    if (resource->version() >= 3) {
        resource->sendSourceActions(7);
        resource->sendAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
    }

    for (auto& m : source->mimes()) {
        LOGM(LOG, " | offer {:x} supports mime {}", (uintptr_t)this, m);
        resource->sendOffer(m.c_str());
    }
}

CWLDataSourceResource::CWLDataSourceResource(SP<CWlDataSource> resource_, SP<CWLDataDeviceResource> device_) : device(device_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CWlDataSource* r) {
        events.destroy.emit();
        PROTO::data->onDestroyDataSource(self);
        PROTO::data->destroyResource(this);
    });
    resource->setOnDestroy([this](CWlDataSource* r) {
        events.destroy.emit();
        PROTO::data->onDestroyDataSource(self);
        PROTO::data->destroyResource(this);
    });

    resource->setOffer([this](CWlDataSource* r, const char* mime) { mimeTypes.push_back(mime); });
    resource->setSetActions([this](CWlDataSource* r, uint32_t a) {
        LOGM(LOG, "DataSource {:x} actions {}", (uintptr_t)this, a);
        actions = (wl_data_device_manager_dnd_action)a;
    });
}

CWLDataSourceResource::~CWLDataSourceResource() {
    events.destroy.emit();
    PROTO::data->onDestroyDataSource(self);
}

SP<CWLDataSourceResource> CWLDataSourceResource::fromResource(wl_resource* res) {
    auto data = (CWLDataSourceResource*)(((CWlDataSource*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CWLDataSourceResource::good() {
    return resource->resource();
}

void CWLDataSourceResource::accepted(const std::string& mime) {
    if (mime.empty()) {
        resource->sendTarget(nullptr);
        return;
    }

    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLDataSourceResource::sendAccepted with non-existent mime");
        return;
    }

    resource->sendTarget(mime.c_str());
}

std::vector<std::string> CWLDataSourceResource::mimes() {
    return mimeTypes;
}

void CWLDataSourceResource::send(const std::string& mime, uint32_t fd) {
    if (std::find(mimeTypes.begin(), mimeTypes.end(), mime) == mimeTypes.end()) {
        LOGM(ERR, "Compositor/App bug: CWLDataSourceResource::sendAskSend with non-existent mime");
        close(fd);
        return;
    }

    resource->sendSend(mime.c_str(), fd);
    close(fd);
}

void CWLDataSourceResource::cancelled() {
    resource->sendCancelled();
}

bool CWLDataSourceResource::hasDnd() {
    return dnd;
}

bool CWLDataSourceResource::dndDone() {
    return dndSuccess;
}

void CWLDataSourceResource::error(uint32_t code, const std::string& msg) {
    resource->error(code, msg);
}

void CWLDataSourceResource::sendDndDropPerformed() {
    if (resource->version() < 3)
        return;
    resource->sendDndDropPerformed();
}

void CWLDataSourceResource::sendDndFinished() {
    if (resource->version() < 3)
        return;
    resource->sendDndFinished();
}

void CWLDataSourceResource::sendDndAction(wl_data_device_manager_dnd_action a) {
    if (resource->version() < 3)
        return;
    resource->sendAction(a);
}

CWLDataDeviceResource::CWLDataDeviceResource(SP<CWlDataDevice> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setRelease([this](CWlDataDevice* r) { PROTO::data->destroyResource(this); });
    resource->setOnDestroy([this](CWlDataDevice* r) { PROTO::data->destroyResource(this); });

    pClient = resource->client();

    resource->setSetSelection([](CWlDataDevice* r, wl_resource* sourceR, uint32_t serial) {
        auto source = sourceR ? CWLDataSourceResource::fromResource(sourceR) : CSharedPointer<CWLDataSourceResource>{};
        if (!source) {
            LOGM(LOG, "Reset selection received");
            g_pSeatManager->setCurrentSelection(nullptr);
            return;
        }

        if (source && source->used)
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        g_pSeatManager->setCurrentSelection(source);
    });

    resource->setStartDrag([](CWlDataDevice* r, wl_resource* sourceR, wl_resource* origin, wl_resource* icon, uint32_t serial) {
        auto source = CWLDataSourceResource::fromResource(sourceR);
        if (!source) {
            LOGM(ERR, "No source in drag");
            return;
        }

        if (source && source->used)
            LOGM(WARN, "setSelection on a used resource. By protocol, this is a violation, but firefox et al insist on doing this.");

        source->markUsed();

        source->dnd = true;

        PROTO::data->initiateDrag(source, icon ? CWLSurfaceResource::fromResource(icon) : nullptr, CWLSurfaceResource::fromResource(origin));
    });
}

bool CWLDataDeviceResource::good() {
    return resource->resource();
}

wl_client* CWLDataDeviceResource::client() {
    return pClient;
}

void CWLDataDeviceResource::sendDataOffer(SP<CWLDataOfferResource> offer) {
    if (offer)
        resource->sendDataOffer(offer->resource.get());
    else
        resource->sendDataOfferRaw(nullptr);
}

void CWLDataDeviceResource::sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<CWLDataOfferResource> offer) {
    resource->sendEnterRaw(serial, surf->getResource()->resource(), wl_fixed_from_double(local.x), wl_fixed_from_double(local.y), offer->resource->resource());
}

void CWLDataDeviceResource::sendLeave() {
    resource->sendLeave();
}

void CWLDataDeviceResource::sendMotion(uint32_t timeMs, const Vector2D& local) {
    resource->sendMotion(timeMs, wl_fixed_from_double(local.x), wl_fixed_from_double(local.y));
}

void CWLDataDeviceResource::sendDrop() {
    resource->sendDrop();
}

void CWLDataDeviceResource::sendSelection(SP<CWLDataOfferResource> offer) {
    if (!offer)
        resource->sendSelectionRaw(nullptr);
    else
        resource->sendSelection(offer->resource.get());
}

CWLDataDeviceManagerResource::CWLDataDeviceManagerResource(SP<CWlDataDeviceManager> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlDataDeviceManager* r) { PROTO::data->destroyResource(this); });

    resource->setCreateDataSource([this](CWlDataDeviceManager* r, uint32_t id) {
        std::erase_if(sources, [](const auto& e) { return e.expired(); });

        const auto RESOURCE = PROTO::data->m_vSources.emplace_back(makeShared<CWLDataSourceResource>(makeShared<CWlDataSource>(r->client(), r->version(), id), device.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::data->m_vSources.pop_back();
            return;
        }

        if (!device)
            LOGM(WARN, "New data source before a device was created");

        RESOURCE->self = RESOURCE;

        sources.push_back(RESOURCE);

        LOGM(LOG, "New data source bound at {:x}", (uintptr_t)RESOURCE.get());
    });

    resource->setGetDataDevice([this](CWlDataDeviceManager* r, uint32_t id, wl_resource* seat) {
        const auto RESOURCE = PROTO::data->m_vDevices.emplace_back(makeShared<CWLDataDeviceResource>(makeShared<CWlDataDevice>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::data->m_vDevices.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        for (auto& s : sources) {
            if (!s)
                continue;
            s->device = RESOURCE;
        }

        LOGM(LOG, "New data device bound at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CWLDataDeviceManagerResource::good() {
    return resource->resource();
}

CWLDataDeviceProtocol::CWLDataDeviceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLDataDeviceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CWLDataDeviceManagerResource>(makeShared<CWlDataDeviceManager>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(LOG, "New datamgr resource bound at {:x}", (uintptr_t)RESOURCE.get());

    // we need to do it here because protocols come before seatMgr
    if (!listeners.onKeyboardFocusChange)
        listeners.onKeyboardFocusChange = g_pSeatManager->events.keyboardFocusChange.registerListener([this](std::any d) { this->onKeyboardFocus(); });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataDeviceManagerResource* seat) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == seat; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataDeviceResource* resource) {
    std::erase_if(m_vDevices, [&](const auto& other) { return other.get() == resource; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataSourceResource* resource) {
    std::erase_if(m_vSources, [&](const auto& other) { return other.get() == resource; });
}

void CWLDataDeviceProtocol::destroyResource(CWLDataOfferResource* resource) {
    std::erase_if(m_vOffers, [&](const auto& other) { return other.get() == resource; });
}

SP<CWLDataDeviceResource> CWLDataDeviceProtocol::dataDeviceForClient(wl_client* c) {
    auto it = std::find_if(m_vDevices.begin(), m_vDevices.end(), [c](const auto& e) { return e->client() == c; });
    if (it == m_vDevices.end())
        return nullptr;
    return *it;
}

void CWLDataDeviceProtocol::sendSelectionToDevice(SP<CWLDataDeviceResource> dev, SP<IDataSource> sel) {
    if (!sel) {
        dev->sendSelection(nullptr);
        return;
    }

    const auto OFFER = m_vOffers.emplace_back(makeShared<CWLDataOfferResource>(makeShared<CWlDataOffer>(dev->resource->client(), dev->resource->version(), 0), sel));

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

void CWLDataDeviceProtocol::onDestroyDataSource(WP<CWLDataSourceResource> source) {
    if (dnd.currentSource == source)
        abortDrag();
}

void CWLDataDeviceProtocol::setSelection(SP<IDataSource> source) {
    for (auto& o : m_vOffers) {
        if (o->source && o->source->hasDnd())
            continue;
        o->dead = true;
    }

    if (!source) {
        LOGM(LOG, "resetting selection");

        if (!g_pSeatManager->state.keyboardFocusResource)
            return;

        auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.keyboardFocusResource->client());
        if (DESTDEVICE)
            sendSelectionToDevice(DESTDEVICE, nullptr);

        return;
    }

    LOGM(LOG, "New selection for data source {:x}", (uintptr_t)source.get());

    if (!g_pSeatManager->state.keyboardFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.keyboardFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::setSelection: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, source);
}

void CWLDataDeviceProtocol::updateSelection() {
    if (!g_pSeatManager->state.keyboardFocusResource)
        return;

    auto DESTDEVICE = dataDeviceForClient(g_pSeatManager->state.keyboardFocusResource->client());

    if (!DESTDEVICE) {
        LOGM(LOG, "CWLDataDeviceProtocol::onKeyboardFocus: cannot send selection to a client without a data_device");
        return;
    }

    sendSelectionToDevice(DESTDEVICE, g_pSeatManager->selection.currentSelection.lock());
}

void CWLDataDeviceProtocol::onKeyboardFocus() {
    for (auto& o : m_vOffers) {
        o->dead = true;
    }

    updateSelection();
    updateDrag();
}

void CWLDataDeviceProtocol::initiateDrag(WP<CWLDataSourceResource> currentSource, SP<CWLSurfaceResource> dragSurface, SP<CWLSurfaceResource> origin) {

    if (dnd.currentSource) {
        LOGM(WARN, "New drag started while old drag still active??");
        abortDrag();
    }

    g_pInputManager->setCursorImageUntilUnset("grabbing");
    dnd.overriddenCursor = true;

    LOGM(LOG, "initiateDrag: source {:x}, surface: {:x}, origin: {:x}", (uintptr_t)currentSource.get(), (uintptr_t)dragSurface, (uintptr_t)origin);

    currentSource->used = true;

    dnd.currentSource = currentSource;
    dnd.originSurface = origin;
    dnd.dndSurface    = dragSurface;
    if (dragSurface) {
        dnd.dndSurfaceDestroy = dragSurface->events.destroy.registerListener([this](std::any d) { abortDrag(); });
        dnd.dndSurfaceCommit  = dragSurface->events.commit.registerListener([this](std::any d) {
            if (dnd.dndSurface->current.texture && !dnd.dndSurface->mapped) {
                dnd.dndSurface->map();
                return;
            }

            if (dnd.dndSurface->current.texture <= 0 && dnd.dndSurface->mapped) {
                dnd.dndSurface->unmap();
                return;
            }
        });
    }

    dnd.mouseButton = g_pHookSystem->hookDynamic("mouseButton", [this](void* self, SCallbackInfo& info, std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);
        if (E.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            LOGM(LOG, "Dropping drag on mouseUp");
            dropDrag();
        }
    });

    dnd.touchUp = g_pHookSystem->hookDynamic("touchUp", [this](void* self, SCallbackInfo& info, std::any e) {
        LOGM(LOG, "Dropping drag on touchUp");
        dropDrag();
    });

    dnd.mouseMove = g_pHookSystem->hookDynamic("mouseMove", [this](void* self, SCallbackInfo& info, std::any e) {
        auto V = std::any_cast<const Vector2D>(e);
        if (dnd.focusedDevice && g_pSeatManager->state.keyboardFocus) {
            auto surf = CWLSurface::fromResource(g_pSeatManager->state.keyboardFocus.lock());

            if (!surf)
                return;

            const auto box = surf->getSurfaceBoxGlobal();

            if (!box.has_value())
                return;

            dnd.focusedDevice->sendMotion(0 /* this is a hack */, V - box->pos());
            LOGM(LOG, "Drag motion {}", V - box->pos());
        }
    });

    dnd.touchMove = g_pHookSystem->hookDynamic("touchMove", [this](void* self, SCallbackInfo& info, std::any e) {
        auto E = std::any_cast<ITouch::SMotionEvent>(e);
        if (dnd.focusedDevice && g_pSeatManager->state.keyboardFocus) {
            auto surf = CWLSurface::fromResource(g_pSeatManager->state.keyboardFocus.lock());

            if (!surf)
                return;

            const auto box = surf->getSurfaceBoxGlobal();

            if (!box.has_value())
                return;

            dnd.focusedDevice->sendMotion(E.timeMs, E.pos);
            LOGM(LOG, "Drag motion {}", E.pos);
        }
    });

    // unfocus the pointer from the surface, this is part of """standard""" wayland procedure and gtk will freak out if this isn't happening.
    // BTW, the spec does NOT require this explicitly...
    // Fuck you gtk.
    g_pSeatManager->setPointerFocus(nullptr, {});

    // make a new offer, etc
    updateDrag();
}

void CWLDataDeviceProtocol::updateDrag() {
    if (!dndActive())
        return;

    if (dnd.focusedDevice)
        dnd.focusedDevice->sendLeave();

    if (!g_pSeatManager->state.keyboardFocusResource)
        return;

    dnd.focusedDevice = dataDeviceForClient(g_pSeatManager->state.keyboardFocusResource->client());

    if (!dnd.focusedDevice)
        return;

    // make a new offer
    const auto OFFER = m_vOffers.emplace_back(
        makeShared<CWLDataOfferResource>(makeShared<CWlDataOffer>(dnd.focusedDevice->resource->client(), dnd.focusedDevice->resource->version(), 0), dnd.currentSource.lock()));

    if (!OFFER->good()) {
        dnd.currentSource->resource->noMemory();
        m_vOffers.pop_back();
        return;
    }

    LOGM(LOG, "New dnd offer {:x} for data source {:x}", (uintptr_t)OFFER.get(), (uintptr_t)dnd.currentSource.get());

    dnd.focusedDevice->sendDataOffer(OFFER);
    OFFER->sendData();
    dnd.focusedDevice->sendEnter(wl_display_next_serial(g_pCompositor->m_sWLDisplay), g_pSeatManager->state.keyboardFocus.lock(),
                                 g_pSeatManager->state.keyboardFocus->current.size / 2.F, OFFER);
}

void CWLDataDeviceProtocol::resetDndState() {
    dnd.dndSurface.reset();
    dnd.dndSurfaceCommit.reset();
    dnd.dndSurfaceDestroy.reset();
    dnd.mouseButton.reset();
    dnd.mouseMove.reset();
    dnd.touchUp.reset();
    dnd.touchMove.reset();
}

void CWLDataDeviceProtocol::dropDrag() {
    if (!dnd.focusedDevice || !dnd.currentSource) {
        if (dnd.currentSource)
            abortDrag();
        return;
    }

    if (!wasDragSuccessful()) {
        abortDrag();
        return;
    }

    dnd.focusedDevice->sendDrop();
    dnd.focusedDevice->sendLeave();

    resetDndState();

    if (dnd.overriddenCursor)
        g_pInputManager->unsetCursorImage();
    dnd.overriddenCursor = false;
}

bool CWLDataDeviceProtocol::wasDragSuccessful() {
    if (!dnd.focusedDevice || !dnd.currentSource)
        return false;

    for (auto& o : m_vOffers) {
        if (o->dead || !o->source || !o->source->hasDnd())
            continue;

        if (o->recvd || o->accepted)
            return true;
    }

    return false;
}

void CWLDataDeviceProtocol::completeDrag() {
    resetDndState();

    if (!dnd.focusedDevice && !dnd.currentSource)
        return;

    if (dnd.currentSource) {
        dnd.currentSource->sendDndDropPerformed();
        dnd.currentSource->sendDndFinished();
    }

    dnd.focusedDevice.reset();
    dnd.currentSource.reset();

    g_pInputManager->simulateMouseMovement();
    g_pSeatManager->resendEnterEvents();
}

void CWLDataDeviceProtocol::abortDrag() {
    resetDndState();

    if (dnd.overriddenCursor)
        g_pInputManager->unsetCursorImage();
    dnd.overriddenCursor = false;

    if (!dnd.focusedDevice && !dnd.currentSource)
        return;

    if (dnd.focusedDevice)
        dnd.focusedDevice->sendLeave();
    if (dnd.currentSource)
        dnd.currentSource->cancelled();

    dnd.focusedDevice.reset();
    dnd.currentSource.reset();

    g_pInputManager->simulateMouseMovement();
    g_pSeatManager->resendEnterEvents();
}

void CWLDataDeviceProtocol::renderDND(CMonitor* pMonitor, timespec* when) {
    if (!dnd.dndSurface || !dnd.dndSurface->current.texture)
        return;

    const auto POS = g_pInputManager->getMouseCoordsInternal();

    CBox       box = CBox{POS, dnd.dndSurface->current.size}.translate(-pMonitor->vecPosition + g_pPointerManager->cursorSizeLogical() / 2.F).scale(pMonitor->scale);
    g_pHyprOpenGL->renderTexture(dnd.dndSurface->current.texture, &box, 1.F);

    box = CBox{POS, dnd.dndSurface->current.size}.translate(g_pPointerManager->cursorSizeLogical() / 2.F);
    g_pHyprRenderer->damageBox(&box);

    dnd.dndSurface->frame(when);
}

bool CWLDataDeviceProtocol::dndActive() {
    return dnd.currentSource && dnd.mouseButton /* test a member of the state to ensure it's also present */;
}
