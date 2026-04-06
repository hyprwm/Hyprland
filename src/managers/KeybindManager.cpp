#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../config/legacy/DispatcherTranslator.hpp"
#include "../config/shared/actions/ConfigActions.hpp"
#include "../devices/IKeyboard.hpp"
#include "../managers/SeatManager.hpp"
#include "../protocols/ShortcutsInhibit.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../hyprerror/HyprError.hpp"
#include "KeybindManager.hpp"
#include "PointerManager.hpp"
#include "Compositor.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "debug/log/Logger.hpp"
#include "../managers/input/InputManager.hpp"
#include "../layout/LayoutManager.hpp"
#include "../event/EventBus.hpp"

#include <string>
#include <cstring>

#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

#include <sys/ioctl.h>
#include <fcntl.h>
#include <vector>
#if defined(__linux__)
#include <linux/vt.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/wscons/wsdisplay_usl_io.h>
#elif defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/consio.h>
#endif

CKeybindManager::CKeybindManager() {
    // initialize all dispatchers
    // populate m_dispatchers from the legacy translator
    for (const auto& name : {"exec",
                             "execr",
                             "killactive",
                             "forcekillactive",
                             "closewindow",
                             "killwindow",
                             "signal",
                             "signalwindow",
                             "togglefloating",
                             "setfloating",
                             "settiled",
                             "workspace",
                             "renameworkspace",
                             "fullscreen",
                             "fullscreenstate",
                             "movetoworkspace",
                             "movetoworkspacesilent",
                             "pseudo",
                             "movefocus",
                             "movewindow",
                             "swapwindow",
                             "centerwindow",
                             "togglegroup",
                             "changegroupactive",
                             "movegroupwindow",
                             "focusmonitor",
                             "movecursortocorner",
                             "movecursor",
                             "workspaceopt",
                             "exit",
                             "movecurrentworkspacetomonitor",
                             "focusworkspaceoncurrentmonitor",
                             "moveworkspacetomonitor",
                             "togglespecialworkspace",
                             "forcerendererreload",
                             "resizeactive",
                             "moveactive",
                             "cyclenext",
                             "focuswindowbyclass",
                             "focuswindow",
                             "tagwindow",
                             "toggleswallow",
                             "submap",
                             "pass",
                             "sendshortcut",
                             "sendkeystate",
                             "layoutmsg",
                             "dpms",
                             "movewindowpixel",
                             "resizewindowpixel",
                             "swapnext",
                             "swapactiveworkspaces",
                             "pin",
                             "mouse",
                             "bringactivetotop",
                             "alterzorder",
                             "focusurgentorlast",
                             "focuscurrentorlast",
                             "lockgroups",
                             "lockactivegroup",
                             "moveintogroup",
                             "moveoutofgroup",
                             "movewindoworgroup",
                             "moveintoorcreategroup",
                             "setignoregrouplock",
                             "denywindowfromgroup",
                             "event",
                             "global",
                             "setprop",
                             "forceidle"}) {
        m_dispatchers[name] = [n = std::string(name)](std::string args) -> SDispatchResult { return Config::Legacy::translator()->run(n, args); };
    }

    m_scrollTimer.reset();

    m_longPressTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            if (!m_lastLongPressKeybind || g_pSeatManager->m_keyboard.expired())
                return;

            const auto PACTIVEKEEB = g_pSeatManager->m_keyboard.lock();
            if (!PACTIVEKEEB->m_allowBinds)
                return;

            const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(m_lastLongPressKeybind->handler);

            Log::logger->log(Log::DEBUG, "Long press timeout passed, calling dispatcher.");
            DISPATCHER->second(m_lastLongPressKeybind->arg);
        },
        nullptr);

    m_repeatKeyTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            if (m_activeKeybinds.empty() || g_pSeatManager->m_keyboard.expired())
                return;

            const auto PACTIVEKEEB = g_pSeatManager->m_keyboard.lock();
            if (!PACTIVEKEEB->m_allowBinds)
                return;

            for (const auto& k : m_activeKeybinds) {
                const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(k->handler);

                Log::logger->log(Log::DEBUG, "Keybind repeat triggered, calling dispatcher.");
                DISPATCHER->second(k->arg);
            }

            self->updateTimeout(std::chrono::milliseconds(1000 / m_repeatKeyRate));
        },
        nullptr);

    // null in --verify-config mode
    if (g_pEventLoopManager) {
        g_pEventLoopManager->addTimer(m_longPressTimer);
        g_pEventLoopManager->addTimer(m_repeatKeyTimer);
    }

    static auto P = Event::bus()->m_events.config.reloaded.listen([this] {
        m_activeKeybinds.clear();
        m_lastLongPressKeybind.reset();
        m_pressedSpecialBinds.clear();
    });
}

