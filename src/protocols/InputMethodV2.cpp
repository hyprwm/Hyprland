#include "InputMethodV2.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../devices/IKeyboard.hpp"
#include <sys/mman.h>
#include "core/Compositor.hpp"
#include <cstring>

CInputMethodKeyboardGrabV2::CInputMethodKeyboardGrabV2(SP<CZwpInputMethodKeyboardGrabV2> resource_, SP<CInputMethodV2> owner_) : resource(resource_), owner(owner_) {
    if (!resource->resource())
        return;

    resource->setRelease([this](CZwpInputMethodKeyboardGrabV2* r) { PROTO::ime->destroyResource(this); });
    resource->setOnDestroy([this](CZwpInputMethodKeyboardGrabV2* r) { PROTO::ime->destroyResource(this); });

    if (!g_pSeatManager->keyboard) {
        LOGM(ERR, "IME called but no active keyboard???");
        return;
    }

    sendKeyboardData(g_pSeatManager->keyboard.lock());
}

CInputMethodKeyboardGrabV2::~CInputMethodKeyboardGrabV2() {
    if (!owner.expired())
        std::erase_if(owner->grabs, [](const auto& g) { return g.expired(); });
}

void CInputMethodKeyboardGrabV2::sendKeyboardData(SP<IKeyboard> keyboard) {

    if (keyboard == pLastKeyboard)
        return;

    pLastKeyboard = keyboard;

    int keymapFD = allocateSHMFile(keyboard->xkbKeymapString.length() + 1);
    if (keymapFD < 0) {
        LOGM(ERR, "Failed to create a keymap file for keyboard grab");
        return;
    }

    void* data = mmap(nullptr, keyboard->xkbKeymapString.length() + 1, PROT_READ | PROT_WRITE, MAP_SHARED, keymapFD, 0);
    if (data == MAP_FAILED) {
        LOGM(ERR, "Failed to mmap a keymap file for keyboard grab");
        close(keymapFD);
        return;
    }

    memcpy(data, keyboard->xkbKeymapString.c_str(), keyboard->xkbKeymapString.length());
    munmap(data, keyboard->xkbKeymapString.length() + 1);

    resource->sendKeymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymapFD, keyboard->xkbKeymapString.length() + 1);

    close(keymapFD);

    sendMods(keyboard->modifiersState.depressed, keyboard->modifiersState.latched, keyboard->modifiersState.locked, keyboard->modifiersState.group);

    resource->sendRepeatInfo(keyboard->repeatRate, keyboard->repeatDelay);
}

void CInputMethodKeyboardGrabV2::sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state) {
    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(resource->client()));

    resource->sendKey(SERIAL, time, key, (uint32_t)state);
}

void CInputMethodKeyboardGrabV2::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(resource->client()));

    resource->sendModifiers(SERIAL, depressed, latched, locked, group);
}

bool CInputMethodKeyboardGrabV2::good() {
    return resource->resource();
}

SP<CInputMethodV2> CInputMethodKeyboardGrabV2::getOwner() {
    return owner.lock();
}

wl_client* CInputMethodKeyboardGrabV2::client() {
    return resource->resource() ? resource->client() : nullptr;
}

CInputMethodPopupV2::CInputMethodPopupV2(SP<CZwpInputPopupSurfaceV2> resource_, SP<CInputMethodV2> owner_, SP<CWLSurfaceResource> surface) : resource(resource_), owner(owner_) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CZwpInputPopupSurfaceV2* r) { PROTO::ime->destroyResource(this); });
    resource->setOnDestroy([this](CZwpInputPopupSurfaceV2* r) { PROTO::ime->destroyResource(this); });

    pSurface = surface;

    listeners.destroySurface = surface->events.destroy.registerListener([this](std::any d) {
        if (mapped)
            events.unmap.emit();

        listeners.destroySurface.reset();
        listeners.commitSurface.reset();

        if (g_pCompositor->m_pLastFocus == pSurface)
            g_pCompositor->m_pLastFocus.reset();

        pSurface.reset();
    });

    listeners.commitSurface = surface->events.commit.registerListener([this](std::any d) {
        if (pSurface->current.texture && !mapped) {
            mapped = true;
            pSurface->map();
            events.map.emit();
            return;
        }

        if (!pSurface->current.texture && mapped) {
            mapped = false;
            pSurface->unmap();
            events.unmap.emit();
            return;
        }

        events.commit.emit();
    });
}

CInputMethodPopupV2::~CInputMethodPopupV2() {
    if (!owner.expired())
        std::erase_if(owner->popups, [](const auto& p) { return p.expired(); });

    events.destroy.emit();
}

bool CInputMethodPopupV2::good() {
    return resource->resource();
}

void CInputMethodPopupV2::sendInputRectangle(const CBox& box) {
    resource->sendTextInputRectangle(box.x, box.y, box.w, box.h);
}

SP<CWLSurfaceResource> CInputMethodPopupV2::surface() {
    return pSurface.lock();
}

void CInputMethodV2::SState::reset() {
    committedString.committed   = false;
    deleteSurrounding.committed = false;
    preeditString.committed     = false;
}

