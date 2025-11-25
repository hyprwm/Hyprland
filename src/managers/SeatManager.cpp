#include "SeatManager.hpp"
#include "../protocols/core/Seat.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/DataDeviceWlr.hpp"
#include "../protocols/ExtDataDevice.hpp"
#include "../protocols/PrimarySelection.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../Compositor.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../devices/IKeyboard.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"
#include <algorithm>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <ranges>
#include <cstring>

using namespace Hyprutils::Utils;

CSeatManager::CSeatManager() {
    m_listeners.newSeatResource = PROTO::seat->m_events.newSeatResource.listen([this](const auto& resource) { onNewSeatResource(resource); });
}

CSeatManager::SSeatResourceContainer::SSeatResourceContainer(SP<CWLSeatResource> res) : resource(res) {
    listeners.destroy = res->m_events.destroy.listen(
        [this] { std::erase_if(g_pSeatManager->m_seatResources, [this](const auto& e) { return e->resource.expired() || e->resource == resource; }); });
}

void CSeatManager::onNewSeatResource(SP<CWLSeatResource> resource) {
    m_seatResources.emplace_back(makeShared<SSeatResourceContainer>(resource));
}

SP<CSeatManager::SSeatResourceContainer> CSeatManager::containerForResource(SP<CWLSeatResource> seatResource) {
    for (auto const& c : m_seatResources) {
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

    auto serial = wl_display_next_serial(g_pCompositor->m_wlDisplay);

    container->serials.emplace_back(serial);

    if (container->serials.size() > MAX_SERIAL_STORE_LEN)
        container->serials.erase(container->serials.begin());

    return serial;
}

bool CSeatManager::serialValid(SP<CWLSeatResource> seatResource, uint32_t serial, bool erase) {
    if (!seatResource)
        return false;

    auto container = containerForResource(seatResource);

    ASSERT(container);

    for (auto it = container->serials.begin(); it != container->serials.end(); ++it) {
        if (*it == serial) {
            if (erase)
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
    if (m_mouse == MAUZ)
        return;

    m_mouse = MAUZ;
}

void CSeatManager::setKeyboard(SP<IKeyboard> KEEB) {
    if (m_keyboard == KEEB)
        return;

    if (m_keyboard)
        m_keyboard->m_active = false;
    m_keyboard = KEEB;

    if (KEEB)
        KEEB->m_active = true;

    updateActiveKeyboardData();
}

void CSeatManager::updateActiveKeyboardData() {
    if (m_keyboard)
        PROTO::seat->updateRepeatInfo(m_keyboard->m_repeatRate, m_keyboard->m_repeatDelay);
    PROTO::seat->updateKeymap();
}

void CSeatManager::setKeyboardFocus(SP<CWLSurfaceResource> surf) {
    if (m_state.keyboardFocus == surf)
        return;

    if (!m_keyboard) {
        Debug::log(ERR, "BUG THIS: setKeyboardFocus without a valid keyboard set");
        return;
    }

    m_listeners.keyboardSurfaceDestroy.reset();

    if (m_state.keyboardFocusResource) {
        auto client = m_state.keyboardFocusResource->client();
        for (auto const& s : m_seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto const& k : s->resource->m_keyboards) {
                if (!k)
                    continue;

                k->sendMods(0, m_keyboard->m_modifiersState.latched, m_keyboard->m_modifiersState.locked, m_keyboard->m_modifiersState.group);
                k->sendLeave();
            }
        }
    }

    m_state.keyboardFocusResource.reset();
    m_state.keyboardFocus = surf;

    if (!surf) {
        m_events.keyboardFocusChange.emit();
        return;
    }

    wl_array keys;
    wl_array_init(&keys);
    CScopeGuard x([&keys] { wl_array_release(&keys); });

    const auto& PRESSED = g_pInputManager->getKeysFromAllKBs();
    static_assert(std::is_same_v<std::decay_t<decltype(PRESSED)>::value_type, uint32_t>, "Element type different from keycode type uint32_t");

    const auto PRESSEDARRSIZE = PRESSED.size() * sizeof(uint32_t);
    const auto PKEYS          = wl_array_add(&keys, PRESSEDARRSIZE);
    if (PKEYS)
        memcpy(PKEYS, PRESSED.data(), PRESSEDARRSIZE);

    auto client = surf->client();
    for (auto const& r : m_seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        m_state.keyboardFocusResource = r->resource;
        for (auto const& k : r->resource->m_keyboards) {
            if (!k)
                continue;

            k->sendEnter(surf, &keys);
            k->sendMods(m_keyboard->m_modifiersState.depressed, m_keyboard->m_modifiersState.latched, m_keyboard->m_modifiersState.locked, m_keyboard->m_modifiersState.group);
        }
    }

    m_listeners.keyboardSurfaceDestroy = surf->m_events.destroy.listen([this] { setKeyboardFocus(nullptr); });

    m_events.keyboardFocusChange.emit();
}

void CSeatManager::sendKeyboardKey(uint32_t timeMs, uint32_t key, wl_keyboard_key_state state_) {
    if (!m_state.keyboardFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.keyboardFocusResource->client())
            continue;

        for (auto const& k : s->resource->m_keyboards) {
            if (!k)
                continue;

            k->sendKey(timeMs, key, state_);
        }
    }
}

void CSeatManager::sendKeyboardMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!m_state.keyboardFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.keyboardFocusResource->client())
            continue;

        for (auto const& k : s->resource->m_keyboards) {
            if (!k)
                continue;

            k->sendMods(depressed, latched, locked, group);
        }
    }
}