CKeybindManager::~CKeybindManager() {
    if (m_xkbTranslationState)
        xkb_state_unref(m_xkbTranslationState);
    if (m_longPressTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_longPressTimer);
        m_longPressTimer.reset();
    }
    if (m_repeatKeyTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_repeatKeyTimer);
        m_repeatKeyTimer.reset();
    }
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_keybinds.emplace_back(makeShared<SKeybind>(kb));

    m_activeKeybinds.clear();
    m_lastLongPressKeybind.reset();
}

void CKeybindManager::removeKeybind(uint32_t mod, const SParsedKey& key) {
    std::erase_if(m_keybinds, [&mod, &key](const auto& el) { return el->modmask == mod && el->key == key.key && el->keycode == key.keycode && el->catchAll == key.catchAll; });

    m_activeKeybinds.clear();
    m_lastLongPressKeybind.reset();
}

uint32_t CKeybindManager::stringToModMask(std::string mods) {
    uint32_t modMask = 0;
    std::ranges::transform(mods, mods.begin(), ::toupper);
    if (mods.contains("SHIFT"))
        modMask |= HL_MODIFIER_SHIFT;
    if (mods.contains("CAPS"))
        modMask |= HL_MODIFIER_CAPS;
    if (mods.contains("CTRL") || mods.contains("CONTROL"))
        modMask |= HL_MODIFIER_CTRL;
    if (mods.contains("ALT") || mods.contains("MOD1"))
        modMask |= HL_MODIFIER_ALT;
    if (mods.contains("MOD2"))
        modMask |= HL_MODIFIER_MOD2;
    if (mods.contains("MOD3"))
        modMask |= HL_MODIFIER_MOD3;
    if (mods.contains("SUPER") || mods.contains("WIN") || mods.contains("LOGO") || mods.contains("MOD4") || mods.contains("META"))
        modMask |= HL_MODIFIER_META;
    if (mods.contains("MOD5"))
        modMask |= HL_MODIFIER_MOD5;

    return modMask;
}

uint32_t CKeybindManager::keycodeToModifier(xkb_keycode_t keycode) {
    if (keycode == 0)
        return 0;

    switch (keycode - 8) {
        case KEY_LEFTMETA: return HL_MODIFIER_META;
        case KEY_RIGHTMETA: return HL_MODIFIER_META;
        case KEY_LEFTSHIFT: return HL_MODIFIER_SHIFT;
        case KEY_RIGHTSHIFT: return HL_MODIFIER_SHIFT;
        case KEY_LEFTCTRL: return HL_MODIFIER_CTRL;
        case KEY_RIGHTCTRL: return HL_MODIFIER_CTRL;
        case KEY_LEFTALT: return HL_MODIFIER_ALT;
        case KEY_RIGHTALT: return HL_MODIFIER_ALT;
        case KEY_CAPSLOCK: return HL_MODIFIER_CAPS;
        case KEY_NUMLOCK: return HL_MODIFIER_MOD2;
        default: return 0;
    }
}

