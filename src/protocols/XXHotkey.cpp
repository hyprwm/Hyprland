#include "XXHotkey.hpp"
#include "../Compositor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/SessionLockManager.hpp"

#include <format>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

static constexpr Input::ModifierMask RELEVANT_MODS = Input::HL_MODIFIER_SHIFT | Input::HL_MODIFIER_CTRL | Input::HL_MODIFIER_ALT | Input::HL_MODIFIER_META;
static constexpr uint32_t            ALL_PROTO_MODS =
    XX_HOTKEY_MANAGER_V1_MODIFIERS_SHIFT | XX_HOTKEY_MANAGER_V1_MODIFIERS_CTRL | XX_HOTKEY_MANAGER_V1_MODIFIERS_ALT | XX_HOTKEY_MANAGER_V1_MODIFIERS_SUPER;

static Input::ModifierMask protoModsToHL(uint32_t mods) {
    Input::ModifierMask out = Input::HL_MODIFIER_NONE;
    if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_SHIFT)
        out |= Input::HL_MODIFIER_SHIFT;
    if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_CTRL)
        out |= Input::HL_MODIFIER_CTRL;
    if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_ALT)
        out |= Input::HL_MODIFIER_ALT;
    if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_SUPER)
        out |= Input::HL_MODIFIER_META;
    return out;
}

static bool isModifierKeysym(xkb_keysym_t sym) {
    switch (sym) {
        case XKB_KEY_ISO_Level3_Shift:
        case XKB_KEY_ISO_Level3_Latch:
        case XKB_KEY_ISO_Level3_Lock:
        case XKB_KEY_ISO_Level5_Shift:
        case XKB_KEY_ISO_Level5_Latch:
        case XKB_KEY_ISO_Level5_Lock:
        case XKB_KEY_Mode_switch:
        case XKB_KEY_Num_Lock:
        case XKB_KEY_Scroll_Lock: return true;
        default: break;
    }
    return sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R;
}

static bool isKeypadKeysym(xkb_keysym_t sym) {
    return sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal;
}

static std::string keysymName(xkb_keysym_t sym) {
    char name[64] = {0};
    if (xkb_keysym_get_name(sym, name, sizeof(name)) <= 0)
        return "";
    return name;
}

static bool isDeadKeysym(xkb_keysym_t sym) {
    return keysymName(sym).starts_with("dead_");
}

static bool isAuxiliaryButton(uint32_t button) {
    switch (button) {
        case BTN_SIDE:
        case BTN_EXTRA:
        case BTN_FORWARD:
        case BTN_BACK:
        case BTN_TASK: return true;
        default: break;
    }
    return false;
}

static bool keyProducesKeysym(SP<IKeyboard> keyboard, uint32_t keycode, xkb_keysym_t sym) {
    const auto KEYMAP = keyboard->m_xkbKeymap;
    if (!KEYMAP)
        return false;

    const auto LAYOUTS = xkb_keymap_num_layouts_for_key(KEYMAP, keycode);
    for (xkb_layout_index_t layout = 0; layout < LAYOUTS; ++layout) {
        const xkb_keysym_t* syms  = nullptr;
        const int           NSYMS = xkb_keymap_key_get_syms_by_level(KEYMAP, keycode, layout, 0, &syms);
        for (int i = 0; i < NSYMS; ++i) {
            if (syms[i] == sym)
                return true;
        }
    }

    return false;
}

static std::string modsLabel(Input::ModifierMask mods) {
    std::string out;
    if (mods & Input::HL_MODIFIER_META)
        out += "SUPER+";
    if (mods & Input::HL_MODIFIER_CTRL)
        out += "CTRL+";
    if (mods & Input::HL_MODIFIER_ALT)
        out += "ALT+";
    if (mods & Input::HL_MODIFIER_SHIFT)
        out += "SHIFT+";
    return out;
}