void CSeatManager::setPointerFocus(SP<CWLSurfaceResource> surf, const Vector2D& local) {
    if (m_state.pointerFocus == surf)
        return;

    if (PROTO::data->dndActive() && surf) {
        if (m_state.dndPointerFocus == surf)
            return;
        Debug::log(LOG, "[seatmgr] Refusing pointer focus during an active dnd, but setting dndPointerFocus");
        m_state.dndPointerFocus = surf;
        m_events.dndPointerFocusChange.emit();
        return;
    }

    if (!m_mouse) {
        Debug::log(ERR, "BUG THIS: setPointerFocus without a valid mouse set");
        return;
    }

    m_listeners.pointerSurfaceDestroy.reset();

    if (m_state.pointerFocusResource) {
        auto client = m_state.pointerFocusResource->client();
        for (auto const& s : m_seatResources) {
            if (s->resource->client() != client)
                continue;

            for (auto const& p : s->resource->m_pointers) {
                if (!p)
                    continue;

                p->sendLeave();
            }
        }
    }

    auto lastPointerFocusResource = m_state.pointerFocusResource;

    m_state.dndPointerFocus.reset();
    m_state.pointerFocusResource.reset();
    m_state.pointerFocus = surf;

    if (!surf) {
        sendPointerFrame(lastPointerFocusResource);
        m_events.pointerFocusChange.emit();
        return;
    }

    m_state.dndPointerFocus = surf;

    auto client = surf->client();
    for (auto const& r : m_seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        m_state.pointerFocusResource = r->resource;
        for (auto const& p : r->resource->m_pointers) {
            if (!p)
                continue;

            p->sendEnter(surf, local);
        }
    }

    if (m_state.pointerFocusResource != lastPointerFocusResource)
        sendPointerFrame(lastPointerFocusResource);

    sendPointerFrame();

    m_listeners.pointerSurfaceDestroy = surf->m_events.destroy.listen([this] { setPointerFocus(nullptr, {}); });

    m_events.pointerFocusChange.emit();
    m_events.dndPointerFocusChange.emit();
}

void CSeatManager::sendPointerMotion(uint32_t timeMs, const Vector2D& local) {
    if (!m_state.pointerFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->m_pointers) {
            if (!p)
                continue;

            p->sendMotion(timeMs, local);
        }
    }

    m_lastLocalCoords = local;
}

void CSeatManager::sendPointerButton(uint32_t timeMs, uint32_t key, wl_pointer_button_state state_) {
    if (!m_state.pointerFocusResource || PROTO::data->dndActive())
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->m_pointers) {
            if (!p)
                continue;

            p->sendButton(timeMs, key, state_);
        }
    }
}

void CSeatManager::sendPointerFrame() {
    if (!m_state.pointerFocusResource)
        return;

    sendPointerFrame(m_state.pointerFocusResource);
}

void CSeatManager::sendPointerFrame(WP<CWLSeatResource> pResource) {
    if (!pResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != pResource->client())
            continue;

        for (auto const& p : s->resource->m_pointers) {
            if (!p)
                continue;

            p->sendFrame();
        }
    }
}