void CKeybindManager::updateXKBTranslationState() {
    if (m_xkbTranslationState) {
        xkb_state_unref(m_xkbTranslationState);

        m_xkbTranslationState = nullptr;
    }

    static auto       PFILEPATH = CConfigValue<std::string>("input:kb_file");
    static auto       PRULES    = CConfigValue<std::string>("input:kb_rules");
    static auto       PMODEL    = CConfigValue<std::string>("input:kb_model");
    static auto       PLAYOUT   = CConfigValue<std::string>("input:kb_layout");
    static auto       PVARIANT  = CConfigValue<std::string>("input:kb_variant");
    static auto       POPTIONS  = CConfigValue<std::string>("input:kb_options");

    const std::string FILEPATH = std::string{*PFILEPATH} == STRVAL_EMPTY ? "" : *PFILEPATH;
    const std::string RULES    = std::string{*PRULES} == STRVAL_EMPTY ? "" : *PRULES;
    const std::string MODEL    = std::string{*PMODEL} == STRVAL_EMPTY ? "" : *PMODEL;
    const std::string LAYOUT   = std::string{*PLAYOUT} == STRVAL_EMPTY ? "" : *PLAYOUT;
    const std::string VARIANT  = std::string{*PVARIANT} == STRVAL_EMPTY ? "" : *PVARIANT;
    const std::string OPTIONS  = std::string{*POPTIONS} == STRVAL_EMPTY ? "" : *POPTIONS;

    xkb_rule_names    rules      = {.rules = RULES.c_str(), .model = MODEL.c_str(), .layout = LAYOUT.c_str(), .variant = VARIANT.c_str(), .options = OPTIONS.c_str()};
    const auto        PCONTEXT   = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    FILE* const       KEYMAPFILE = FILEPATH.empty() ? nullptr : fopen(absolutePath(FILEPATH, Config::mgr()->currentConfigPath()).c_str(), "r");

    auto              PKEYMAP = KEYMAPFILE ? xkb_keymap_new_from_file(PCONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS) :
                                             xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (KEYMAPFILE)
        fclose(KEYMAPFILE);

    if (!PKEYMAP) {
        g_pHyprError->queueCreate("[Runtime Error] Invalid keyboard layout passed. ( rules: " + RULES + ", model: " + MODEL + ", variant: " + VARIANT + ", options: " + OPTIONS +
                                      ", layout: " + LAYOUT + " )",
                                  CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));

        Log::logger->log(Log::ERR, "[XKBTranslationState] Keyboard layout {} with variant {} (rules: {}, model: {}, options: {}) couldn't have been loaded.", rules.layout,
                         rules.variant, rules.rules, rules.model, rules.options);
        memset(&rules, 0, sizeof(rules));

        PKEYMAP = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    xkb_context_unref(PCONTEXT);
    m_xkbTranslationState = xkb_state_new(PKEYMAP);
    xkb_keymap_unref(PKEYMAP);
}

bool CKeybindManager::ensureMouseBindState() {
    if (!g_layoutManager->dragController()->target())
        return false;

    if (g_layoutManager->dragController()->target()) {
        changeMouseBindMode(MBIND_INVALID);
        return true;
    }

    return false;
}

