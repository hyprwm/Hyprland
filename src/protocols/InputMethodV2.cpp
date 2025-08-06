#include "InputMethodV2.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../devices/IKeyboard.hpp"
#include <sys/mman.h>
#include "core/Compositor.hpp"
#include <cstring>

CInputMethodKeyboardGrabV2::CInputMethodKeyboardGrabV2(SP<CZwpInputMethodKeyboardGrabV2> resource_, SP<CInputMethodV2> owner_) : m_resource(resource_), m_owner(owner_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setRelease([this](CZwpInputMethodKeyboardGrabV2* r) { PROTO::ime->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpInputMethodKeyboardGrabV2* r) { PROTO::ime->destroyResource(this); });

    if (!g_pSeatManager->m_keyboard) {
        LOGM(ERR, "IME called but no active keyboard???");
        return;
    }

    sendKeyboardData(g_pSeatManager->m_keyboard.lock());
}

CInputMethodKeyboardGrabV2::~CInputMethodKeyboardGrabV2() {
    if (!m_owner.expired())
        std::erase_if(m_owner->m_grabs, [](const auto& g) { return g.expired(); });
}

void CInputMethodKeyboardGrabV2::sendKeyboardData(SP<IKeyboard> keyboard) {

    if (keyboard == m_lastKeyboard)
        return;

    m_lastKeyboard = keyboard;

    auto keymapFD = allocateSHMFile(keyboard->m_xkbKeymapString.length() + 1);
    if UNLIKELY (!keymapFD.isValid()) {
        LOGM(ERR, "Failed to create a keymap file for keyboard grab");
        return;
    }

    void* data = mmap(nullptr, keyboard->m_xkbKeymapString.length() + 1, PROT_READ | PROT_WRITE, MAP_SHARED, keymapFD.get(), 0);
    if UNLIKELY (data == MAP_FAILED) {
        LOGM(ERR, "Failed to mmap a keymap file for keyboard grab");
        return;
    }

    memcpy(data, keyboard->m_xkbKeymapString.c_str(), keyboard->m_xkbKeymapString.length());
    munmap(data, keyboard->m_xkbKeymapString.length() + 1);

    m_resource->sendKeymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymapFD.get(), keyboard->m_xkbKeymapString.length() + 1);

    sendMods(keyboard->m_modifiersState.depressed, keyboard->m_modifiersState.latched, keyboard->m_modifiersState.locked, keyboard->m_modifiersState.group);

    m_resource->sendRepeatInfo(keyboard->m_repeatRate, keyboard->m_repeatDelay);
}

void CInputMethodKeyboardGrabV2::sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state) {
    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(m_resource->client()));

    m_resource->sendKey(SERIAL, time, key, static_cast<uint32_t>(state));
}

void CInputMethodKeyboardGrabV2::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(m_resource->client()));

    m_resource->sendModifiers(SERIAL, depressed, latched, locked, group);
}

bool CInputMethodKeyboardGrabV2::good() {
    return m_resource->resource();
}

SP<CInputMethodV2> CInputMethodKeyboardGrabV2::getOwner() {
    return m_owner.lock();
}

wl_client* CInputMethodKeyboardGrabV2::client() {
    return m_resource->resource() ? m_resource->client() : nullptr;
}

CInputMethodPopupV2::CInputMethodPopupV2(SP<CZwpInputPopupSurfaceV2> resource_, SP<CInputMethodV2> owner_, SP<CWLSurfaceResource> surface) :
    m_resource(resource_), m_owner(owner_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CZwpInputPopupSurfaceV2* r) { PROTO::ime->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwpInputPopupSurfaceV2* r) { PROTO::ime->destroyResource(this); });

    m_surface = surface;

    m_listeners.destroySurface = surface->m_events.destroy.listen([this] {
        if (m_mapped)
            m_events.unmap.emit();

        m_listeners.destroySurface.reset();
        m_listeners.commitSurface.reset();

        if (g_pCompositor->m_lastFocus == m_surface)
            g_pCompositor->m_lastFocus.reset();

        m_surface.reset();
    });

    m_listeners.commitSurface = surface->m_events.commit.listen([this] {
        if (m_surface->m_current.texture && !m_mapped) {
            m_mapped = true;
            m_surface->map();
            m_events.map.emit();
            return;
        }

        if (!m_surface->m_current.texture && m_mapped) {
            m_mapped = false;
            m_surface->unmap();
            m_events.unmap.emit();
            return;
        }

        m_events.commit.emit();
    });
}

CInputMethodPopupV2::~CInputMethodPopupV2() {
    if (!m_owner.expired())
        std::erase_if(m_owner->m_popups, [](const auto& p) { return p.expired(); });

    m_events.destroy.emit();
}

bool CInputMethodPopupV2::good() {
    return m_resource->resource();
}

void CInputMethodPopupV2::sendInputRectangle(const CBox& box) {
    m_resource->sendTextInputRectangle(box.x, box.y, box.w, box.h);
}

SP<CWLSurfaceResource> CInputMethodPopupV2::surface() {
    return m_surface.lock();
}

void CInputMethodV2::SState::reset() {
    committedString.committed   = false;
    deleteSurrounding.committed = false;
    preeditString.committed     = false;
}

