#include "Hotkey.hpp"
#include "../hotkeys/Manager.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>

static vicinaeHotkeyV1DenyReason denyReason(Hotkeys::eDenyReason reason) {
    switch (reason) {
        case Hotkeys::eDenyReason::ALREADY_BOUND: return VICINAE_HOTKEY_V1_DENY_REASON_ALREADY_BOUND;
        case Hotkeys::eDenyReason::NOT_PERMITTED: return VICINAE_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
    }
    return VICINAE_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
}

CVicinaeHotkeyManager::CVicinaeHotkeyManager(SP<CVicinaeHotkeyManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CVicinaeHotkeyManagerV1*) { PROTO::hotkey->destroyManager(this); });
    m_resource->setDestroy([this](CVicinaeHotkeyManagerV1*) { PROTO::hotkey->destroyManager(this); });

    m_resource->setBind([this](CVicinaeHotkeyManagerV1* mgr, uint32_t id, uint32_t keysym, vicinaeHotkeyManagerV1Modifiers mods, wl_resource* seat, const char* appid,
                               const char* description) { PROTO::hotkey->onBind(m_resource, id, keysym, sc<uint32_t>(mods), appid, description); });
}

bool CVicinaeHotkeyManager::good() {
    return m_resource->resource();
}

CVicinaeHotkey::CVicinaeHotkey(SP<CVicinaeHotkeyV1> resource, const Hotkeys::STrigger& trigger, const std::string& appId, const std::string& description) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CVicinaeHotkeyV1*) { PROTO::hotkey->destroyHotkey(this); });
    m_resource->setDestroy([this](CVicinaeHotkeyV1*) { PROTO::hotkey->destroyHotkey(this); });

    if (trigger.code == XKB_KEY_NoSymbol) {
        m_resource->sendDenied(VICINAE_HOTKEY_V1_DENY_REASON_INVALID, "the keysym is not a valid trigger");
        return;
    }

    m_hotkey = Hotkeys::mgr()->create();

    m_listeners.triggered = m_hotkey->m_events.triggered.listen([this](uint32_t serial, uint32_t timeMs) { m_resource->sendPressed(serial, timeMs); });
    m_listeners.released  = m_hotkey->m_events.released.listen([this](uint32_t serial, uint32_t timeMs) { m_resource->sendReleased(serial, timeMs); });
    m_listeners.revoked =
        m_hotkey->m_events.revoked.listen([this](const std::string& message) { m_resource->sendRevoked(VICINAE_HOTKEY_V1_REVOKE_REASON_SUPERSEDED, message.c_str()); });

    const auto RESULT = m_hotkey->commit(trigger, appId, description);
    if (RESULT.denied) {
        m_resource->sendDenied(denyReason(*RESULT.denied), RESULT.message.c_str());
        return;
    }

    m_resource->sendBound();
}

bool CVicinaeHotkey::good() {
    return m_resource->resource();
}

CHotkeyProtocol::CHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CHotkeyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CVicinaeHotkeyManager>(makeShared<CVicinaeHotkeyManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CHotkeyProtocol::destroyManager(CVicinaeHotkeyManager* mgr) {
    // hotkeys outlive their manager
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == mgr; });
}

void CHotkeyProtocol::destroyHotkey(CVicinaeHotkey* hk) {
    std::erase_if(m_hotkeys, [&](const auto& other) { return other.get() == hk; });
}

void CHotkeyProtocol::onBind(SP<CVicinaeHotkeyManagerV1> mgr, uint32_t id, uint32_t keysym, uint32_t protoMods, const char* appid, const char* description) {
    const Hotkeys::STrigger TRIGGER = {
        .type = Hotkeys::eTriggerType::KEY,
        .code = keysym,
        .mods = Hotkeys::modsFromProtocol(protoMods),
    };

    const auto RESOURCE = m_hotkeys.emplace_back(
        makeShared<CVicinaeHotkey>(makeShared<CVicinaeHotkeyV1>(mgr->client(), mgr->version(), id), TRIGGER, appid ? appid : "", description ? description : ""));

    if UNLIKELY (!RESOURCE->good()) {
        mgr->noMemory();
        m_hotkeys.pop_back();
        return;
    }
}