static uint32_t nextSerial() {
    return wl_display_next_serial(g_pCompositor->m_wlDisplay);
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

        if (mods & ~ALL_PROTO_MODS) {
            m_resource->error(XX_HOTKEY_V1_ERROR_INVALID_TRIGGER, "unknown modifier bits");
            return;
        }

        m_pendingTrigger = STrigger{
            .type = eTriggerType::TRIGGER_KEY,
            .code = xkb_keysym_to_lower(keysym),
            .mods = protoModsToHL(mods),
        };
    });

    m_resource->setSetButtonTrigger([this](CXxHotkeyV1*, uint32_t button, uint32_t mods) {
        if (mods & ~ALL_PROTO_MODS) {
            m_resource->error(XX_HOTKEY_V1_ERROR_INVALID_TRIGGER, "unknown modifier bits");
            return;
        }

        m_pendingTrigger = STrigger{
            .type = eTriggerType::TRIGGER_BUTTON,
            .code = button,
            .mods = protoModsToHL(mods),
        };
    });

    m_resource->setCommit([this](CXxHotkeyV1*) { commit(); });
}

bool CXXHotkeyResource::good() {
    return m_resource->resource();
}

bool CXXHotkeyResource::isTap() const {
    return m_trigger.type == eTriggerType::TRIGGER_KEY && isModifierKeysym(m_trigger.code);
}

void CXXHotkeyResource::resetHeld() {
    m_held       = false;
    m_tapPending = false;
    m_heldCode   = 0;
    m_heldDevice.reset();
}

bool CXXHotkeyResource::acceptable(const STrigger& trigger) {
    if (trigger.mods & (Input::HL_MODIFIER_CTRL | Input::HL_MODIFIER_ALT | Input::HL_MODIFIER_META))
        return true;

    if (trigger.type == eTriggerType::TRIGGER_BUTTON)
        return isAuxiliaryButton(trigger.code);

    if (isModifierKeysym(trigger.code))
        return true;
    if (isKeypadKeysym(trigger.code))
        return true;
    if (isDeadKeysym(trigger.code))
        return false;
    if (xkb_keysym_to_utf32(trigger.code) != 0)
        return false;
    return true;
}

void CXXHotkeyResource::commit() {
    if (!m_pendingTrigger) {
        m_resource->error(XX_HOTKEY_V1_ERROR_NO_TRIGGER, "commit sent before a trigger was described");
        return;
    }

    if (const auto MGR = m_manager.lock()) {
        MGR->m_committed = true;
        m_appId          = MGR->m_appId;
    }

    const auto& TRIGGER = *m_pendingTrigger;

    if (!acceptable(TRIGGER)) {
        const auto MESSAGE = TRIGGER.type == eTriggerType::TRIGGER_BUTTON ? "an unmodified button trigger must be an auxiliary button" :
                                                                            "an unmodified key trigger must not be a key that produces text";
        m_resource->sendDenied(XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED, MESSAGE);
        return;
    }

    m_trigger     = TRIGGER;
    m_description = m_pendingDescription;
    m_bound       = true;
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

void CXXHotkeyProtocol::cancelTaps(uint32_t exceptCode, const WP<IHID>& exceptDevice) {
    for (const auto& hk : m_hotkeys) {
        if (!hk->m_held || !hk->m_tapPending)
            continue;
        if (exceptCode && hk->m_heldCode == exceptCode && hk->m_heldDevice == exceptDevice)
            continue;

        hk->resetHeld();
    }
}

bool CXXHotkeyProtocol::onKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard) {
    if (!g_pCompositor->m_sessionActive || !keyboard->m_allowBinds)
        return false;

    const auto     KEYCODE  = event.keycode + 8;
    const bool     PRESSED  = event.state == WL_KEYBOARD_KEY_STATE_PRESSED;
    const WP<IHID> DEVICE   = keyboard;
    const auto     MODS     = g_pInputManager->getModsFromAllKBs() & RELEVANT_MODS;
    const bool     LOCKED   = g_pSessionLockManager->isSessionLocked();
    bool           consumed = false;

    cancelTaps(LOCKED ? 0 : KEYCODE, DEVICE);

    for (const auto& hk : m_hotkeys) {
        if (hk->m_held && hk->m_heldDevice.expired())
            hk->resetHeld();

        if (PRESSED) {
            if (LOCKED || !hk->m_bound || hk->m_held || hk->m_trigger.type != CXXHotkeyResource::eTriggerType::TRIGGER_KEY)
                continue;
            if (hk->m_trigger.mods != MODS || !keyProducesKeysym(keyboard, KEYCODE, hk->m_trigger.code))
                continue;

            hk->m_held       = true;
            hk->m_heldCode   = KEYCODE;
            hk->m_heldDevice = DEVICE;

            if (hk->isTap()) {
                hk->m_tapPending = true;
                continue;
            }

            hk->m_resource->sendTriggered(nextSerial(), event.timeMs);
            consumed = true;
            continue;
        }

        if (!hk->m_held || hk->m_heldCode != KEYCODE || hk->m_heldDevice != DEVICE)
            continue;

        const bool TAP = hk->m_tapPending;
        hk->resetHeld();

        if (TAP)
            hk->m_resource->sendTriggered(nextSerial(), event.timeMs);
        hk->m_resource->sendReleased(nextSerial(), event.timeMs);
        consumed = consumed || !TAP;
    }

    return consumed;
}

