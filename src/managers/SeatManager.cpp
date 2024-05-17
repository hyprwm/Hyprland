#include "SeatManager.hpp"
#include "../protocols/core/Seat.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/DataDeviceWlr.hpp"
#include "../protocols/PrimarySelection.hpp"
#include "../Compositor.hpp"
#include "../devices/IKeyboard.hpp"
#include <algorithm>
#include <ranges>

CSeatManager::CSeatManager() {
    listeners.newSeatResource = PROTO::seat->events.newSeatResource.registerListener([this](std::any res) { onNewSeatResource(std::any_cast<SP<CWLSeatResource>>(res)); });
}

CSeatManager::SSeatResourceContainer::SSeatResourceContainer(SP<CWLSeatResource> res) {
    resource          = res;
    listeners.destroy = res->events.destroy.registerListener(
        [this](std::any data) { std::erase_if(g_pSeatManager->seatResources, [this](const auto& e) { return e->resource.expired() || e->resource == resource; }); });
}

void CSeatManager::onNewSeatResource(SP<CWLSeatResource> resource) {
    seatResources.emplace_back(makeShared<SSeatResourceContainer>(resource));
}

SP<CSeatManager::SSeatResourceContainer> CSeatManager::containerForResource(SP<CWLSeatResource> seatResource) {
    for (auto& c : seatResources) {
        if (c->resource == seatResource)
            return c;
    }

    return nullptr;
}

uint32_t CSeatManager::nextSerial(SP<CWLSeatResource> seatResource) {
    if (!seatResource)
        return 0;

    auto container = containerForResource(seatResource);

    ASSERT(container);

    auto serial = wl_display_next_serial(g_pCompositor->m_sWLDisplay);

    container->serials.emplace_back(serial);

    if (container->serials.size() > MAX_SERIAL_STORE_LEN)
        container->serials.erase(container->serials.begin());

    return serial;
}

bool CSeatManager::serialValid(SP<CWLSeatResource> seatResource, uint32_t serial) {
    if (!seatResource)
        return false;

    auto container = containerForResource(seatResource);

    ASSERT(container);

    for (auto it = container->serials.begin(); it != container->serials.end(); ++it) {
        if (*it == serial) {
            container->serials.erase(it);
            return true;
        }
    }

    return false;
}

void CSeatManager::updateCapabilities(uint32_t capabilities) {
    PROTO::seat->updateCapabilities(capabilities);
}

void CSeatManager::setMouse(SP<IPointer> MAUZ) {
    if (mouse == MAUZ)
        return;

    mouse = MAUZ;
}

void CSeatManager::setKeyboard(SP<IKeyboard> KEEB) {
    if (keyboard == KEEB)
        return;

    if (keyboard)
        keyboard->active = false;
    keyboard = KEEB;

    if (KEEB)
        KEEB->active = true;

    updateActiveKeyboardData();
}

void CSeatManager::updateActiveKeyboardData() {
    if (keyboard)
        PROTO::seat->updateRepeatInfo(keyboard->wlr()->repeat_info.rate, keyboard->wlr()->repeat_info.delay);
    PROTO::seat->updateKeymap();
}