bool CKeybindManager::onKeyEvent(std::any event, SP<IKeyboard> pKeyboard) {
    if (!g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        m_pressedKeys.clear();
        return true;
    }

    if (!pKeyboard->m_allowBinds)
        return true;

    if (!m_xkbTranslationState) {
        Log::logger->log(Log::ERR, "BUG THIS: m_pXKBTranslationState nullptr!");
        updateXKBTranslationState();

        if (!m_xkbTranslationState)
            return true;
    }

    auto               e = std::any_cast<IKeyboard::SKeyEvent>(event);

    const auto         KEYCODE = e.keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t keysym         = xkb_state_key_get_one_sym(pKeyboard->m_resolveBindsBySym ? pKeyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE);
    const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(pKeyboard->m_xkbState, KEYCODE);

    if (keysym == XKB_KEY_Escape || internalKeysym == XKB_KEY_Escape)
        PROTO::data->abortDndIfPresent();

    // handleInternalKeybinds returns true when the key should be suppressed,
    // while this function returns true when the key event should be sent
    if (handleInternalKeybinds(internalKeysym))
        return false;

    const auto MODS = g_pInputManager->getModsFromAllKBs();

    Config::Actions::state()->m_timeLastMs    = e.timeMs;
    Config::Actions::state()->m_lastCode      = KEYCODE;
    Config::Actions::state()->m_lastMouseCode = 0;

    bool       mouseBindWasActive = ensureMouseBindState();

    const auto KEY = SPressedKeyWithMods{
        .keysym             = keysym,
        .keycode            = KEYCODE,
        .modmaskAtPressTime = MODS,
        .sent               = true,
        .submapAtPress      = SSubmap{.name = Config::Actions::state()->m_currentSubmap},
        .mousePosAtPress    = g_pInputManager->getMouseCoordsInternal(),
    };

    m_activeKeybinds.clear();

    m_lastLongPressKeybind.reset();

    bool suppressEvent = false;
    if (e.state == WL_KEYBOARD_KEY_STATE_PRESSED) {

        m_pressedKeys.push_back(KEY);

        suppressEvent = !handleKeybinds(MODS, KEY, true, pKeyboard, pKeyboard).passEvent;

        if (suppressEvent)
            shadowKeybinds(keysym, KEYCODE);

        m_pressedKeys.back().sent = !suppressEvent;
    } else { // key release

        bool foundInPressedKeys = false;
        for (auto it = m_pressedKeys.begin(); it != m_pressedKeys.end();) {
            if (it->keycode == KEYCODE) {
                handleKeybinds(MODS, *it, false, pKeyboard, pKeyboard);
                foundInPressedKeys = true;
                suppressEvent      = !it->sent;
                it                 = m_pressedKeys.erase(it);
            } else {
                ++it;
            }
        }
        if (!foundInPressedKeys) {
            Log::logger->log(Log::ERR, "BUG THIS: key not found in m_dPressedKeys");
            // fallback with wrong `KEY.modmaskAtPressTime`, this can be buggy
            suppressEvent = !handleKeybinds(MODS, KEY, false, pKeyboard, pKeyboard).passEvent;
        }

        shadowKeybinds();
    }

    return !suppressEvent && !mouseBindWasActive;
}

bool CKeybindManager::onAxisEvent(const IPointer::SAxisEvent& e, SP<IPointer> pointer) {
    const auto  MODS = g_pInputManager->getModsFromAllKBs();

    static auto PDELAY = CConfigValue<Config::INTEGER>("binds:scroll_event_delay");

    if (m_scrollTimer.getMillis() < *PDELAY)
        return true; // timer hasn't passed yet!

    m_scrollTimer.reset();

    m_activeKeybinds.clear();

    bool found = false;
    if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (e.delta < 0)
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_down"}, true, nullptr, pointer).passEvent;
        else
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_up"}, true, nullptr, pointer).passEvent;
    } else if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        if (e.delta < 0)
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_left"}, true, nullptr, pointer).passEvent;
        else
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_right"}, true, nullptr, pointer).passEvent;
    }

    if (found)
        shadowKeybinds();

    return !found;
}