CInputMethodV2::CInputMethodV2(SP<CZwpInputMethodV2> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CZwpInputMethodV2* r) {
        events.destroy.emit();
        PROTO::ime->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwpInputMethodV2* r) {
        events.destroy.emit();
        PROTO::ime->destroyResource(this);
    });

    resource->setCommitString([this](CZwpInputMethodV2* r, const char* str) {
        pending.committedString.string    = str;
        pending.committedString.committed = true;
    });

    resource->setDeleteSurroundingText([this](CZwpInputMethodV2* r, uint32_t before, uint32_t after) {
        pending.deleteSurrounding.before    = before;
        pending.deleteSurrounding.after     = after;
        pending.deleteSurrounding.committed = true;
    });

    resource->setSetPreeditString([this](CZwpInputMethodV2* r, const char* str, int32_t begin, int32_t end) {
        pending.preeditString.string    = str;
        pending.preeditString.begin     = begin;
        pending.preeditString.end       = end;
        pending.preeditString.committed = true;
    });

    resource->setCommit([this](CZwpInputMethodV2* r, uint32_t serial) {
        current = pending;
        pending.reset();
        events.onCommit.emit();
    });

    resource->setGetInputPopupSurface([this](CZwpInputMethodV2* r, uint32_t id, wl_resource* surface) {
        const auto RESOURCE = PROTO::ime->m_vPopups.emplace_back(
            makeShared<CInputMethodPopupV2>(makeShared<CZwpInputPopupSurfaceV2>(r->client(), r->version(), id), self.lock(), CWLSurfaceResource::fromResource(surface)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::ime->m_vPopups.pop_back();
            return;
        }

        LOGM(LOG, "New IME Popup with resource id {}", id);

        popups.emplace_back(RESOURCE);

        events.newPopup.emit(RESOURCE);
    });

    resource->setGrabKeyboard([this](CZwpInputMethodV2* r, uint32_t id) {
        const auto RESOURCE =
            PROTO::ime->m_vGrabs.emplace_back(makeShared<CInputMethodKeyboardGrabV2>(makeShared<CZwpInputMethodKeyboardGrabV2>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::ime->m_vGrabs.pop_back();
            return;
        }

        LOGM(LOG, "New IME Grab with resource id {}", id);

        grabs.emplace_back(RESOURCE);
    });
}

CInputMethodV2::~CInputMethodV2() {
    events.destroy.emit();
}

bool CInputMethodV2::good() {
    return resource->resource();
}

void CInputMethodV2::activate() {
    if (active)
        return;
    resource->sendActivate();
    active = true;
}

void CInputMethodV2::deactivate() {
    if (!active)
        return;
    resource->sendDeactivate();
    active = false;
}

void CInputMethodV2::surroundingText(const std::string& text, uint32_t cursor, uint32_t anchor) {
    resource->sendSurroundingText(text.c_str(), cursor, anchor);
}

void CInputMethodV2::textChangeCause(zwpTextInputV3ChangeCause changeCause) {
    resource->sendTextChangeCause((uint32_t)changeCause);
}

void CInputMethodV2::textContentType(zwpTextInputV3ContentHint hint, zwpTextInputV3ContentPurpose purpose) {
    resource->sendContentType((uint32_t)hint, (uint32_t)purpose);
}

void CInputMethodV2::done() {
    resource->sendDone();
}

void CInputMethodV2::unavailable() {
    resource->sendUnavailable();
}

bool CInputMethodV2::hasGrab() {
    return !grabs.empty();
}

wl_client* CInputMethodV2::grabClient() {
    if (grabs.empty())
        return nullptr;

    for (auto& gw : grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        return g->client();
    }

    return nullptr;
}

void CInputMethodV2::sendInputRectangle(const CBox& box) {
    inputRectangle = box;
    for (auto& wp : popups) {
        auto p = wp.lock();

        if (!p)
            continue;

        p->sendInputRectangle(inputRectangle);
    }
}

void CInputMethodV2::sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state) {
    for (auto& gw : grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendKey(time, key, state);
    }
}

void CInputMethodV2::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    for (auto& gw : grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendMods(depressed, latched, locked, group);
    }
}

void CInputMethodV2::setKeyboard(SP<IKeyboard> keyboard) {
    for (auto& gw : grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendKeyboardData(keyboard);
    }
}

wl_client* CInputMethodV2::client() {
    return resource->client();
}

CInputMethodV2Protocol::CInputMethodV2Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CInputMethodV2Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpInputMethodManagerV2>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpInputMethodManagerV2* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpInputMethodManagerV2* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetInputMethod([this](CZwpInputMethodManagerV2* pMgr, wl_resource* seat, uint32_t id) { this->onGetIME(pMgr, seat, id); });
}

void CInputMethodV2Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodPopupV2* popup) {
    std::erase_if(m_vPopups, [&](const auto& other) { return other.get() == popup; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodKeyboardGrabV2* grab) {
    std::erase_if(m_vGrabs, [&](const auto& other) { return other.get() == grab; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodV2* ime) {
    std::erase_if(m_vIMEs, [&](const auto& other) { return other.get() == ime; });
}

void CInputMethodV2Protocol::onGetIME(CZwpInputMethodManagerV2* mgr, wl_resource* seat, uint32_t id) {
    const auto RESOURCE = m_vIMEs.emplace_back(makeShared<CInputMethodV2>(makeShared<CZwpInputMethodV2>(mgr->client(), mgr->version(), id)));

    if (!RESOURCE->good()) {
        mgr->noMemory();
        m_vIMEs.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;

    LOGM(LOG, "New IME with resource id {}", id);

    events.newIME.emit(RESOURCE);
}