bool CXXHotkeyProtocol::onButton(const IPointer::SButtonEvent& event, SP<IPointer> pointer) {
    if (!g_pCompositor->m_sessionActive)
        return false;

    const bool     PRESSED  = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
    const WP<IHID> DEVICE   = pointer;
    const auto     MODS     = g_pInputManager->getModsFromAllKBs() & RELEVANT_MODS;
    const bool     LOCKED   = g_pSessionLockManager->isSessionLocked();
    bool           consumed = false;

    cancelTaps();

    for (const auto& hk : m_hotkeys) {
        if (hk->m_held && hk->m_heldDevice.expired())
            hk->resetHeld();

        if (PRESSED) {
            if (LOCKED || !hk->m_bound || hk->m_held || hk->m_trigger.type != CXXHotkeyResource::eTriggerType::TRIGGER_BUTTON)
                continue;
            if (hk->m_trigger.mods != MODS || hk->m_trigger.code != event.button)
                continue;

            hk->m_held       = true;
            hk->m_heldCode   = event.button;
            hk->m_heldDevice = DEVICE;
            hk->m_resource->sendTriggered(nextSerial(), event.timeMs);
            consumed = true;
            continue;
        }

        if (!hk->m_held || hk->m_heldCode != event.button || hk->m_heldDevice != DEVICE)
            continue;

        hk->resetHeld();
        hk->m_resource->sendReleased(nextSerial(), event.timeMs);
        consumed = true;
    }

    return consumed;
}

void CXXHotkeyProtocol::onAxis() {
    cancelTaps();
}

std::vector<CXXHotkeyProtocol::SHotkeyInfo> CXXHotkeyProtocol::getAllShortcuts() {
    std::vector<SHotkeyInfo> out;
    out.reserve(m_hotkeys.size());

    for (const auto& hk : m_hotkeys) {
        if (!hk->m_bound)
            continue;

        std::string trigger = modsLabel(hk->m_trigger.mods);
        if (hk->m_trigger.type == CXXHotkeyResource::eTriggerType::TRIGGER_BUTTON)
            trigger += std::format("mouse:{}", hk->m_trigger.code);
        else
            trigger += keysymName(hk->m_trigger.code);

        out.emplace_back(SHotkeyInfo{
            .appId       = hk->m_appId,
            .trigger     = trigger,
            .description = hk->m_description,
        });
    }

    return out;
}
