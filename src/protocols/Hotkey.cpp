#include "Hotkey.hpp"
#include "../Compositor.hpp"
#include "../devices/IKeyboard.hpp"
#include "../managers/KeybindManager.hpp"
#include "../event/EventBus.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>
#include <format>

static constexpr uint32_t RELEVANT_MODS = HL_MODIFIER_SHIFT | HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;

static uint32_t           protoModsToHL(uint32_t mods) {
    uint32_t out = 0;
    if (mods & VICINAE_HOTKEY_MANAGER_V1_MODIFIERS_SHIFT)
        out |= HL_MODIFIER_SHIFT;
    if (mods & VICINAE_HOTKEY_MANAGER_V1_MODIFIERS_CTRL)
        out |= HL_MODIFIER_CTRL;
    if (mods & VICINAE_HOTKEY_MANAGER_V1_MODIFIERS_ALT)
        out |= HL_MODIFIER_ALT;
    if (mods & VICINAE_HOTKEY_MANAGER_V1_MODIFIERS_SUPER)
        out |= HL_MODIFIER_META;
    return out;
}

static bool isFunctionKey(xkb_keysym_t sym) {
    return sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35;
}

static bool isValidTrigger(xkb_keysym_t sym, uint32_t modmask) {
    if (isFunctionKey(sym))
        return true;
    return modmask & (HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META);
}

static std::string keybindLabel(const SP<SKeybind>& k) {
    if (!k->description.empty())
        return k->description;
    if (!k->arg.empty())
        return k->handler + ", " + k->arg;
    return k->handler;
}

CVicinaeHotkeyManager::CVicinaeHotkeyManager(SP<CVicinaeHotkeyManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CVicinaeHotkeyManagerV1*) { PROTO::hotkey->destroyManager(this); });
    m_resource->setDestroy([this](CVicinaeHotkeyManagerV1*) { PROTO::hotkey->destroyManager(this); });

    m_resource->setBind([this](CVicinaeHotkeyManagerV1* mgr, uint32_t id, uint32_t keysym, vicinaeHotkeyManagerV1Modifiers mods, wl_resource* seat, const char* appid,
                               const char* description) { PROTO::hotkey->onBind(m_resource, id, (xkb_keysym_t)keysym, (uint32_t)mods, appid, description); });
}

bool CVicinaeHotkeyManager::good() {
    return m_resource->resource();
}

CHotkeyProtocol::CHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    m_reloadListener = Event::bus()->m_events.config.reloaded.listen([this] { revokeConflicting(); });
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

bool CHotkeyProtocol::comboTakenByHotkey(xkb_keysym_t keysym, uint32_t modmask) {
    for (const auto& hk : m_hotkeys) {
        if (hk->bound && hk->keysym == keysym && hk->modmask == modmask)
            return true;
    }
    return false;
}

void CHotkeyProtocol::onBind(SP<CVicinaeHotkeyManagerV1> mgr, uint32_t id, xkb_keysym_t keysym, uint32_t protoMods, const char* appid, const char* description) {
    auto hk      = makeShared<SBoundHotkey>();
    hk->resource = makeShared<CVicinaeHotkeyV1>(mgr->client(), mgr->version(), id);

    if UNLIKELY (!hk->resource->resource()) {
        mgr->noMemory();
        return;
    }

    hk->keysym      = keysym;
    hk->modmask     = protoModsToHL(protoMods);
    hk->appid       = appid ? appid : "";
    hk->description = description ? description : "";

    SBoundHotkey* raw = hk.get();
    hk->resource->setDestroy([raw](CVicinaeHotkeyV1*) { std::erase_if(PROTO::hotkey->m_hotkeys, [&](const auto& other) { return other.get() == raw; }); });
    hk->resource->setOnDestroy([raw](CVicinaeHotkeyV1*) { std::erase_if(PROTO::hotkey->m_hotkeys, [&](const auto& other) { return other.get() == raw; }); });

    m_hotkeys.emplace_back(hk);

    if (keysym == XKB_KEY_NoSymbol) {
        hk->resource->sendDenied(VICINAE_HOTKEY_V1_DENY_REASON_INVALID, "the keysym is not a valid trigger");
        return;
    }

    if (!isValidTrigger(keysym, hk->modmask)) {
        hk->resource->sendDenied(VICINAE_HOTKEY_V1_DENY_REASON_NOT_PERMITTED, "a non-latching modifier (Ctrl, Alt or Super) is required unless the trigger is a function key");
        return;
    }

    if (comboTakenByHotkey(keysym, hk->modmask)) {
        hk->resource->sendDenied(VICINAE_HOTKEY_V1_DENY_REASON_ALREADY_BOUND, "the combination is already bound by another hotkey");
        return;
    }

    if (const auto CONFLICT = g_pKeybindManager->findConflictingKeybind(keysym, hk->modmask)) {
        hk->resource->sendDenied(VICINAE_HOTKEY_V1_DENY_REASON_ALREADY_BOUND,
                                 std::format("the combination is reserved by a compositor keybind ({})", keybindLabel(CONFLICT)).c_str());
        return;
    }

    hk->bound = true;
    hk->resource->sendBound();
}

bool CHotkeyProtocol::onKey(xkb_keysym_t keysym, uint32_t modmask, uint32_t keycode, bool pressed, uint32_t timeMs) {
    const uint32_t mods     = modmask & RELEVANT_MODS;
    bool           consumed = false;

    if (pressed) {
        for (const auto& hk : m_hotkeys) {
            if (!hk->bound || hk->held)
                continue;
            if (hk->keysym != keysym || hk->modmask != mods)
                continue;

            hk->held              = true;
            hk->heldCode          = keycode;
            const uint32_t serial = wl_display_next_serial(g_pCompositor->m_wlDisplay);
            hk->resource->sendPressed(serial, timeMs);
            consumed = true;
        }
    } else {
        for (const auto& hk : m_hotkeys) {
            if (!hk->held || hk->heldCode != keycode)
                continue;

            hk->held              = false;
            hk->heldCode          = 0;
            const uint32_t serial = wl_display_next_serial(g_pCompositor->m_wlDisplay);
            hk->resource->sendReleased(serial, timeMs);
            consumed = true;
        }
    }

    return consumed;
}

void CHotkeyProtocol::revokeConflicting() {
    for (const auto& hk : m_hotkeys) {
        if (!hk->bound)
            continue;
        const auto CONFLICT = g_pKeybindManager->findConflictingKeybind(hk->keysym, hk->modmask);
        if (!CONFLICT)
            continue;

        hk->bound    = false;
        hk->held     = false;
        hk->heldCode = 0;
        hk->resource->sendRevoked(VICINAE_HOTKEY_V1_REVOKE_REASON_SUPERSEDED,
                                  std::format("the combination is now reserved by a compositor keybind ({})", keybindLabel(CONFLICT)).c_str());
    }
}