bool CKeybindManager::onMouseEvent(const IPointer::SButtonEvent& e, SP<IPointer> mouse) {
    const auto MODS = g_pInputManager->getModsFromAllKBs();

    bool       suppressEvent = false;

    Config::Actions::state()->m_lastMouseCode = e.button;
    Config::Actions::state()->m_lastCode      = 0;
    Config::Actions::state()->m_timeLastMs    = e.timeMs;

    bool       mouseBindWasActive = ensureMouseBindState();

    const auto KEY_NAME = "mouse:" + std::to_string(e.button);

    const auto KEY = SPressedKeyWithMods{
        .keyName            = KEY_NAME,
        .modmaskAtPressTime = MODS,
        .mousePosAtPress    = g_pInputManager->getMouseCoordsInternal(),
    };

    m_activeKeybinds.clear();

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_pressedKeys.push_back(KEY);

        suppressEvent = !handleKeybinds(MODS, KEY, true, nullptr, mouse).passEvent;

        if (suppressEvent)
            shadowKeybinds();

        m_pressedKeys.back().sent = !suppressEvent;
    } else {
        bool foundInPressedKeys = false;
        for (auto it = m_pressedKeys.begin(); it != m_pressedKeys.end();) {
            if (it->keyName == KEY_NAME) {
                suppressEvent      = !handleKeybinds(MODS, *it, false, nullptr, mouse).passEvent;
                foundInPressedKeys = true;
                suppressEvent      = !it->sent;
                it                 = m_pressedKeys.erase(it);
            } else {
                ++it;
            }
        }
        if (!foundInPressedKeys) {
            Log::logger->log(Log::ERR, "BUG THIS: key not found in m_dPressedKeys (2)");
            // fallback with wrong `KEY.modmaskAtPressTime`, this can be buggy
            suppressEvent = !handleKeybinds(MODS, KEY, false, nullptr, mouse).passEvent;
        }

        shadowKeybinds();
    }

    return !suppressEvent && !mouseBindWasActive;
}

void CKeybindManager::resizeWithBorder(const IPointer::SButtonEvent& e) {
    changeMouseBindMode(e.state == WL_POINTER_BUTTON_STATE_PRESSED ? MBIND_RESIZE : MBIND_INVALID);
}

void CKeybindManager::onSwitchEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:" + switchName}, true, nullptr, nullptr);
}

void CKeybindManager::onSwitchOnEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:on:" + switchName}, true, nullptr, nullptr);
}

void CKeybindManager::onSwitchOffEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:off:" + switchName}, true, nullptr, nullptr);
}

eMultiKeyCase CKeybindManager::mkKeysymSetMatches(const std::vector<xkb_keysym_t> keybindKeysyms, const std::set<xkb_keysym_t> pressedKeysyms) {
    // Returns whether two sets of keysyms are equal, partially equal, or not
    // matching. (Partially matching means that pressed is a subset of bound)

    std::set<xkb_keysym_t> boundKeysNotPressed;
    std::set<xkb_keysym_t> pressedKeysNotBound;

    std::set<xkb_keysym_t> symsKb;
    for (const auto& k : keybindKeysyms) {
        symsKb.emplace(k);
    }

    std::ranges::set_difference(symsKb, pressedKeysyms, std::inserter(boundKeysNotPressed, boundKeysNotPressed.begin()));
    std::ranges::set_difference(pressedKeysyms, symsKb, std::inserter(pressedKeysNotBound, pressedKeysNotBound.begin()));

    if (boundKeysNotPressed.empty() && pressedKeysNotBound.empty())
        return MK_FULL_MATCH;

    if (!boundKeysNotPressed.empty() && pressedKeysNotBound.empty())
        return MK_PARTIAL_MATCH;

    return MK_NO_MATCH;
}

eMultiKeyCase CKeybindManager::mkBindMatches(const SP<SKeybind> keybind) {
    if (mkKeysymSetMatches(keybind->sMkMods, m_mkMods) != MK_FULL_MATCH)
        return MK_NO_MATCH;

    return mkKeysymSetMatches(keybind->sMkKeys, m_mkKeys);
}

SSubmap CKeybindManager::getCurrentSubmap() {
    return SSubmap{.name = Config::Actions::state()->m_currentSubmap};
}

