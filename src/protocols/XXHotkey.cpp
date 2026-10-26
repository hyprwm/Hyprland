#include "XXHotkey.hpp"
#include "../hotkeys/Manager.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>

static xxHotkeyV1DenyReason denyReason(Hotkeys::eDenyReason reason) {
    switch (reason) {
        case Hotkeys::eDenyReason::ALREADY_BOUND: return XX_HOTKEY_V1_DENY_REASON_ALREADY_BOUND;
        case Hotkeys::eDenyReason::NOT_PERMITTED: return XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
    }
    return XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
}

CXXHotkeyManagerResource::CXXHotkeyManagerResource(SP<CXxHotkeyManagerV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CXxHotkeyManagerV1*) { PROTO::xxHotkey->destroyManager(this); });
    m_resource->setDestroy([this](CXxHotkeyManagerV1*) { PROTO::xxHotkey->destroyManager(this); });

    m_resource->setSetAppId([this](CXxHotkeyManagerV1*, const char* appId) {
        if (!appId || !*appId) {
            m_resource->error(XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID, "app_id must not be empty");
            return;
        }

        if (m_appIdSet) {
            m_resource->error(XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID, "app_id was already set");
            return;
        }

        if (m_committed) {
            m_resource->error(XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID, "app_id must be set before the first commit");
            return;
        }

        m_appId    = appId;
        m_appIdSet = true;
    });

    m_resource->setCreateHotkey([this](CXxHotkeyManagerV1*, uint32_t id) { PROTO::xxHotkey->createHotkey(m_self, id); });
}

bool CXXHotkeyManagerResource::good() {
    return m_resource->resource();
}

CXXHotkeyResource::CXXHotkeyResource(SP<CXxHotkeyV1> resource, WP<CXXHotkeyManagerResource> manager) : m_resource(resource), m_manager(manager) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CXxHotkeyV1*) { PROTO::xxHotkey->destroyHotkey(this); });
    m_resource->setDestroy([this](CXxHotkeyV1*) { PROTO::xxHotkey->destroyHotkey(this); });

    m_resource->setSetDescription([this](CXxHotkeyV1*, const char* description) { m_pendingDescription = description ? description : ""; });

    m_resource->setSetSeat([](CXxHotkeyV1*, wl_resource*) { ; });

    m_resource->setSetKeyTrigger([this](CXxHotkeyV1*, uint32_t keysym, uint32_t mods) {
        if (keysym == XKB_KEY_NoSymbol) {
            m_resource->error(XX_HOTKEY_V1_ERROR_INVALID_TRIGGER, "keysym must not be null");
            return;
        }

        if (!Hotkeys::validProtocolMods(mods)) {
            m_resource->error(XX_HOTKEY_V1_ERROR_INVALID_TRIGGER, "unknown modifier bits");
            return;
        }

        m_pendingTrigger = Hotkeys::STrigger{
            .type = Hotkeys::eTriggerType::KEY,
            .code = keysym,
            .mods = Hotkeys::modsFromProtocol(mods),
        };
    });

    m_resource->setSetButtonTrigger([this](CXxHotkeyV1*, uint32_t button, uint32_t mods) {
        if (!Hotkeys::validProtocolMods(mods)) {
            m_resource->error(XX_HOTKEY_V1_ERROR_INVALID_TRIGGER, "unknown modifier bits");
            return;
        }

        m_pendingTrigger = Hotkeys::STrigger{
            .type = Hotkeys::eTriggerType::BUTTON,
            .code = button,
            .mods = Hotkeys::modsFromProtocol(mods),
        };
    });

    m_resource->setCommit([this](CXxHotkeyV1*) { commit(); });

    m_hotkey = Hotkeys::mgr()->create();

    m_listeners.triggered = m_hotkey->m_events.triggered.listen([this](uint32_t serial, uint32_t timeMs) { m_resource->sendTriggered(serial, timeMs); });
    m_listeners.released  = m_hotkey->m_events.released.listen([this](uint32_t serial, uint32_t timeMs) { m_resource->sendReleased(serial, timeMs); });
    m_listeners.revoked   = m_hotkey->m_events.revoked.listen([this](const std::string& message) { m_resource->sendRevoked(message.c_str()); });
}

bool CXXHotkeyResource::good() {
    return m_resource->resource();
}

void CXXHotkeyResource::commit() {
    if (!m_pendingTrigger) {
        m_resource->error(XX_HOTKEY_V1_ERROR_NO_TRIGGER, "commit sent before a trigger was described");
        return;
    }

    std::string appId;
    if (const auto MGR = m_manager.lock()) {
        MGR->m_committed = true;
        appId            = MGR->m_appId;
    } else
        appId = m_hotkey->appId();

    const auto RESULT = m_hotkey->commit(*m_pendingTrigger, appId, m_pendingDescription);
    if (RESULT.denied) {
        m_resource->sendDenied(denyReason(*RESULT.denied), RESULT.message.c_str());
        return;
    }

    m_resource->sendBound();
}

CXXHotkeyProtocol::CXXHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXXHotkeyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CXXHotkeyManagerResource>(makeShared<CXxHotkeyManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;
}

void CXXHotkeyProtocol::createHotkey(WP<CXXHotkeyManagerResource> mgr, uint32_t id) {
    const auto MGR      = mgr->m_resource;
    const auto RESOURCE = m_hotkeys.emplace_back(makeShared<CXXHotkeyResource>(makeShared<CXxHotkeyV1>(MGR->client(), MGR->version(), id), mgr));

    if UNLIKELY (!RESOURCE->good()) {
        MGR->noMemory();
        m_hotkeys.pop_back();
        return;
    }
}

void CXXHotkeyProtocol::destroyManager(CXXHotkeyManagerResource* mgr) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == mgr; });
}

void CXXHotkeyProtocol::destroyHotkey(CXXHotkeyResource* hk) {
    std::erase_if(m_hotkeys, [&](const auto& other) { return other.get() == hk; });
}
