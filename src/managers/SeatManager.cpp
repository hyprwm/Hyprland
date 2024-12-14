#include "SeatManager.hpp"
#include "../protocols/core/Seat.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/DataDeviceWlr.hpp"
#include "../protocols/PrimarySelection.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../Compositor.hpp"
#include "../devices/IKeyboard.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"
#include <algorithm>
#include <ranges>

CSeatManager::CSeatManager() {
    listeners.newSeatResource = PROTO::seat->events.newSeatResource.registerListener([this](std::any res) { onNewSeatResource(std::any_cast<SP<CWLSeatResource>>(res)); });
}

CSeatManager::SSeatResourceContainer::SSeatResourceContainer(SP<CWLSeatResource> res) : resource(res) {
    listeners.destroy = res->events.destroy.registerListener(
        [this](std::any data) { std::erase_if(g_pSeatManager->seatResources, [this](const auto& e) { return e->resource.expired() || e->resource == resource; }); });
}

void CSeatManager::onNewSeatResource(SP<CWLSeatResource> resource) {
    seatResources.emplace_back(makeShared<SSeatResourceContainer>(resource));
}

SP<CSeatManager::SSeatResourceContainer> CSeatManager::containerForResource(SP<CWLSeatResource> seatResource) {
    for (auto const& c : seatResources) {
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
        PROTO::seat->updateRepeatInfo(keyboard->repeatRate, keyboard->repeatDelay);
    PROTO::seat->updateKeymap();
}

void CSeatManager::setKeyboardFocus(SP<CWLSurfaceResource> surf) {
    if (state.keyboardFocus == surf)
        return;

    if (!keyboard) {
        Debug::log(ERR, "BUG THIS: setKeyboardFocus without a valid keyboard set");
        return;
    }

    listeners.keyboardSurfaceDestroy.reset();

    if (state.keyboardFocusResource) {
        auto client = state.keyboardFocusResource->client();
        for (auto const& s : seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto const& k : s->resource->keyboards) {
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

    auto client = surf->client();
    for (auto const& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.keyboardFocusResource = r->resource;
        for (auto const& k : r->resource->keyboards) {
            if (!k)
                continue;

            k->sendEnter(surf);
            k->sendMods(keyboard->modifiersState.depressed, keyboard->modifiersState.latched, keyboard->modifiersState.locked, keyboard->modifiersState.group);
        }
    }

    listeners.keyboardSurfaceDestroy = surf->events.destroy.registerListener([this](std::any d) { setKeyboardFocus(nullptr); });

    events.keyboardFocusChange.emit();
}

void CSeatManager::sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state_) {
    if (!state.keyboardFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.keyboardFocusResource->client())
            continue;

        for (auto const& k : s->resource->keyboards) {
            if (!k)
                continue;

            k->sendKey(timeMs, key, state_);
        }
    }
}

void CSeatManager::sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!state.keyboardFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.keyboardFocusResource->client())
            continue;

        for (auto const& k : s->resource->keyboards) {
            if (!k)
                continue;

            k->sendMods(depressed, latched, locked, group);
        }
    }
}

void CSeatManager::setPointerFocus(SP<CWLSurfaceResource> surf, const Vector2D& local) {
    if (state.pointerFocus == surf)
        return;

    if (PROTO::data->dndActive()) {
        if (state.dndPointerFocus == surf)
            return;
        Debug::log(LOG, "[seatmgr] Refusing pointer focus during an active dnd, but setting dndPointerFocus");
        state.dndPointerFocus = surf;
        events.dndPointerFocusChange.emit();
        return;
    }

    if (!mouse) {
        Debug::log(ERR, "BUG THIS: setPointerFocus without a valid mouse set");
        return;
    }

    listeners.pointerSurfaceDestroy.reset();

    if (state.pointerFocusResource) {
        auto client = state.pointerFocusResource->client();
        for (auto const& s : seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto const& p : s->resource->pointers) {
                if (!p)
                    continue;

                p->sendLeave();
            }
        }
    }

    auto lastPointerFocusResource = state.pointerFocusResource;

    state.dndPointerFocus.reset();
    state.pointerFocusResource.reset();
    state.pointerFocus = surf;

    if (!surf) {
        sendPointerFrame(lastPointerFocusResource);
        events.pointerFocusChange.emit();
        return;
    }

    state.dndPointerFocus = surf;

    auto client = surf->client();
    for (auto const& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.pointerFocusResource = r->resource;
        for (auto const& p : r->resource->pointers) {
            if (!p)
                continue;

            p->sendEnter(surf, local);
        }
    }

    if (state.pointerFocusResource != lastPointerFocusResource)
        sendPointerFrame(lastPointerFocusResource);

    sendPointerFrame();

    listeners.pointerSurfaceDestroy = surf->events.destroy.registerListener([this](std::any d) { setPointerFocus(nullptr, {}); });

    events.pointerFocusChange.emit();
    events.dndPointerFocusChange.emit();
}

void CSeatManager::sendPointerMotion(uint32_t timeMs, const Vector2D& local) {
    if (!state.pointerFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->pointers) {
            if (!p)
                continue;

            p->sendMotion(timeMs, local);
        }
    }

    lastLocalCoords = local;
}

void CSeatManager::sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state_) {
    if (!state.pointerFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->pointers) {
            if (!p)
                continue;

            p->sendButton(timeMs, key, state_);
        }
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

    for (auto const& s : seatResources) {
        if (s->resource->client() != pResource->client())
            continue;

        for (auto const& p : s->resource->pointers) {
            if (!p)
                continue;

            p->sendFrame();
        }
    }
}