SDispatchResult CKeybindManager::handleKeybinds(const uint32_t modmask, const SPressedKeyWithMods& key, bool pressed, SP<IKeyboard> keyboard, SP<IHID> device) {
    static auto     PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");
    static auto     PDRAGTHRESHOLD  = CConfigValue<Config::INTEGER>("binds:drag_threshold");

    bool            found = false;
    SDispatchResult res;

    // Skip keysym tracking for events with no keysym (e.g., scroll wheel events).
    // Scroll events have keysym=0 and are always "pressed" (never released),
    // so without this check, 0 gets inserted into m_mkKeys and never removed,
    // breaking multi-key binds (binds flag 's'). See issue #8699.
    if (key.keysym != 0) {
        if (pressed) {
            if (keycodeToModifier(key.keycode))
                m_mkMods.insert(key.keysym);
            else
                m_mkKeys.insert(key.keysym);
        } else {
            if (keycodeToModifier(key.keycode))
                m_mkMods.erase(key.keysym);
            else
                m_mkKeys.erase(key.keysym);
        }
    }

    for (auto& k : m_keybinds) {
        const bool SPECIALDISPATCHER = k->handler == "global" || k->handler == "pass" || k->handler == "sendshortcut" || k->handler == "mouse";
        const bool SPECIALTRIGGERED  = std::ranges::find_if(m_pressedSpecialBinds, [&](const auto& other) { return other == k; }) != m_pressedSpecialBinds.end();
        const bool IGNORECONDITIONS =
            SPECIALDISPATCHER && !pressed && SPECIALTRIGGERED; // ignore mods. Pass, global dispatchers should be released immediately once the key is released.

        if (!k->dontInhibit && !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited())
            continue;

        if (!k->locked && g_pSessionLockManager->isSessionLocked())
            continue;

        if (!IGNORECONDITIONS && ((modmask != k->modmask && !k->ignoreMods) || (k->submap.name != Config::Actions::state()->m_currentSubmap && !k->submapUniversal) || k->shadowed))
            continue;

        if (device) {
            if (k->deviceInclusive ^ k->devices.contains(device->m_hlName))
                continue;
        }

        if (k->multiKey) {
            switch (mkBindMatches(k)) {
                case MK_NO_MATCH: continue;
                case MK_PARTIAL_MATCH: found = true; continue;
                case MK_FULL_MATCH: found = true;
            }
        } else if (!k->sMkKeys.empty() && key.keyName.empty()) {
            if (!k->release && mkKeysymSetMatches(k->sMkKeys, m_mkKeys) != MK_FULL_MATCH)
                continue;
            if (key.keysym != k->sMkKeys.back())
                continue;
        } else if (!key.keyName.empty()) {
            if (key.keyName != k->key)
                continue;
        } else if (k->keycode != 0) {
            if (key.keycode != k->keycode)
                continue;
        } else if (k->catchAll) {
            if (found || key.submapAtPress.name != Config::Actions::state()->m_currentSubmap)
                continue;
        } else {
            // in this case, we only have the keysym to go off of for this keybind, and it's invalid
            // since there might be something like keycode to match with other keybinds, try the next
            if (key.keysym == XKB_KEY_NoSymbol)
                continue;

            // oMg such performance hit!!11!
            // this little maneouver is gonna cost us 4µs
            const auto KBKEY      = xkb_keysym_from_name(k->key.c_str(), XKB_KEYSYM_NO_FLAGS);
            const auto KBKEYLOWER = xkb_keysym_from_name(k->key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

            if (KBKEY == XKB_KEY_NoSymbol && KBKEYLOWER == XKB_KEY_NoSymbol) {
                // Keysym failed to resolve from the key name of the currently iterated bind.
                // This happens for names such as `switch:off:Lid Switch` as well as some keys
                // (such as yen and ro).
                //
                // We can't let compare a 0-value with currently pressed key below,
                // because if this key also have no keysym (i.e. key.keysym == 0) it will incorrectly trigger the
                // currently iterated bind. That's confirmed to be happening with yen and ro keys.
                continue;
            }

            if (key.keysym != KBKEY && key.keysym != KBKEYLOWER)
                continue;
        }

        if (pressed && k->release && !SPECIALDISPATCHER) {
            if (k->nonConsuming)
                continue;

            found = true; // suppress the event
            continue;
        }

        if (!pressed) {
            // Require mods to be matching when the key was first pressed.
            if (key.modmaskAtPressTime != modmask && !k->ignoreMods) {
                // Handle properly `bindr` where a key is itself a bind mod for example:
                // "bindr = SUPER, SUPER_L, exec, $launcher".
                // This needs to be handled separately for the above case, because `key.modmaskAtPressTime` is set
                // from currently pressed keys as programs see them, but it doesn't yet include the currently
                // pressed mod key, which is still being handled internally.
                if (keycodeToModifier(key.keycode) == key.modmaskAtPressTime)
                    continue;

            } else if (!k->release && !SPECIALDISPATCHER) {
                if (k->nonConsuming)
                    continue;

                found = true; // suppress the event
                continue;
            }

            // Require mouse to stay inside drag_threshold for clicks, outside for drags
            // Check if either a mouse bind has triggered or currently over the threshold (maybe there is no mouse bind on the same key)
            const auto THRESHOLDREACHED = key.mousePosAtPress.distanceSq(g_pInputManager->getMouseCoordsInternal()) > std::pow(*PDRAGTHRESHOLD, 2);
            if (k->click && (g_layoutManager->dragController()->dragThresholdReached() || THRESHOLDREACHED))
                continue;
            else if (k->drag && !g_layoutManager->dragController()->dragThresholdReached() && !THRESHOLDREACHED)
                continue;
        }

        if (pressed && k->longPress) {
            const auto PACTIVEKEEB = g_pSeatManager->m_keyboard.lock();

            m_longPressTimer->updateTimeout(std::chrono::milliseconds(PACTIVEKEEB->m_repeatDelay));
            m_lastLongPressKeybind = k;

            continue;
        }

        const auto DISPATCHER = m_dispatchers.find(k->mouse ? "mouse" : k->handler);

        if (SPECIALTRIGGERED && !pressed)
            std::erase_if(m_pressedSpecialBinds, [&](const auto& other) { return other == k; });
        else if (SPECIALDISPATCHER && pressed)
            m_pressedSpecialBinds.emplace_back(k);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_dispatchers.end()) {
            Log::logger->log(Log::ERR, "Invalid handler in a keybind! (handler {} does not exist)", k->handler);
        } else {
            // call the dispatcher
            Log::logger->log(Log::DEBUG, "Keybind triggered, calling dispatcher ({}, {}, {}, {})", modmask, key.keyName, key.keysym, DISPATCHER->first);

            Config::Actions::state()->m_passPressed = sc<int>(pressed);

            // if the dispatchers says to pass event then we will
            if (k->handler == "mouse")
                res = DISPATCHER->second((pressed ? "1" : "0") + k->arg);
            else
                res = DISPATCHER->second(k->arg);

            Config::Actions::state()->m_passPressed = -1;

            if (k->handler == "submap") {
                found = true; // don't process keybinds on submap change.
                break;
            }
            if (k->handler != "submap" && !k->submap.reset.empty()) // NOLINTNEXTLINE
                Config::Actions::setSubmap(k->submap.reset);
        }

        if (pressed && k->repeat) {
            const auto KEEB = keyboard ? keyboard : g_pSeatManager->m_keyboard.lock();
            m_repeatKeyRate = KEEB->m_repeatRate;

            m_activeKeybinds.emplace_back(k);
            m_repeatKeyTimer->updateTimeout(std::chrono::milliseconds(KEEB->m_repeatDelay));
        }

        if (!k->nonConsuming)
            found = true;
    }

    g_layoutManager->dragController()->resetDragThresholdReached();

    // if keybind wasn't found (or dispatcher said to) then pass event
    res.passEvent |= !found;

    if (!found && !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited()) {
        Log::logger->log(Log::DEBUG, "Keybind handling is disabled due to an inhibitor");

        res.success = false;
        if (res.error.empty())
            res.error = "Keybind handling is disabled due to an inhibitor";
    }

    return res;
}