void CSeatManager::setKeyboardFocus(wlr_surface* surf) {
    if (state.keyboardFocus == surf)
        return;

    if (!keyboard || !keyboard->wlr()) {
        Debug::log(ERR, "BUG THIS: setKeyboardFocus without a valid keyboard set");
        return;
    }

    hyprListener_keyboardSurfaceDestroy.removeCallback();

    if (state.keyboardFocusResource) {
        // we will iterate over all bound wl_seat
        // resources here, because some idiotic apps (e.g. those based on smithay)
        // tend to bind wl_seat twice.
        // I can't be arsed to actually pass all events to all seat resources, so we will
        // only pass enter and leave.
        // If you have an issue with that, fix your app.
        auto client = state.keyboardFocusResource->client();
        for (auto& s : seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto& k : s->resource->keyboards) {
                if (!k)
                    continue;

                k->sendLeave();
            }
        }
    }

    state.keyboardFocusResource.reset();
    state.keyboardFocus = surf;

    if (!surf) {
        events.keyboardFocusChange.emit();
        return;
    }

    auto client = wl_resource_get_client(surf->resource);
    for (auto& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.keyboardFocusResource = r->resource;
        for (auto& k : r->resource->keyboards) {
            if (!k)
                continue;

            k->sendEnter(surf);
            k->sendMods(keyboard->wlr()->modifiers.depressed, keyboard->wlr()->modifiers.latched, keyboard->wlr()->modifiers.locked, keyboard->wlr()->modifiers.group);
        }
    }

    hyprListener_keyboardSurfaceDestroy.initCallback(
        &surf->events.destroy, [this](void* owner, void* data) { setKeyboardFocus(nullptr); }, nullptr, "CSeatManager");

    events.keyboardFocusChange.emit();
}

void CSeatManager::sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state_) {
    if (!state.keyboardFocusResource)
        return;

    for (auto& k : state.keyboardFocusResource->keyboards) {
        if (!k)
            continue;

        k->sendKey(timeMs, key, state_);
    }
}

void CSeatManager::sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!state.keyboardFocusResource)
        return;

    for (auto& k : state.keyboardFocusResource->keyboards) {
        if (!k)
            continue;

        k->sendMods(depressed, latched, locked, group);
    }
}

void CSeatManager::setPointerFocus(wlr_surface* surf, const Vector2D& local) {
    if (state.pointerFocus == surf)
        return;

    if (!mouse || !mouse->wlr()) {
        Debug::log(ERR, "BUG THIS: setPointerFocus without a valid mouse set");
        return;
    }

    hyprListener_pointerSurfaceDestroy.removeCallback();

    if (state.pointerFocusResource) {
        auto client = state.pointerFocusResource->client();
        for (auto& s : seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto& p : s->resource->pointers) {
                if (!p)
                    continue;

                p->sendLeave();
            }
        }
    }

    auto lastPointerFocusResource = state.pointerFocusResource;

    state.pointerFocusResource.reset();
    state.pointerFocus = surf;

    if (!surf) {
        sendPointerFrame(lastPointerFocusResource);
        events.pointerFocusChange.emit();
        return;
    }

    auto client = wl_resource_get_client(surf->resource);
    for (auto& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.pointerFocusResource = r->resource;
        for (auto& p : r->resource->pointers) {
            if (!p)
                continue;

            p->sendEnter(surf, local);
        }
    }

    if (state.pointerFocusResource != lastPointerFocusResource)
        sendPointerFrame(lastPointerFocusResource);

    sendPointerFrame();

    hyprListener_pointerSurfaceDestroy.initCallback(
        &surf->events.destroy, [this](void* owner, void* data) { setPointerFocus(nullptr, {}); }, nullptr, "CSeatManager");

    events.pointerFocusChange.emit();
}

void CSeatManager::sendPointerMotion(uint32_t timeMs, const Vector2D& local) {
    if (!state.pointerFocusResource)
        return;

    for (auto& p : state.pointerFocusResource->pointers) {
        if (!p)
            continue;

        p->sendMotion(timeMs, local);
    }

    lastLocalCoords = local;
}

void CSeatManager::sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state_) {
    if (!state.pointerFocusResource)
        return;

    for (auto& p : state.pointerFocusResource->pointers) {
        if (!p)
            continue;

        p->sendButton(timeMs, key, state_);
    }
}

void CSeatManager::sendPointerFrame() {
    if (!state.pointerFocusResource)
        return;

    sendPointerFrame(state.pointerFocusResource);
}

void CSeatManager::sendPointerFrame(WP<CWLSeatResource> pResource) {
    if (!pResource)
        return;

    for (auto& p : pResource->pointers) {
        if (!p)
            continue;

        p->sendFrame();
    }
}