void CSeatManager::sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, int32_t value120, wl_pointer_axis_source source,
                                   wl_pointer_axis_relative_direction relative) {
    if (!state.pointerFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->pointers) {
            if (!p)
                continue;

            p->sendAxis(timeMs, axis, value);
            p->sendAxisSource(source);
            p->sendAxisRelativeDirection(axis, relative);

            if (source == 0) {
                if (p->version() >= 8)
                    p->sendAxisValue120(axis, value120);
                else
                    p->sendAxisDiscrete(axis, discrete);
            } else if (value == 0)
                p->sendAxisStop(timeMs, axis);
        }
    }
}

void CSeatManager::sendTouchDown(SP<CWLSurfaceResource> surf, uint32_t timeMs, int32_t id, const Vector2D& local) {
    listeners.touchSurfaceDestroy.reset();

    state.touchFocusResource.reset();
    state.touchFocus = surf;

    auto client = surf->client();
    for (auto const& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.touchFocusResource = r->resource;
        for (auto const& t : r->resource->touches) {
            if (!t)
                continue;

            t->sendDown(surf, timeMs, id, local);
        }
    }

    listeners.touchSurfaceDestroy = surf->events.destroy.registerListener([this, timeMs, id](std::any d) { sendTouchUp(timeMs + 10, id); });

    touchLocks++;

    if (touchLocks <= 1)
        events.touchFocusChange.emit();
}

void CSeatManager::sendTouchUp(uint32_t timeMs, int32_t id) {
    if (!state.touchFocusResource || touchLocks <= 0)
        return;

    auto client = state.touchFocusResource->client();
    for (auto const& r : seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        state.touchFocusResource = r->resource;
        for (auto const& t : r->resource->touches) {
            if (!t)
                continue;

            t->sendUp(timeMs, id);
        }
    }

    touchLocks--;

    if (touchLocks <= 0)
        events.touchFocusChange.emit();
}

void CSeatManager::sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!state.touchFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->touches) {
            if (!t)
                continue;

            t->sendMotion(timeMs, id, local);
        }
    }
}

void CSeatManager::sendTouchFrame() {
    if (!state.touchFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->touches) {
            if (!t)
                continue;

            t->sendFrame();
        }
    }
}

void CSeatManager::sendTouchCancel() {
    if (!state.touchFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->touches) {
            if (!t)
                continue;

            t->sendCancel();
        }
    }
}

void CSeatManager::sendTouchShape(int32_t id, const Vector2D& shape) {
    if (!state.touchFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->touches) {
            if (!t)
                continue;

            t->sendShape(id, shape);
        }
    }
}

void CSeatManager::sendTouchOrientation(int32_t id, double angle) {
    if (!state.touchFocusResource)
        return;

    for (auto const& s : seatResources) {
        if (s->resource->client() != state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->touches) {
            if (!t)
                continue;

            t->sendOrientation(id, angle);
        }
    }
}

void CSeatManager::refocusGrab() {
    if (!seatGrab)
        return;

    if (seatGrab->surfs.size() > 0) {
        // try to find a surf in focus first
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        for (auto const& s : seatGrab->surfs) {
            auto hlSurf = CWLSurface::fromResource(s.lock());
            if (!hlSurf)
                continue;

            auto b = hlSurf->getSurfaceBoxGlobal();
            if (!b.has_value())
                continue;

            if (!b->containsPoint(MOUSE))
                continue;

            if (seatGrab->keyboard)
                setKeyboardFocus(s.lock());
            if (seatGrab->pointer)
                setPointerFocus(s.lock(), MOUSE - b->pos());
            return;
        }

        SP<CWLSurfaceResource> surf = seatGrab->surfs.at(0).lock();
        if (seatGrab->keyboard)
            setKeyboardFocus(surf);
        if (seatGrab->pointer)
            setPointerFocus(surf, {});
    }
}

void CSeatManager::onSetCursor(SP<CWLSeatResource> seatResource, uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& hotspot) {
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

    events.setSelection.emit();
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

    events.setPrimarySelection.emit();
}

void CSeatManager::setGrab(SP<CSeatGrab> grab) {
    if (seatGrab) {
        auto oldGrab = seatGrab;
        seatGrab.reset();
        g_pInputManager->refocus();

        auto           currentFocus = state.keyboardFocus.lock();
        auto           refocus      = !currentFocus;

        SP<CWLSurface> surf;
        PHLLS          layer;

        if (!refocus) {
            surf  = CWLSurface::fromResource(currentFocus);
            layer = surf->getLayer();
        }

        if (!refocus && !layer) {
            auto popup = surf->getPopup();
            if (popup) {
                auto parent = popup->getT1Owner();
                layer       = parent->getLayer();
            }
        }

        if (!refocus && layer)
            refocus = layer->interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;

        if (refocus) {
            auto candidate = g_pCompositor->m_pLastWindow.lock();

            if (candidate)
                g_pCompositor->focusWindow(candidate);
        }

        if (oldGrab->onEnd)
            oldGrab->onEnd();
    }

    if (!grab)
        return;

    seatGrab = grab;

    refocusGrab();
}

void CSeatManager::resendEnterEvents() {
    SP<CWLSurfaceResource> kb = state.keyboardFocus.lock();
    SP<CWLSurfaceResource> pt = state.pointerFocus.lock();

    auto                   last = lastLocalCoords;

    setKeyboardFocus(nullptr);
    setPointerFocus(nullptr, {});

    setKeyboardFocus(kb);
    setPointerFocus(pt, last);
}

bool CSeatGrab::accepts(SP<CWLSurfaceResource> surf) {
    return std::find(surfs.begin(), surfs.end(), surf) != surfs.end();
}

void CSeatGrab::add(SP<CWLSurfaceResource> surf) {
    surfs.emplace_back(surf);
}

void CSeatGrab::remove(SP<CWLSurfaceResource> surf) {
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