void CKeybindManager::shadowKeybinds(const xkb_keysym_t& doesntHave, const uint32_t doesntHaveCode) {
    // shadow disables keybinds after one has been triggered

    for (auto& k : m_keybinds) {

        bool shadow = false;

        if (k->handler == "global" || k->transparent)
            continue; // can't be shadowed

        if (k->multiKey && (mkBindMatches(k) == MK_FULL_MATCH))
            shadow = true;
        else {
            const auto KBKEY      = xkb_keysym_from_name(k->key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
            const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);

            for (auto const& pk : m_pressedKeys) {
                if ((pk.keysym != 0 && (pk.keysym == KBKEY || pk.keysym == KBKEYUPPER))) {
                    shadow = true;

                    if (pk.keysym == doesntHave && doesntHave != 0) {
                        shadow = false;
                        break;
                    }
                }

                if (pk.keycode != 0 && pk.keycode == k->keycode) {
                    shadow = true;

                    if (pk.keycode == doesntHaveCode && doesntHaveCode != 0) {
                        shadow = false;
                        break;
                    }
                }
            }
        }

        k->shadowed = shadow;
    }
}

bool CKeybindManager::handleVT(xkb_keysym_t keysym) {
    // Handles the CTRL+ALT+FX TTY keybinds
    if (keysym < XKB_KEY_XF86Switch_VT_1 || keysym > XKB_KEY_XF86Switch_VT_12)
        return false;

    // beyond this point, return true to not handle anything else.
    // we'll avoid printing shit to active windows.

    if (g_pCompositor->m_aqBackend->hasSession()) {
        const unsigned int TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;

        const auto         CURRENT_TTY = g_pCompositor->getVTNr();

        if (!CURRENT_TTY.has_value() || *CURRENT_TTY == TTY)
            return true;

        Log::logger->log(Log::DEBUG, "Switching from VT {} to VT {}", *CURRENT_TTY, TTY);

        g_pCompositor->m_aqBackend->session->switchVT(TTY);
    }

    return true;
}