void CSeatManager::sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, wl_pointer_axis_source source,
                                   wl_pointer_axis_relative_direction relative) {
    if (!state.pointerFocusResource)
        return;

    for (auto& p : state.pointerFocusResource->pointers) {
        if (!p)
            continue;

        p->sendAxis(timeMs, axis, value);
        p->sendAxisSource(source);
        p->sendAxisRelativeDirection(axis, relative);

        if (source == 0)
            p->sendAxisDiscrete(axis, discrete);

        if (value == 0)
            p->sendAxisStop(timeMs, axis);
    }
}

void CSeatManager::sendTouchDown(wlr_surface* surf, uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (state.touchFocus == surf)
        return;

    hyprListener_touchSurfaceDestroy.removeCallback();

    if (state.touchFocusResource) {
        auto client = state.touchFocusResource->client();
        for (auto& s : seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto& t : s->resource->touches) {
                if (!t)
                    continue;

                t->sendUp(timeMs, id);
            }
        }
    }

    state.touchFocusResource.reset();
    state.touchFocus = surf;

    if (!surf) {
        events.touchFocusChange.emit();
        return;
    }

    auto client = wl_resource_get_client(surf->resource);
    for (auto& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.touchFocusResource = r->resource;
        for (auto& t : r->resource->touches) {
            if (!t)
                continue;

            t->sendDown(surf, timeMs, id, local);
        }
    }

    hyprListener_touchSurfaceDestroy.initCallback(
        &surf->events.destroy, [this, timeMs, id](void* owner, void* data) { sendTouchUp(timeMs + 10, id); }, nullptr, "CSeatManager");

    events.touchFocusChange.emit();
}

void CSeatManager::sendTouchUp(uint32_t timeMs, int32_t id) {
    sendTouchDown(nullptr, timeMs, id, {});
}

void CSeatManager::sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!state.touchFocusResource)
        return;

    for (auto& t : state.touchFocusResource->touches) {
        if (!t)
            continue;

        t->sendMotion(timeMs, id, local);
    }
}

void CSeatManager::sendTouchFrame() {
    if (!state.touchFocusResource)
        return;

    for (auto& t : state.touchFocusResource->touches) {
        if (!t)
            continue;

        t->sendFrame();
    }
}

void CSeatManager::sendTouchCancel() {
    if (!state.touchFocusResource)
        return;

    for (auto& t : state.touchFocusResource->touches) {
        if (!t)
            continue;

        t->sendCancel();
    }
}

void CSeatManager::sendTouchShape(int32_t id, const Vector2D& shape) {
    if (!state.touchFocusResource)
        return;

    for (auto& t : state.touchFocusResource->touches) {
        if (!t)
            continue;

        t->sendShape(id, shape);
    }
}

void CSeatManager::sendTouchOrientation(int32_t id, double angle) {
    if (!state.touchFocusResource)
        return;

    for (auto& t : state.touchFocusResource->touches) {
        if (!t)
            continue;

        t->sendOrientation(id, angle);
    }
}

void CSeatManager::refocusGrab() {
    if (!seatGrab)
        return;

    if (seatGrab->surfs.size() > 0) {
        // try to find a surf in focus first
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        for (auto& s : seatGrab->surfs) {
            auto hlSurf = CWLSurface::surfaceFromWlr(s);
            if (!hlSurf)
                continue;

            auto b = hlSurf->getSurfaceBoxGlobal();
            if (!b.has_value())
                continue;

            if (!b->containsPoint(MOUSE))
                continue;

            if (seatGrab->keyboard)
                setKeyboardFocus(s);
            if (seatGrab->pointer)
                setPointerFocus(s, MOUSE - b->pos());
            return;
        }

        wlr_surface* surf = seatGrab->surfs.at(0);
        if (seatGrab->keyboard)
            setKeyboardFocus(surf);
        if (seatGrab->pointer)
            setPointerFocus(surf, {});
    }
}