CInputMethodV2::CInputMethodV2(SP<CZwpInputMethodV2> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CZwpInputMethodV2* r) {
        m_events.destroy.emit();
        PROTO::ime->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CZwpInputMethodV2* r) {
        m_events.destroy.emit();
        PROTO::ime->destroyResource(this);
    });

    m_resource->setCommitString([this](CZwpInputMethodV2* r, const char* str) {
        m_pending.committedString.string    = str;
        m_pending.committedString.committed = true;
    });

    m_resource->setDeleteSurroundingText([this](CZwpInputMethodV2* r, uint32_t before, uint32_t after) {
        m_pending.deleteSurrounding.before    = before;
        m_pending.deleteSurrounding.after     = after;
        m_pending.deleteSurrounding.committed = true;
    });

    m_resource->setSetPreeditString([this](CZwpInputMethodV2* r, const char* str, int32_t begin, int32_t end) {
        m_pending.preeditString.string    = str;
        m_pending.preeditString.begin     = begin;
        m_pending.preeditString.end       = end;
        m_pending.preeditString.committed = true;
    });

    m_resource->setCommit([this](CZwpInputMethodV2* r, uint32_t serial) {
        m_current = m_pending;
        m_pending.reset();
        m_events.onCommit.emit();
    });

    m_resource->setGetInputPopupSurface([this](CZwpInputMethodV2* r, uint32_t id, wl_resource* surface) {
        const auto RESOURCE = PROTO::ime->m_popups.emplace_back(
            makeShared<CInputMethodPopupV2>(makeShared<CZwpInputPopupSurfaceV2>(r->client(), r->version(), id), m_self.lock(), CWLSurfaceResource::fromResource(surface)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::ime->m_popups.pop_back();
            return;
        }

        LOGM(LOG, "New IME Popup with resource id {}", id);

        m_popups.emplace_back(RESOURCE);

        m_events.newPopup.emit(RESOURCE);
    });

    m_resource->setGrabKeyboard([this](CZwpInputMethodV2* r, uint32_t id) {
        const auto RESOURCE =
            PROTO::ime->m_grabs.emplace_back(makeShared<CInputMethodKeyboardGrabV2>(makeShared<CZwpInputMethodKeyboardGrabV2>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::ime->m_grabs.pop_back();
            return;
        }

        LOGM(LOG, "New IME Grab with resource id {}", id);

        m_grabs.emplace_back(RESOURCE);
    });
}

CInputMethodV2::~CInputMethodV2() {
    m_events.destroy.emit();
}

bool CInputMethodV2::good() {
    return m_resource->resource();
}

void CInputMethodV2::activate() {
    if (m_active)
        return;
    m_resource->sendActivate();
    m_active = true;
}

void CInputMethodV2::deactivate() {
    if (!m_active)
        return;
    m_resource->sendDeactivate();
    m_active = false;
}

void CInputMethodV2::surroundingText(const std::string& text, uint32_t cursor, uint32_t anchor) {
    m_resource->sendSurroundingText(text.c_str(), cursor, anchor);
}

void CInputMethodV2::textChangeCause(zwpTextInputV3ChangeCause changeCause) {
    m_resource->sendTextChangeCause(changeCause);
}

void CInputMethodV2::textContentType(zwpTextInputV3ContentHint hint, zwpTextInputV3ContentPurpose purpose) {
    m_resource->sendContentType(hint, purpose);
}

void CInputMethodV2::done() {
    m_resource->sendDone();
}

void CInputMethodV2::unavailable() {
    m_resource->sendUnavailable();
}

bool CInputMethodV2::hasGrab() {
    return !m_grabs.empty();
}

wl_client* CInputMethodV2::grabClient() {
    if (m_grabs.empty())
        return nullptr;

    for (auto const& gw : m_grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        return g->client();
    }

    return nullptr;
}

void CInputMethodV2::sendInputRectangle(const CBox& box) {
    m_inputRectangle = box;
    for (auto const& wp : m_popups) {
        auto p = wp.lock();

        if (!p)
            continue;

        p->sendInputRectangle(m_inputRectangle);
    }
}

void CInputMethodV2::sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state) {
    for (auto const& gw : m_grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendKey(time, key, state);
    }
}

void CInputMethodV2::sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    for (auto const& gw : m_grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendMods(depressed, latched, locked, group);
    }
}

void CInputMethodV2::setKeyboard(SP<IKeyboard> keyboard) {
    for (auto const& gw : m_grabs) {
        auto g = gw.lock();

        if (!g)
            continue;

        g->sendKeyboardData(keyboard);
    }
}

wl_client* CInputMethodV2::client() {
    return m_resource->client();
}

CInputMethodV2Protocol::CInputMethodV2Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CInputMethodV2Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpInputMethodManagerV2>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpInputMethodManagerV2* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpInputMethodManagerV2* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetInputMethod([this](CZwpInputMethodManagerV2* pMgr, wl_resource* seat, uint32_t id) { this->onGetIME(pMgr, seat, id); });
}

void CInputMethodV2Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodPopupV2* popup) {
    std::erase_if(m_popups, [&](const auto& other) { return other.get() == popup; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodKeyboardGrabV2* grab) {
    std::erase_if(m_grabs, [&](const auto& other) { return other.get() == grab; });
}

void CInputMethodV2Protocol::destroyResource(CInputMethodV2* ime) {
    std::erase_if(m_imes, [&](const auto& other) { return other.get() == ime; });
}

void CInputMethodV2Protocol::onGetIME(CZwpInputMethodManagerV2* mgr, wl_resource* seat, uint32_t id) {
    const auto RESOURCE = m_imes.emplace_back(makeShared<CInputMethodV2>(makeShared<CZwpInputMethodV2>(mgr->client(), mgr->version(), id)));

    if UNLIKELY (!RESOURCE->good()) {
        mgr->noMemory();
        m_imes.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;

    LOGM(LOG, "New IME with resource id {}", id);

    m_events.newIME.emit(RESOURCE);
}