void CSeatManager::sendPointerAxis(uint32_t timeMs, wl_pointer_axis axis, double value, int32_t discrete, int32_t value120, wl_pointer_axis_source source,
                                   wl_pointer_axis_relative_direction relative) {
    if (!m_state.pointerFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.pointerFocusResource->client())
            continue;

        for (auto const& p : s->resource->m_pointers) {
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
    m_listeners.touchSurfaceDestroy.reset();

    m_state.touchFocusResource.reset();
    m_state.touchFocus = surf;

    auto client = surf->client();
    for (auto const& r : m_seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        m_state.touchFocusResource = r->resource;
        for (auto const& t : r->resource->m_touches) {
            if (!t)
                continue;

            t->sendDown(surf, timeMs, id, local);
        }
    }

    m_listeners.touchSurfaceDestroy = surf->m_events.destroy.listen([this, timeMs, id] { sendTouchUp(timeMs + 10, id); });

    m_touchLocks++;

    if (m_touchLocks <= 1)
        m_events.touchFocusChange.emit();
}

void CSeatManager::sendTouchUp(uint32_t timeMs, int32_t id) {
    if (!m_state.touchFocusResource || m_touchLocks <= 0)
        return;

    auto client = m_state.touchFocusResource->client();
    for (auto const& r : m_seatResources | std::views::reverse) {
        if (r->resource->client() != client)
            continue;

        m_state.touchFocusResource = r->resource;
        for (auto const& t : r->resource->m_touches) {
            if (!t)
                continue;

            t->sendUp(timeMs, id);
        }
    }

    m_touchLocks--;

    if (m_touchLocks <= 0)
        m_events.touchFocusChange.emit();
}

void CSeatManager::sendTouchMotion(uint32_t timeMs, int32_t id, const Vector2D& local) {
    if (!m_state.touchFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->m_touches) {
            if (!t)
                continue;

            t->sendMotion(timeMs, id, local);
        }
    }
}

void CSeatManager::sendTouchFrame() {
    if (!m_state.touchFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->m_touches) {
            if (!t)
                continue;

            t->sendFrame();
        }
    }
}

void CSeatManager::sendTouchCancel() {
    if (!m_state.touchFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->m_touches) {
            if (!t)
                continue;

            t->sendCancel();
        }
    }
}

void CSeatManager::sendTouchShape(int32_t id, const Vector2D& shape) {
    if (!m_state.touchFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->m_touches) {
            if (!t)
                continue;

            t->sendShape(id, shape);
        }
    }
}

void CSeatManager::sendTouchOrientation(int32_t id, double angle) {
    if (!m_state.touchFocusResource)
        return;

    for (auto const& s : m_seatResources) {
        if (s->resource->client() != m_state.touchFocusResource->client())
            continue;

        for (auto const& t : s->resource->m_touches) {
            if (!t)
                continue;

            t->sendOrientation(id, angle);
        }
    }
}

void CSeatManager::refocusGrab() {
    if (!m_seatGrab)
        return;

    if (!m_seatGrab->m_surfs.empty()) {
        // try to find a surf in focus first
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        for (auto const& s : m_seatGrab->m_surfs) {
            auto hlSurf = CWLSurface::fromResource(s.lock());
            if (!hlSurf)
                continue;

            auto b = hlSurf->getSurfaceBoxGlobal();
            if (!b.has_value())
                continue;

            if (!b->containsPoint(MOUSE))
                continue;

            if (m_seatGrab->m_keyboard)
                setKeyboardFocus(s.lock());
            if (m_seatGrab->m_pointer)
                setPointerFocus(s.lock(), MOUSE - b->pos());
            return;
        }

        SP<CWLSurfaceResource> surf = m_seatGrab->m_surfs.at(0).lock();
        if (m_seatGrab->m_keyboard)
            setKeyboardFocus(surf);
        if (m_seatGrab->m_pointer)
            setPointerFocus(surf, {});
    }
}

void CSeatManager::onSetCursor(SP<CWLSeatResource> seatResource, uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& hotspot) {
    if (!m_state.pointerFocusResource || !seatResource || seatResource->client() != m_state.pointerFocusResource->client()) {
        Debug::log(LOG, "[seatmgr] Rejecting a setCursor because the client ain't in focus");
        return;
    }

    // TODO: fix this. Probably should be done in the CWlPointer as the serial could be lost by us.
    // if (!serialValid(seatResource, serial)) {
    //     Debug::log(LOG, "[seatmgr] Rejecting a setCursor because the serial is invalid");
    //     return;
    // }

    m_events.setCursor.emit(SSetCursorEvent{surf, hotspot});
}

SP<CWLSeatResource> CSeatManager::seatResourceForClient(wl_client* client) {
    return PROTO::seat->seatResourceForClient(client);
}

void CSeatManager::setCurrentSelection(SP<IDataSource> source) {
    if (source == m_selection.currentSelection) {
        Debug::log(WARN, "[seat] duplicated setCurrentSelection?");
        return;
    }

    m_selection.destroySelection.reset();

    if (m_selection.currentSelection)
        m_selection.currentSelection->cancelled();

    if (!source)
        PROTO::data->setSelection(nullptr);

    m_selection.currentSelection = source;

    if (source) {
        m_selection.destroySelection = source->m_events.destroy.listen([this] { setCurrentSelection(nullptr); });
        PROTO::data->setSelection(source);
        PROTO::dataWlr->setSelection(source, false);
        PROTO::extDataDevice->setSelection(source, false);
    }

    m_events.setSelection.emit();
}