void CSeatManager::onSetCursor(SP<CWLSeatResource> seatResource, uint32_t serial, wlr_surface* surf, const Vector2D& hotspot) {
    if (!state.pointerFocusResource || !seatResource || seatResource->client() != state.pointerFocusResource->client()) {
        Debug::log(LOG, "[seatmgr] Rejecting a setCursor because the client ain't in focus");
        return;
    }

    // TODO: fix this. Probably should be done in the CWlPointer as the serial could be lost by us.
    // if (!serialValid(seatResource, serial)) {
    //     Debug::log(LOG, "[seatmgr] Rejecting a setCursor because the serial is invalid");
    //     return;
    // }

    events.setCursor.emit(SSetCursorEvent{surf, hotspot});
}

SP<CWLSeatResource> CSeatManager::seatResourceForClient(wl_client* client) {
    return PROTO::seat->seatResourceForClient(client);
}

void CSeatManager::setCurrentSelection(SP<IDataSource> source) {
    if (source == selection.currentSelection) {
        Debug::log(WARN, "[seat] duplicated setCurrentSelection?");
        return;
    }

    selection.destroySelection.reset();

    if (selection.currentSelection)
        selection.currentSelection->cancelled();

    if (!source)
        PROTO::data->setSelection(nullptr);

    selection.currentSelection = source;

    if (source) {
        selection.destroySelection = source->events.destroy.registerListener([this](std::any d) { setCurrentSelection(nullptr); });
        PROTO::data->setSelection(source);
        PROTO::dataWlr->setSelection(source, false);
    }
}

void CSeatManager::setCurrentPrimarySelection(SP<IDataSource> source) {
    if (source == selection.currentPrimarySelection) {
        Debug::log(WARN, "[seat] duplicated setCurrentPrimarySelection?");
        return;
    }

    selection.destroyPrimarySelection.reset();

    if (selection.currentPrimarySelection)
        selection.currentPrimarySelection->cancelled();

    if (!source)
        PROTO::primarySelection->setSelection(nullptr);

    selection.currentPrimarySelection = source;

    if (source) {
        selection.destroyPrimarySelection = source->events.destroy.registerListener([this](std::any d) { setCurrentPrimarySelection(nullptr); });
        PROTO::primarySelection->setSelection(source);
        PROTO::dataWlr->setSelection(source, true);
    }
}

void CSeatManager::setGrab(SP<CSeatGrab> grab) {
    if (seatGrab) {
        auto oldGrab = seatGrab;
        seatGrab.reset();
        g_pInputManager->refocus();
        if (oldGrab->onEnd)
            oldGrab->onEnd();
    }

    if (!grab)
        return;

    seatGrab = grab;

    refocusGrab();
}

void CSeatManager::resendEnterEvents() {
    wlr_surface* kb = state.keyboardFocus;
    wlr_surface* pt = state.pointerFocus;

    auto         last = lastLocalCoords;

    setKeyboardFocus(nullptr);
    setPointerFocus(nullptr, {});

    setKeyboardFocus(kb);
    setPointerFocus(pt, last);
}

bool CSeatGrab::accepts(wlr_surface* surf) {
    return std::find(surfs.begin(), surfs.end(), surf) != surfs.end();
}

void CSeatGrab::add(wlr_surface* surf) {
    surfs.push_back(surf);
}

void CSeatGrab::remove(wlr_surface* surf) {
    std::erase(surfs, surf);
    if ((keyboard && g_pSeatManager->state.keyboardFocus == surf) || (pointer && g_pSeatManager->state.pointerFocus == surf))
        g_pSeatManager->refocusGrab();
}

void CSeatGrab::setCallback(std::function<void()> onEnd_) {
    onEnd = onEnd_;
}

void CSeatGrab::clear() {
    surfs.clear();
}