bool CKeybindManager::handleInternalKeybinds(xkb_keysym_t keysym) {
    if (handleVT(keysym))
        return true;

    // handle ESC while in kill mode
    if (g_pInputManager->getClickMode() == CLICKMODE_KILL) {
        const auto KBKEY = xkb_keysym_from_name("ESCAPE", XKB_KEYSYM_CASE_INSENSITIVE);

        if (keysym == KBKEY) {
            g_pInputManager->setClickMode(CLICKMODE_DEFAULT);
            return true;
        }
    }

    return false;
}

void CKeybindManager::clearKeybinds() {
    m_keybinds.clear();
}

SDispatchResult CKeybindManager::changeMouseBindMode(const eMouseBindMode MODE) {
    if (MODE != MBIND_INVALID) {
        if (g_layoutManager->dragController()->target())
            return {};

        const auto      MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const PHLWINDOW PWINDOW = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

        if (!PWINDOW)
            return SDispatchResult{.passEvent = true};

        if (!PWINDOW->isFullscreen() && MODE == MBIND_MOVE) {
            if (PWINDOW->checkInputOnDecos(INPUT_TYPE_DRAG_START, MOUSECOORDS))
                return SDispatchResult{.passEvent = false};
        }

        g_layoutManager->beginDragTarget(PWINDOW->layoutTarget(), MODE);
    } else {
        if (!g_layoutManager->dragController()->target())
            return {};

        g_layoutManager->endDragTarget();
    }

    return {};
}