void CSeatManager::setCurrentPrimarySelection(SP<IDataSource> source) {
    if (source == m_selection.currentPrimarySelection) {
        Debug::log(WARN, "[seat] duplicated setCurrentPrimarySelection?");
        return;
    }

    m_selection.destroyPrimarySelection.reset();

    if (m_selection.currentPrimarySelection)
        m_selection.currentPrimarySelection->cancelled();

    if (!source)
        PROTO::primarySelection->setSelection(nullptr);

    m_selection.currentPrimarySelection = source;

    if (source) {
        m_selection.destroyPrimarySelection = source->m_events.destroy.listen([this] { setCurrentPrimarySelection(nullptr); });
        PROTO::primarySelection->setSelection(source);
        PROTO::dataWlr->setSelection(source, true);
        PROTO::extDataDevice->setSelection(source, true);
    }

    m_events.setPrimarySelection.emit();
}

void CSeatManager::setGrab(SP<CSeatGrab> grab) {
    if (m_seatGrab) {
        auto oldGrab = m_seatGrab;

        // Try to find the parent window from the grab
        PHLWINDOW parentWindow;
        if (oldGrab && oldGrab->m_surfs.size()) {
            // Try to find the surface that had focus when the grab ended
            SP<CWLSurfaceResource> focusedSurf;
            auto                   keyboardFocus = m_state.keyboardFocus.lock();
            auto                   pointerFocus  = m_state.pointerFocus.lock();

            // Check if keyboard or pointer focus is in the grab
            for (auto const& s : oldGrab->m_surfs) {
                auto surf = s.lock();
                if (surf && (surf == keyboardFocus || surf == pointerFocus)) {
                    focusedSurf = surf;
                    break;
                }
            }

            // Fall back to first surface if no focused surface found
            if (!focusedSurf)
                focusedSurf = oldGrab->m_surfs.front().lock();

            if (focusedSurf) {
                auto hlSurface = CWLSurface::fromResource(focusedSurf);
                if (hlSurface) {
                    auto popup = hlSurface->getPopup();
                    if (popup) {
                        auto t1Owner = popup->getT1Owner();
                        if (t1Owner)
                            parentWindow = t1Owner->getWindow();
                    }
                }
            }
        }

        m_seatGrab.reset();

        static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
        if (*PFOLLOWMOUSE == 0 || *PFOLLOWMOUSE == 2 || *PFOLLOWMOUSE == 3) {
            const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

            // If this was a popup grab, focus its parent window to maintain context
            if (validMapped(parentWindow)) {
                Desktop::focusState()->rawWindowFocus(parentWindow);
                Debug::log(LOG, "[seatmgr] Refocused popup parent window {} (follow_mouse={})", parentWindow->m_title, *PFOLLOWMOUSE);
            } else
                g_pInputManager->refocusLastWindow(PMONITOR);
        } else
            g_pInputManager->refocus();

        auto           currentFocus = m_state.keyboardFocus.lock();
        auto           refocus      = !currentFocus;

        SP<CWLSurface> surf;
        PHLLS          layer;

        if (!refocus) {
            surf  = CWLSurface::fromResource(currentFocus);
            layer = surf ? surf->getLayer() : nullptr;
        }

        if (!refocus && !layer) {
            auto popup = surf ? surf->getPopup() : nullptr;
            if (popup) {
                auto parent = popup->getT1Owner();
                layer       = parent->getLayer();
            }
        }

        if (!refocus && layer)
            refocus = layer->m_interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;

        if (refocus) {
            auto candidate = Desktop::focusState()->window();

            if (candidate)
                Desktop::focusState()->rawWindowFocus(candidate);
        }

        if (oldGrab->m_onEnd)
            oldGrab->m_onEnd();
    }

    if (!grab)
        return;

    m_seatGrab = grab;

    refocusGrab();
}

void CSeatManager::resendEnterEvents() {
    SP<CWLSurfaceResource> kb = m_state.keyboardFocus.lock();
    SP<CWLSurfaceResource> pt = m_state.pointerFocus.lock();

    auto                   last = m_lastLocalCoords;

    setKeyboardFocus(nullptr);
    setPointerFocus(nullptr, {});

    setKeyboardFocus(kb);
    setPointerFocus(pt, last);
}

bool CSeatGrab::accepts(SP<CWLSurfaceResource> surf) {
    return std::ranges::find(m_surfs, surf) != m_surfs.end();
}

void CSeatGrab::add(SP<CWLSurfaceResource> surf) {
    m_surfs.emplace_back(surf);
}

void CSeatGrab::remove(SP<CWLSurfaceResource> surf) {
    std::erase(m_surfs, surf);
    if ((m_keyboard && g_pSeatManager->m_state.keyboardFocus == surf) || (m_pointer && g_pSeatManager->m_state.pointerFocus == surf))
        g_pSeatManager->refocusGrab();
}

void CSeatGrab::setCallback(std::function<void()> onEnd_) {
    m_onEnd = onEnd_;
}

void CSeatGrab::clear() {
    m_surfs.clear();
}
