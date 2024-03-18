#include "KeybindManager.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "debug/Log.hpp"
#include "helpers/VarList.hpp"
#include "../config/ConfigValue.hpp"

#include <regex>

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

    m_mDispatchers["exec"]                           = spawn;
    m_mDispatchers["execr"]                          = spawnRaw;
    m_mDispatchers["killactive"]                     = killActive;
    m_mDispatchers["closewindow"]                    = kill;
    m_mDispatchers["togglefloating"]                 = toggleActiveFloating;
    m_mDispatchers["setfloating"]                    = setActiveFloating;
    m_mDispatchers["settiled"]                       = setActiveTiled;
    m_mDispatchers["workspace"]                      = changeworkspace;
    m_mDispatchers["renameworkspace"]                = renameWorkspace;
    m_mDispatchers["fullscreen"]                     = fullscreenActive;
    m_mDispatchers["fakefullscreen"]                 = fakeFullscreenActive;
    m_mDispatchers["movetoworkspace"]                = moveActiveToWorkspace;
    m_mDispatchers["movetoworkspacesilent"]          = moveActiveToWorkspaceSilent;
    m_mDispatchers["pseudo"]                         = toggleActivePseudo;
    m_mDispatchers["movefocus"]                      = moveFocusTo;
    m_mDispatchers["movewindow"]                     = moveActiveTo;
    m_mDispatchers["swapwindow"]                     = swapActive;
    m_mDispatchers["centerwindow"]                   = centerWindow;
    m_mDispatchers["togglegroup"]                    = toggleGroup;
    m_mDispatchers["changegroupactive"]              = changeGroupActive;
    m_mDispatchers["movegroupwindow"]                = moveGroupWindow;
    m_mDispatchers["togglesplit"]                    = toggleSplit;
    m_mDispatchers["swapsplit"]                      = swapSplit;
    m_mDispatchers["splitratio"]                     = alterSplitRatio;
    m_mDispatchers["focusmonitor"]                   = focusMonitor;
    m_mDispatchers["movecursortocorner"]             = moveCursorToCorner;
    m_mDispatchers["movecursor"]                     = moveCursor;
    m_mDispatchers["workspaceopt"]                   = workspaceOpt;
    m_mDispatchers["exit"]                           = exitHyprland;
    m_mDispatchers["movecurrentworkspacetomonitor"]  = moveCurrentWorkspaceToMonitor;
    m_mDispatchers["focusworkspaceoncurrentmonitor"] = focusWorkspaceOnCurrentMonitor;
    m_mDispatchers["moveworkspacetomonitor"]         = moveWorkspaceToMonitor;
    m_mDispatchers["togglespecialworkspace"]         = toggleSpecialWorkspace;
    m_mDispatchers["forcerendererreload"]            = forceRendererReload;
    m_mDispatchers["resizeactive"]                   = resizeActive;
    m_mDispatchers["moveactive"]                     = moveActive;
    m_mDispatchers["cyclenext"]                      = circleNext;
    m_mDispatchers["focuswindowbyclass"]             = focusWindow;
    m_mDispatchers["focuswindow"]                    = focusWindow;
    m_mDispatchers["submap"]                         = setSubmap;
    m_mDispatchers["pass"]                           = pass;
    m_mDispatchers["layoutmsg"]                      = layoutmsg;
    m_mDispatchers["toggleopaque"]                   = toggleOpaque;
    m_mDispatchers["dpms"]                           = dpms;
    m_mDispatchers["movewindowpixel"]                = moveWindow;
    m_mDispatchers["resizewindowpixel"]              = resizeWindow;
    m_mDispatchers["swapnext"]                       = swapnext;
    m_mDispatchers["swapactiveworkspaces"]           = swapActiveWorkspaces;
    m_mDispatchers["pin"]                            = pinActive;
    m_mDispatchers["mouse"]                          = mouse;
    m_mDispatchers["bringactivetotop"]               = bringActiveToTop;
    m_mDispatchers["alterzorder"]                    = alterZOrder;
    m_mDispatchers["focusurgentorlast"]              = focusUrgentOrLast;
    m_mDispatchers["focuscurrentorlast"]             = focusCurrentOrLast;
    m_mDispatchers["lockgroups"]                     = lockGroups;
    m_mDispatchers["lockactivegroup"]                = lockActiveGroup;
    m_mDispatchers["moveintogroup"]                  = moveIntoGroup;
    m_mDispatchers["moveoutofgroup"]                 = moveOutOfGroup;
    m_mDispatchers["movewindoworgroup"]              = moveWindowOrGroup;
    m_mDispatchers["setignoregrouplock"]             = setIgnoreGroupLock;
    m_mDispatchers["denywindowfromgroup"]            = denyWindowFromGroup;
    m_mDispatchers["global"]                         = global;

    m_tScrollTimer.reset();

    g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) {
        // clear cuz realloc'd
        m_pActiveKeybind = nullptr;
        m_vPressedSpecialBinds.clear();
    });
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_lKeybinds.push_back(kb);

    m_pActiveKeybind = nullptr;
}

void CKeybindManager::removeKeybind(uint32_t mod, const SParsedKey& key) {
    for (auto it = m_lKeybinds.begin(); it != m_lKeybinds.end(); ++it) {
        if (it->modmask == mod && it->key == key.key && it->keycode == key.keycode && it->catchAll == key.catchAll) {
            it = m_lKeybinds.erase(it);

            if (it == m_lKeybinds.end())
                break;
        }
    }

    m_pActiveKeybind = nullptr;
}

uint32_t CKeybindManager::stringToModMask(std::string mods) {
    uint32_t modMask = 0;
    std::transform(mods.begin(), mods.end(), mods.begin(), ::toupper);
    if (mods.contains("SHIFT"))
        modMask |= WLR_MODIFIER_SHIFT;
    if (mods.contains("CAPS"))
        modMask |= WLR_MODIFIER_CAPS;
    if (mods.contains("CTRL") || mods.contains("CONTROL"))
        modMask |= WLR_MODIFIER_CTRL;
    if (mods.contains("ALT") || mods.contains("MOD1"))
        modMask |= WLR_MODIFIER_ALT;
    if (mods.contains("MOD2"))
        modMask |= WLR_MODIFIER_MOD2;
    if (mods.contains("MOD3"))
        modMask |= WLR_MODIFIER_MOD3;
    if (mods.contains("SUPER") || mods.contains("WIN") || mods.contains("LOGO") || mods.contains("MOD4"))
        modMask |= WLR_MODIFIER_LOGO;
    if (mods.contains("MOD5"))
        modMask |= WLR_MODIFIER_MOD5;

    return modMask;
}

uint32_t CKeybindManager::keycodeToModifier(xkb_keycode_t keycode) {
    switch (keycode - 8) {
        case KEY_LEFTMETA: return WLR_MODIFIER_LOGO;
        case KEY_RIGHTMETA: return WLR_MODIFIER_LOGO;
        case KEY_LEFTSHIFT: return WLR_MODIFIER_SHIFT;
        case KEY_RIGHTSHIFT: return WLR_MODIFIER_SHIFT;
        case KEY_LEFTCTRL: return WLR_MODIFIER_CTRL;
        case KEY_RIGHTCTRL: return WLR_MODIFIER_CTRL;
        case KEY_LEFTALT: return WLR_MODIFIER_ALT;
        case KEY_RIGHTALT: return WLR_MODIFIER_ALT;
        case KEY_CAPSLOCK: return WLR_MODIFIER_CAPS;
        case KEY_NUMLOCK: return WLR_MODIFIER_MOD2;
        default: return 0;
    }
}

void CKeybindManager::updateXKBTranslationState() {
    if (m_pXKBTranslationState) {
        xkb_keymap_unref(xkb_state_get_keymap(m_pXKBTranslationState));
        xkb_state_unref(m_pXKBTranslationState);

        m_pXKBTranslationState = nullptr;
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
    FILE* const       KEYMAPFILE = FILEPATH == "" ? NULL : fopen(absolutePath(FILEPATH, g_pConfigManager->configCurrentPath).c_str(), "r");

    auto              PKEYMAP = KEYMAPFILE ? xkb_keymap_new_from_file(PCONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS) :
                                             xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (KEYMAPFILE)
        fclose(KEYMAPFILE);

    if (!PKEYMAP) {
        g_pHyprError->queueCreate("[Runtime Error] Invalid keyboard layout passed. ( rules: " + RULES + ", model: " + MODEL + ", variant: " + VARIANT + ", options: " + OPTIONS +
                                      ", layout: " + LAYOUT + " )",
                                  CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));

        Debug::log(ERR, "[XKBTranslationState] Keyboard layout {} with variant {} (rules: {}, model: {}, options: {}) couldn't have been loaded.", rules.layout, rules.variant,
                   rules.rules, rules.model, rules.options);
        memset(&rules, 0, sizeof(rules));

        PKEYMAP = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    xkb_context_unref(PCONTEXT);
    m_pXKBTranslationState = xkb_state_new(PKEYMAP);
}

bool CKeybindManager::ensureMouseBindState() {
    if (!m_bIsMouseBindActive)
        return false;

    if (g_pInputManager->currentlyDraggedWindow) {
        m_bIsMouseBindActive = false;
        g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
        g_pInputManager->currentlyDraggedWindow = nullptr;
        g_pInputManager->dragMode               = MBIND_INVALID;

        return true;
    }

    m_bIsMouseBindActive = false;

    return false;
}

bool CKeybindManager::tryMoveFocusToMonitor(CMonitor* monitor) {
    if (!monitor)
        return false;

    const auto LASTMONITOR = g_pCompositor->m_pLastMonitor;
    if (LASTMONITOR == monitor) {
        Debug::log(LOG, "Tried to move to active monitor");
        return false;
    }

    const auto PWORKSPACE        = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);
    const auto PNEWMAINWORKSPACE = g_pCompositor->getWorkspaceByID(monitor->activeWorkspace);

    g_pInputManager->unconstrainMouse();
    PNEWMAINWORKSPACE->rememberPrevWorkspace(PWORKSPACE);

    const auto PNEWWORKSPACE = monitor->specialWorkspaceID != 0 ? g_pCompositor->getWorkspaceByID(monitor->specialWorkspaceID) : PNEWMAINWORKSPACE;

    const auto PNEWWINDOW = PNEWWORKSPACE->getLastFocusedWindow();
    if (PNEWWINDOW) {
        g_pCompositor->focusWindow(PNEWWINDOW);
        g_pCompositor->warpCursorTo(PNEWWINDOW->middle());

        g_pInputManager->m_pForcedFocus = PNEWWINDOW;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_pForcedFocus = nullptr;
    } else {
        g_pCompositor->focusWindow(nullptr);
        g_pCompositor->warpCursorTo(monitor->middle());
    }
    g_pCompositor->setActiveMonitor(monitor);

    return true;
}

void CKeybindManager::switchToWindow(CWindow* PWINDOWTOCHANGETO) {
    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (PWINDOWTOCHANGETO == PLASTWINDOW || !PWINDOWTOCHANGETO)
        return;

    // remove constraints
    g_pInputManager->unconstrainMouse();

    if (PLASTWINDOW && PLASTWINDOW->m_iWorkspaceID == PWINDOWTOCHANGETO->m_iWorkspaceID && PLASTWINDOW->m_bIsFullscreen) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PLASTWINDOW->m_iWorkspaceID);
        const auto FSMODE     = PWORKSPACE->m_efFullscreenMode;

        if (!PWINDOWTOCHANGETO->m_bPinned)
            g_pCompositor->setWindowFullscreen(PLASTWINDOW, false, FULLSCREEN_FULL);

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);

        if (!PWINDOWTOCHANGETO->m_bPinned)
            g_pCompositor->setWindowFullscreen(PWINDOWTOCHANGETO, true, FSMODE);
    } else {
        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());

        g_pInputManager->m_pForcedFocus = PWINDOWTOCHANGETO;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_pForcedFocus = nullptr;

        if (PLASTWINDOW && PLASTWINDOW->m_iMonitorID != PWINDOWTOCHANGETO->m_iMonitorID) {
            // event
            const auto PNEWMON = g_pCompositor->getMonitorFromID(PWINDOWTOCHANGETO->m_iMonitorID);

            g_pCompositor->setActiveMonitor(PNEWMON);
        }
    }
};

bool CKeybindManager::onKeyEvent(wlr_keyboard_key_event* e, SKeyboard* pKeyboard) {
    if (!g_pCompositor->m_bSessionActive || g_pCompositor->m_bUnsafeState) {
        m_dPressedKeys.clear();
        return true;
    }

    if (pKeyboard->isVirtual && g_pInputManager->shouldIgnoreVirtualKeyboard(pKeyboard))
        return true;

    if (!m_pXKBTranslationState) {
        Debug::log(ERR, "BUG THIS: m_pXKBTranslationState NULL!");
        updateXKBTranslationState();

        if (!m_pXKBTranslationState)
            return true;
    }

    const auto         KEYCODE = e->keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t keysym         = xkb_state_key_get_one_sym(pKeyboard->resolveBindsBySym ? pKeyboard->xkbTranslationState : m_pXKBTranslationState, KEYCODE);
    const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(wlr_keyboard_from_input_device(pKeyboard->keyboard)->xkb_state, KEYCODE);

    if (handleInternalKeybinds(internalKeysym))
        return true;

    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    m_uTimeLastMs    = e->time_msec;
    m_uLastCode      = KEYCODE;
    m_uLastMouseCode = 0;

    bool       mouseBindWasActive = ensureMouseBindState();

    const auto KEY = SPressedKeyWithMods{
        .keysym             = keysym,
        .keycode            = KEYCODE,
        .modmaskAtPressTime = MODS,
        .sent               = true,
        .submapAtPress      = m_szCurrentSelectedSubmap,
    };

    bool suppressEvent = false;
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // clean repeat
        if (m_pActiveKeybindEventSource) {
            wl_event_source_remove(m_pActiveKeybindEventSource);
            m_pActiveKeybindEventSource = nullptr;
            m_pActiveKeybind            = nullptr;
        }

        m_dPressedKeys.push_back(KEY);

        suppressEvent = handleKeybinds(MODS, KEY, true);

        if (suppressEvent)
            shadowKeybinds(keysym, KEYCODE);

        m_dPressedKeys.back().sent = !suppressEvent;
    } else { // key release
        // clean repeat
        if (m_pActiveKeybindEventSource) {
            wl_event_source_remove(m_pActiveKeybindEventSource);
            m_pActiveKeybindEventSource = nullptr;
            m_pActiveKeybind            = nullptr;
        }

        bool foundInPressedKeys = false;
        for (auto it = m_dPressedKeys.begin(); it != m_dPressedKeys.end();) {
            if (it->keycode == KEYCODE) {
                if (it->submapAtPress == m_szCurrentSelectedSubmap)
                    handleKeybinds(MODS, *it, false);
                foundInPressedKeys = true;
                suppressEvent      = !it->sent;
                it                 = m_dPressedKeys.erase(it);
            } else {
                ++it;
            }
        }
        if (!foundInPressedKeys) {
            Debug::log(ERR, "BUG THIS: key not found in m_dPressedKeys");
            // fallback with wrong `KEY.modmaskAtPressTime`, this can be buggy
            suppressEvent = handleKeybinds(MODS, KEY, false);
        }

        shadowKeybinds();
    }

    return !suppressEvent && !mouseBindWasActive;
}

bool CKeybindManager::onAxisEvent(wlr_pointer_axis_event* e) {
    const auto  MODS = g_pInputManager->accumulateModsFromAllKBs();

    static auto PDELAY = CConfigValue<Hyprlang::INT>("binds:scroll_event_delay");

    if (m_tScrollTimer.getMillis() < *PDELAY) {
        m_tScrollTimer.reset();
        return true; // timer hasn't passed yet!
    }

    m_tScrollTimer.reset();

    bool found = false;
    if (e->source == WL_POINTER_AXIS_SOURCE_WHEEL && e->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (e->delta < 0)
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_down"}, true);
        else
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_up"}, true);
    } else if (e->source == WL_POINTER_AXIS_SOURCE_WHEEL && e->orientation == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        if (e->delta < 0)
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_left"}, true);
        else
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_right"}, true);
    }

    if (found)
        shadowKeybinds();

    return !found;
}

bool CKeybindManager::onMouseEvent(wlr_pointer_button_event* e) {
    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    bool       suppressEvent = false;

    m_uLastMouseCode = e->button;
    m_uLastCode      = 0;
    m_uTimeLastMs    = e->time_msec;

    bool       mouseBindWasActive = ensureMouseBindState();

    const auto KEY_NAME = "mouse:" + std::to_string(e->button);

    const auto KEY = SPressedKeyWithMods{
        .keyName            = KEY_NAME,
        .modmaskAtPressTime = MODS,
    };

    if (e->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_dPressedKeys.push_back(KEY);

        suppressEvent = handleKeybinds(MODS, KEY, true);

        if (suppressEvent)
            shadowKeybinds();

        m_dPressedKeys.back().sent = !suppressEvent;
    } else {
        bool foundInPressedKeys = false;
        for (auto it = m_dPressedKeys.begin(); it != m_dPressedKeys.end();) {
            if (it->keyName == KEY_NAME) {
                suppressEvent      = handleKeybinds(MODS, *it, false);
                foundInPressedKeys = true;
                suppressEvent      = !it->sent;
                it                 = m_dPressedKeys.erase(it);
            } else {
                ++it;
            }
        }
        if (!foundInPressedKeys) {
            Debug::log(ERR, "BUG THIS: key not found in m_dPressedKeys (2)");
            // fallback with wrong `KEY.modmaskAtPressTime`, this can be buggy
            suppressEvent = handleKeybinds(MODS, KEY, false);
        }

        shadowKeybinds();
    }

    return !suppressEvent && !mouseBindWasActive;
}

void CKeybindManager::resizeWithBorder(wlr_pointer_button_event* e) {
    if (e->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        mouse("1resizewindow");
    } else {
        mouse("0resizewindow");
    }
}

void CKeybindManager::onSwitchEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:" + switchName}, true);
}

void CKeybindManager::onSwitchOnEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:on:" + switchName}, true);
}

void CKeybindManager::onSwitchOffEvent(const std::string& switchName) {
    handleKeybinds(0, SPressedKeyWithMods{.keyName = "switch:off:" + switchName}, true);
}

int repeatKeyHandler(void* data) {
    SKeybind** ppActiveKeybind = (SKeybind**)data;

    if (!*ppActiveKeybind)
        return 0;

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find((*ppActiveKeybind)->handler);

    Debug::log(LOG, "Keybind repeat triggered, calling dispatcher.");
    DISPATCHER->second((*ppActiveKeybind)->arg);

    wl_event_source_timer_update(g_pKeybindManager->m_pActiveKeybindEventSource, 1000 / g_pInputManager->m_pActiveKeyboard->repeatRate);

    return 0;
}

bool CKeybindManager::handleKeybinds(const uint32_t modmask, const SPressedKeyWithMods& key, bool pressed) {
    bool found = false;

    if (g_pCompositor->m_sSeat.exclusiveClient)
        Debug::log(LOG, "Keybind handling only locked (inhibitor)");

    if (!m_lShortcutInhibitors.empty()) {
        for (auto& i : m_lShortcutInhibitors) {
            if (i.pWlrInhibitor->surface == g_pCompositor->m_pLastFocus) {
                Debug::log(LOG, "Keybind handling is disabled due to an inhibitor for surface {:x}", (uintptr_t)i.pWlrInhibitor->surface);
                return false;
            }
        }
    }

    for (auto& k : m_lKeybinds) {
        const bool SPECIALDISPATCHER = k.handler == "global" || k.handler == "pass" || k.handler == "mouse";
        const bool SPECIALTRIGGERED =
            std::find_if(m_vPressedSpecialBinds.begin(), m_vPressedSpecialBinds.end(), [&](const auto& other) { return other == &k; }) != m_vPressedSpecialBinds.end();
        const bool IGNORECONDITIONS =
            SPECIALDISPATCHER && !pressed && SPECIALTRIGGERED; // ignore mods. Pass, global dispatchers should be released immediately once the key is released.

        if (!IGNORECONDITIONS &&
            ((modmask != k.modmask && !k.ignoreMods) || (g_pCompositor->m_sSeat.exclusiveClient && !k.locked) || k.submap != m_szCurrentSelectedSubmap || k.shadowed))
            continue;

        if (!key.keyName.empty()) {
            if (key.keyName != k.key)
                continue;
        } else if (k.keycode != 0) {
            if (key.keycode != k.keycode)
                continue;
        } else if (k.catchAll) {
            if (found)
                continue;
        } else {
            // oMg such performance hit!!11!
            // this little maneouver is gonna cost us 4µs
            const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

            if (KBKEY == 0) {
                // Keysym failed to resolve from the key name of the currently iterated bind.
                // This happens for names such as `switch:off:Lid Switch` as well as some keys
                // (such as yen and ro).
                //
                // We can't let compare a 0-value with currently pressed key below,
                // because if this key also have no keysym (i.e. key.keysym == 0) it will incorrectly trigger the
                // currently iterated bind. That's confirmed to be happening with yen and ro keys.
                continue;
            }

            const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);

            if (key.keysym != KBKEY && key.keysym != KBKEYUPPER)
                continue;
        }

        if (pressed && k.release && !SPECIALDISPATCHER) {
            if (k.nonConsuming)
                continue;

            found = true; // suppress the event
            continue;
        }

        if (!pressed) {
            // Require mods to be matching when the key was first pressed.
            if (key.modmaskAtPressTime != modmask && !k.ignoreMods) {
                // Handle properly `bindr` where a key is itself a bind mod for example:
                // "bindr = SUPER, SUPER_L, exec, $launcher".
                // This needs to be handled separately for the above case, because `key.modmaskAtPressTime` is set
                // from currently pressed keys as programs see them, but it doesn't yet include the currently
                // pressed mod key, which is still being handled internally.
                if (keycodeToModifier(key.keycode) == key.modmaskAtPressTime)
                    continue;

            } else if (!k.release && !SPECIALDISPATCHER) {
                if (k.nonConsuming)
                    continue;

                found = true; // suppress the event
                continue;
            }
        }

        const auto DISPATCHER = m_mDispatchers.find(k.mouse ? "mouse" : k.handler);

        if (SPECIALTRIGGERED && !pressed)
            std::erase_if(m_vPressedSpecialBinds, [&](const auto& other) { return other == &k; });
        else if (SPECIALDISPATCHER && pressed)
            m_vPressedSpecialBinds.push_back(&k);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_mDispatchers.end()) {
            Debug::log(ERR, "Invalid handler in a keybind! (handler {} does not exist)", k.handler);
        } else {
            // call the dispatcher
            Debug::log(LOG, "Keybind triggered, calling dispatcher ({}, {}, {})", modmask, key.keyName, key.keysym);

            m_iPassPressed = (int)pressed;

            if (k.handler == "mouse")
                DISPATCHER->second((pressed ? "1" : "0") + k.arg);
            else
                DISPATCHER->second(k.arg);

            m_iPassPressed = -1;

            if (k.handler == "submap") {
                found = true; // don't process keybinds on submap change.
                break;
            }
        }

        if (k.repeat) {
            m_pActiveKeybind            = &k;
            m_pActiveKeybindEventSource = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, repeatKeyHandler, &m_pActiveKeybind);

            const auto PACTIVEKEEB = g_pInputManager->m_pActiveKeyboard;

            wl_event_source_timer_update(m_pActiveKeybindEventSource, PACTIVEKEEB->repeatDelay);
        }

        if (!k.nonConsuming)
            found = true;
    }

    return found;
}

void CKeybindManager::shadowKeybinds(const xkb_keysym_t& doesntHave, const uint32_t doesntHaveCode) {
    // shadow disables keybinds after one has been triggered

    for (auto& k : m_lKeybinds) {

        bool shadow = false;

        if (k.handler == "global" || k.transparent)
            continue; // can't be shadowed

        const auto KBKEY      = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);

        for (auto& pk : m_dPressedKeys) {
            if ((pk.keysym != 0 && (pk.keysym == KBKEY || pk.keysym == KBKEYUPPER))) {
                shadow = true;

                if (pk.keysym == doesntHave && doesntHave != 0) {
                    shadow = false;
                    break;
                }
            }

            if (pk.keycode != 0 && pk.keycode == k.keycode) {
                shadow = true;

                if (pk.keycode == doesntHaveCode && doesntHaveCode != 0) {
                    shadow = false;
                    break;
                }
            }
        }

        k.shadowed = shadow;
    }
}

bool CKeybindManager::handleVT(xkb_keysym_t keysym) {
    // Handles the CTRL+ALT+FX TTY keybinds
    if (!(keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12))
        return false;

    // beyond this point, return true to not handle anything else.
    // we'll avoid printing shit to active windows.

    if (g_pCompositor->m_sWLRSession) {
        const unsigned int TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;

        // vtnr is bugged for some reason.
        unsigned int ttynum = 0;
        int          fd;
        if ((fd = open("/dev/tty", O_RDONLY | O_NOCTTY)) >= 0) {
#if defined(VT_GETSTATE)
            struct vt_stat st;
            if (!ioctl(fd, VT_GETSTATE, &st))
                ttynum = st.v_active;
#elif defined(VT_GETACTIVE)
            int vt;
            if (!ioctl(fd, VT_GETACTIVE, &vt))
                ttynum = vt;
#endif
            close(fd);
        }

        if (ttynum == TTY)
            return true;

        Debug::log(LOG, "Switching from VT {} to VT {}", ttynum, TTY);

        if (!wlr_session_change_vt(g_pCompositor->m_sWLRSession, TTY))
            return true; // probably same session

        g_pCompositor->m_bSessionActive = false;

        for (auto& m : g_pCompositor->m_vMonitors) {
            m->noFrameSchedule = true;
            m->framesToSkip    = 1;
        }

        Debug::log(LOG, "Switched to VT {}, destroyed all render data, frames to skip for each: 2", TTY);

        return true;
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

// Dispatchers

void CKeybindManager::spawn(std::string args) {

    args = removeBeginEndSpacesTabs(args);

    std::string RULES = "";

    if (args[0] == '[') {
        // we have exec rules
        RULES = args.substr(1, args.substr(1).find_first_of(']'));
        args  = args.substr(args.find_first_of(']') + 1);
    }

    const uint64_t PROC = spawnRaw(args);

    if (!RULES.empty()) {
        const auto RULESLIST = CVarList(RULES, 0, ';');

        for (auto& r : RULESLIST) {
            g_pConfigManager->addExecRule({r, (unsigned long)PROC});
        }

        Debug::log(LOG, "Applied {} rule arguments for exec.", RULESLIST.size());
    }
}

uint64_t CKeybindManager::spawnRaw(std::string args) {
    Debug::log(LOG, "Executing {}", args);

    int socket[2];
    if (pipe(socket) != 0) {
        Debug::log(LOG, "Unable to create pipe for fork");
    }

    pid_t child, grandchild;
    child = fork();
    if (child < 0) {
        close(socket[0]);
        close(socket[1]);
        Debug::log(LOG, "Fail to create the first fork");
        return 0;
    }
    if (child == 0) {
        // run in child

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

        grandchild = fork();
        if (grandchild == 0) {
            // run in grandchild
            close(socket[0]);
            close(socket[1]);
            execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
            // exit grandchild
            _exit(0);
        }
        close(socket[0]);
        write(socket[1], &grandchild, sizeof(grandchild));
        close(socket[1]);
        // exit child
        _exit(0);
    }
    // run in parent
    close(socket[1]);
    read(socket[0], &grandchild, sizeof(grandchild));
    close(socket[0]);
    // clear child and leave grandchild to init
    waitpid(child, NULL, 0);
    if (grandchild < 0) {
        Debug::log(LOG, "Fail to create the second fork");
        return 0;
    }

    Debug::log(LOG, "Process Created with pid {}", grandchild);

    return grandchild;
}

void CKeybindManager::killActive(std::string args) {
    g_pCompositor->closeWindow(g_pCompositor->m_pLastWindow);
}

void CKeybindManager::kill(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(args);

    if (!PWINDOW) {
        Debug::log(ERR, "kill: no window found");
        return;
    }

    g_pCompositor->closeWindow(PWINDOW);
}

void CKeybindManager::clearKeybinds() {
    m_lKeybinds.clear();
}

void CKeybindManager::toggleActiveFloating(std::string args) {
    return toggleActiveFloatingCore(args, std::nullopt);
}

void CKeybindManager::setActiveFloating(std::string args) {
    return toggleActiveFloatingCore(args, true);
}

void CKeybindManager::setActiveTiled(std::string args) {
    return toggleActiveFloatingCore(args, false);
}

void toggleActiveFloatingCore(std::string args, std::optional<bool> floatState) {
    CWindow* PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    if (floatState.has_value() && floatState == PWINDOW->m_bIsFloating)
        return;

    // remove drag status
    g_pInputManager->currentlyDraggedWindow = nullptr;

    if (PWINDOW->m_sGroupData.pNextWindow && PWINDOW->m_sGroupData.pNextWindow != PWINDOW) {
        const auto PCURRENT = PWINDOW->getGroupCurrent();

        PCURRENT->m_bIsFloating = !PCURRENT->m_bIsFloating;
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PCURRENT);

        CWindow* curr = PCURRENT->m_sGroupData.pNextWindow;
        while (curr != PCURRENT) {
            curr->m_bIsFloating = PCURRENT->m_bIsFloating;
            curr->updateDynamicRules();
            curr->updateSpecialRenderData();
            curr = curr->m_sGroupData.pNextWindow;
        }
    } else {
        PWINDOW->m_bIsFloating = !PWINDOW->m_bIsFloating;

        PWINDOW->updateDynamicRules();

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PWINDOW);
    }
}

void CKeybindManager::centerWindow(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW || !PWINDOW->m_bIsFloating || PWINDOW->m_bIsFullscreen)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    auto       RESERVEDOFFSET = Vector2D();
    if (args == "1")
        RESERVEDOFFSET = (PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight) / 2.f;

    PWINDOW->m_vRealPosition = PMONITOR->middle() - PWINDOW->m_vRealSize.goal() / 2.f + RESERVEDOFFSET;
    PWINDOW->m_vPosition     = PWINDOW->m_vRealPosition.goal();
}

void CKeybindManager::toggleActivePseudo(std::string args) {
    const auto ACTIVEWINDOW = g_pCompositor->m_pLastWindow;

    if (!ACTIVEWINDOW)
        return;

    ACTIVEWINDOW->m_bIsPseudotiled = !ACTIVEWINDOW->m_bIsPseudotiled;

    if (!ACTIVEWINDOW->m_bIsFullscreen)
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(ACTIVEWINDOW);
}

void CKeybindManager::changeworkspace(std::string args) {
    int         workspaceToChangeTo = 0;
    std::string workspaceName       = "";

    // Workspace_back_and_forth being enabled means that an attempt to switch to
    // the current workspace will instead switch to the previous.
    static auto PBACKANDFORTH         = CConfigValue<Hyprlang::INT>("binds:workspace_back_and_forth");
    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");
    static auto PWORKSPACECENTERON    = CConfigValue<Hyprlang::INT>("binds:workspace_center_on");

    const auto  PMONITOR = g_pCompositor->m_pLastMonitor;

    if (!PMONITOR)
        return;

    const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    const bool EXPLICITPREVIOUS  = args.starts_with("previous");

    if (args.starts_with("previous")) {
        // Do nothing if there's no previous workspace, otherwise switch to it.
        if (PCURRENTWORKSPACE->m_sPrevWorkspace.iID == -1) {
            Debug::log(LOG, "No previous workspace to change to");
            return;
        } else {
            workspaceToChangeTo = PCURRENTWORKSPACE->m_iID;

            if (const auto PWORKSPACETOCHANGETO = g_pCompositor->getWorkspaceByID(PCURRENTWORKSPACE->m_sPrevWorkspace.iID); PWORKSPACETOCHANGETO)
                workspaceName = PWORKSPACETOCHANGETO->m_szName;
            else
                workspaceName =
                    PCURRENTWORKSPACE->m_sPrevWorkspace.name.empty() ? std::to_string(PCURRENTWORKSPACE->m_sPrevWorkspace.iID) : PCURRENTWORKSPACE->m_sPrevWorkspace.name;
        }
    } else {
        workspaceToChangeTo = getWorkspaceIDFromString(args, workspaceName);
    }

    if (workspaceToChangeTo == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return;
    }

    const bool BISWORKSPACECURRENT = workspaceToChangeTo == PCURRENTWORKSPACE->m_iID;

    if (BISWORKSPACECURRENT && (!(*PBACKANDFORTH || EXPLICITPREVIOUS) || PCURRENTWORKSPACE->m_sPrevWorkspace.iID == -1))
        return;

    g_pInputManager->unconstrainMouse();
    g_pInputManager->m_bEmptyFocusCursorSet = false;

    auto pWorkspaceToChangeTo = g_pCompositor->getWorkspaceByID(BISWORKSPACECURRENT ? PCURRENTWORKSPACE->m_sPrevWorkspace.iID : workspaceToChangeTo);
    if (!pWorkspaceToChangeTo)
        pWorkspaceToChangeTo = g_pCompositor->createNewWorkspace(BISWORKSPACECURRENT ? PCURRENTWORKSPACE->m_sPrevWorkspace.iID : workspaceToChangeTo, PMONITOR->ID,
                                                                 BISWORKSPACECURRENT ? PCURRENTWORKSPACE->m_sPrevWorkspace.name : workspaceName);

    if (!BISWORKSPACECURRENT && pWorkspaceToChangeTo->m_bIsSpecialWorkspace) {
        PMONITOR->setSpecialWorkspace(pWorkspaceToChangeTo);
        g_pInputManager->simulateMouseMovement();
        return;
    }

    g_pInputManager->releaseAllMouseButtons();

    const auto PMONITORWORKSPACEOWNER = PMONITOR->ID == pWorkspaceToChangeTo->m_iMonitorID ? PMONITOR : g_pCompositor->getMonitorFromID(pWorkspaceToChangeTo->m_iMonitorID);

    if (!PMONITORWORKSPACEOWNER)
        return;

    g_pCompositor->setActiveMonitor(PMONITORWORKSPACEOWNER);

    PMONITORWORKSPACEOWNER->changeWorkspace(pWorkspaceToChangeTo, false, true);

    if (PMONITOR != PMONITORWORKSPACEOWNER) {
        Vector2D middle = PMONITORWORKSPACEOWNER->middle();
        if (const auto PLAST = pWorkspaceToChangeTo->getLastFocusedWindow(); PLAST) {
            g_pCompositor->focusWindow(PLAST);
            if (*PWORKSPACECENTERON == 1)
                middle = PLAST->middle();
        }
        g_pCompositor->warpCursorTo(middle);
    }

    if (BISWORKSPACECURRENT) {
        if (*PALLOWWORKSPACECYCLES)
            pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);
        else if (!EXPLICITPREVIOUS)
            pWorkspaceToChangeTo->rememberPrevWorkspace(nullptr);
    } else
        pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);

    if (!g_pInputManager->m_bLastFocusOnLS) {
        if (g_pCompositor->m_pLastFocus)
            g_pInputManager->sendMotionEventsToFocused();
        else
            g_pInputManager->simulateMouseMovement();
    }
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    PWINDOW->m_bDontSendFullscreen = false;
    if (args == "2")
        PWINDOW->m_bDontSendFullscreen = true;
    g_pCompositor->setWindowFullscreen(PWINDOW, !PWINDOW->m_bIsFullscreen, args == "1" ? FULLSCREEN_MAXIMIZED : FULLSCREEN_FULL);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {

    CWindow* PWINDOW = nullptr;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow;
    }

    if (!PWINDOW)
        return;

    // hack
    std::string workspaceName;
    const auto  WORKSPACEID = getWorkspaceIDFromString(args, workspaceName);

    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(LOG, "Invalid workspace in moveActiveToWorkspace");
        return;
    }

    if (WORKSPACEID == PWINDOW->m_iWorkspaceID) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    auto        pWorkspace            = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    CMonitor*   pMonitor              = nullptr;
    const auto  POLDWS                = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");

    g_pHyprRenderer->damageWindow(PWINDOW);

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
        pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        g_pCompositor->setActiveMonitor(pMonitor);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->m_iMonitorID, workspaceName);
        pMonitor   = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    }

    POLDWS->m_pLastFocusedWindow = g_pCompositor->getFirstWindowOnWorkspace(POLDWS->m_iID);

    if (pWorkspace->m_bIsSpecialWorkspace)
        pMonitor->setSpecialWorkspace(pWorkspace);
    else if (POLDWS->m_bIsSpecialWorkspace)
        g_pCompositor->getMonitorFromID(POLDWS->m_iMonitorID)->setSpecialWorkspace(nullptr);

    pMonitor->changeWorkspace(pWorkspace);

    g_pCompositor->focusWindow(PWINDOW);
    g_pCompositor->warpCursorTo(PWINDOW->middle());

    if (*PALLOWWORKSPACECYCLES)
        pWorkspace->rememberPrevWorkspace(POLDWS);
}

void CKeybindManager::moveActiveToWorkspaceSilent(std::string args) {
    CWindow*   PWINDOW = nullptr;

    const auto ORIGINALARGS = args;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow;
    }

    if (!PWINDOW)
        return;

    std::string workspaceName = "";

    const int   WORKSPACEID = getWorkspaceIDFromString(args, workspaceName);

    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in moveActiveToWorkspaceSilent, invalid value");
        return;
    }

    if (WORKSPACEID == PWINDOW->m_iWorkspaceID)
        return;

    g_pHyprRenderer->damageWindow(PWINDOW);

    auto       pWorkspace = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    const auto OLDMIDDLE  = PWINDOW->middle();

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->m_iMonitorID, workspaceName);
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    }

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        if (const auto PATCOORDS = g_pCompositor->vectorToWindowUnified(OLDMIDDLE, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, PWINDOW); PATCOORDS)
            g_pCompositor->focusWindow(PATCOORDS);
        else
            g_pInputManager->refocus();
    }
}

void CKeybindManager::moveFocusTo(std::string args) {
    static auto PFULLCYCLE = CConfigValue<Hyprlang::INT>("binds:movefocus_cycles_fullscreen");
    char        arg        = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move focus in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;
    if (!PLASTWINDOW) {
        tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg));
        return;
    }

    const auto PWINDOWTOCHANGETO = *PFULLCYCLE && PLASTWINDOW->m_bIsFullscreen ?
        (arg == 'd' || arg == 'b' || arg == 'r' ? g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW, true) : g_pCompositor->getPrevWindowOnWorkspace(PLASTWINDOW, true)) :
        g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);

    // Found window in direction, switch to it
    if (PWINDOWTOCHANGETO) {
        switchToWindow(PWINDOWTOCHANGETO);
        return;
    }

    Debug::log(LOG, "No window found in direction {}, looking for a monitor", arg);

    if (tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg)))
        return;

    static auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");
    if (*PNOFALLBACK)
        return;

    Debug::log(LOG, "No monitor found in direction {}, falling back to next window on current workspace", arg);

    const auto PWINDOWNEXT = g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW, true);
    if (PWINDOWNEXT)
        switchToWindow(PWINDOWNEXT);
}

void CKeybindManager::focusUrgentOrLast(std::string args) {
    const auto PWINDOWURGENT = g_pCompositor->getUrgentWindow();
    const auto PWINDOWPREV   = g_pCompositor->m_pLastWindow ? (g_pCompositor->m_vWindowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_vWindowFocusHistory[1]) :
                                                              (g_pCompositor->m_vWindowFocusHistory.empty() ? nullptr : g_pCompositor->m_vWindowFocusHistory[0]);

    if (!PWINDOWURGENT && !PWINDOWPREV)
        return;

    switchToWindow(PWINDOWURGENT ? PWINDOWURGENT : PWINDOWPREV);
}

void CKeybindManager::focusCurrentOrLast(std::string args) {
    const auto PWINDOWPREV = g_pCompositor->m_pLastWindow ? (g_pCompositor->m_vWindowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_vWindowFocusHistory[1]) :
                                                            (g_pCompositor->m_vWindowFocusHistory.empty() ? nullptr : g_pCompositor->m_vWindowFocusHistory[0]);

    if (!PWINDOWPREV)
        return;

    switchToWindow(PWINDOWPREV);
}

void CKeybindManager::swapActive(std::string args) {
    char arg = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    Debug::log(LOG, "Swapping active window in direction {}", arg);
    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;
    if (!PLASTWINDOW || PLASTWINDOW->m_bIsFullscreen)
        return;

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (!PWINDOWTOCHANGETO)
        return;

    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, PWINDOWTOCHANGETO);
    g_pCompositor->warpCursorTo(PLASTWINDOW->middle());
}

void CKeybindManager::moveActiveTo(std::string args) {
    char arg = args[0];

    if (args.starts_with("mon:")) {
        const auto PNEWMONITOR = g_pCompositor->getMonitorFromString(args.substr(4));
        if (!PNEWMONITOR)
            return;

        moveActiveToWorkspace(std::to_string(PNEWMONITOR->activeWorkspace));
        return;
    }

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (!PLASTWINDOW || PLASTWINDOW->m_bIsFullscreen)
        return;

    if (PLASTWINDOW->m_bIsFloating) {
        Vector2D   vPos;
        const auto PMONITOR   = g_pCompositor->getMonitorFromID(PLASTWINDOW->m_iMonitorID);
        const auto BORDERSIZE = PLASTWINDOW->getRealBorderSize();

        switch (arg) {
            case 'l': vPos.x = PMONITOR->vecReservedTopLeft.x + BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 'r': vPos.x = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PLASTWINDOW->m_vRealSize.goal().x - BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 't':
            case 'u': vPos.y = PMONITOR->vecReservedTopLeft.y + BORDERSIZE + PMONITOR->vecPosition.y; break;
            case 'b':
            case 'd': vPos.y = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PLASTWINDOW->m_vRealSize.goal().y - BORDERSIZE + PMONITOR->vecPosition.y; break;
        }

        PLASTWINDOW->m_vRealPosition = Vector2D(vPos.x != 0 ? vPos.x : PLASTWINDOW->m_vRealPosition.goal().x, vPos.y != 0 ? vPos.y : PLASTWINDOW->m_vRealPosition.goal().y);
        return;
    }

    // If the window to change to is on the same workspace, switch them
    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (PWINDOWTOCHANGETO) {
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PLASTWINDOW, args);
        g_pCompositor->warpCursorTo(PLASTWINDOW->middle());
        return;
    }

    // Otherwise, we always want to move to the next monitor in that direction
    const auto PMONITORTOCHANGETO = g_pCompositor->getMonitorInDirection(arg);
    if (!PMONITORTOCHANGETO)
        return;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITORTOCHANGETO->activeWorkspace);

    moveActiveToWorkspace(PWORKSPACE->getConfigName());
}

void CKeybindManager::toggleGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    g_pCompositor->setWindowFullscreen(PWINDOW, false, FULLSCREEN_FULL);

    if (!PWINDOW->m_sGroupData.pNextWindow)
        PWINDOW->createGroup();
    else
        PWINDOW->destroyGroup();

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();
}

void CKeybindManager::changeGroupActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    if (!PWINDOW->m_sGroupData.pNextWindow)
        return;

    if (PWINDOW->m_sGroupData.pNextWindow == PWINDOW)
        return;

    if (isNumber(args, false)) {
        // index starts from '1'; '0' means last window
        const int INDEX = std::stoi(args);
        if (INDEX > PWINDOW->getGroupSize())
            return;
        if (INDEX == 0)
            PWINDOW->setGroupCurrent(PWINDOW->getGroupTail());
        else
            PWINDOW->setGroupCurrent(PWINDOW->getGroupWindowByIndex(INDEX - 1));
        return;
    }

    if (args != "b" && args != "prev") {
        PWINDOW->setGroupCurrent(PWINDOW->m_sGroupData.pNextWindow);
    } else {
        PWINDOW->setGroupCurrent(PWINDOW->getGroupPrevious());
    }
}

void CKeybindManager::toggleSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow;

    if (!header.pWindow)
        return;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(header.pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow)
        return;

    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "togglesplit");
}

void CKeybindManager::swapSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow;

    if (!header.pWindow)
        return;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(header.pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow)
        return;

    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "swapsplit");
}

void CKeybindManager::alterSplitRatio(std::string args) {
    std::optional<float> splitResult;
    bool                 exact = false;

    if (args.starts_with("exact")) {
        exact       = true;
        splitResult = getPlusMinusKeywordResult(args.substr(5), 0);
    } else
        splitResult = getPlusMinusKeywordResult(args, 0);

    if (!splitResult.has_value()) {
        Debug::log(ERR, "Splitratio invalid in alterSplitRatio!");
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (!PLASTWINDOW)
        return;

    g_pLayoutManager->getCurrentLayout()->alterSplitRatio(PLASTWINDOW, splitResult.value(), exact);
}

void CKeybindManager::focusMonitor(std::string arg) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(arg);
    tryMoveFocusToMonitor(PMONITOR);
}

void CKeybindManager::moveCursorToCorner(std::string arg) {
    if (!isNumber(arg)) {
        Debug::log(ERR, "moveCursorToCorner, arg has to be a number.");
        return;
    }

    const auto CORNER = std::stoi(arg);

    if (CORNER < 0 || CORNER > 3) {
        Debug::log(ERR, "moveCursorToCorner, corner not 0 - 3.");
        return;
    }

    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    switch (CORNER) {
        case 0:
            // bottom left
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.value().x,
                            PWINDOW->m_vRealPosition.value().y + PWINDOW->m_vRealSize.value().y);
            break;
        case 1:
            // bottom right
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.value().x + PWINDOW->m_vRealSize.value().x,
                            PWINDOW->m_vRealPosition.value().y + PWINDOW->m_vRealSize.value().y);
            break;
        case 2:
            // top right
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.value().x + PWINDOW->m_vRealSize.value().x,
                            PWINDOW->m_vRealPosition.value().y);
            break;
        case 3:
            // top left
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.value().x, PWINDOW->m_vRealPosition.value().y);
            break;
    }
}

void CKeybindManager::moveCursor(std::string args) {
    std::string x_str, y_str;
    int         x, y;

    size_t      i = args.find_first_of(' ');
    if (i == std::string::npos) {
        Debug::log(ERR, "moveCursor, takes 2 arguments.");
        return;
    }

    x_str = args.substr(0, i);
    y_str = args.substr(i + 1);

    if (!isNumber(x_str)) {
        Debug::log(ERR, "moveCursor, x argument has to be a number.");
        return;
    }
    if (!isNumber(y_str)) {
        Debug::log(ERR, "moveCursor, y argument has to be a number.");
        return;
    }

    x = std::stoi(x_str);
    y = std::stoi(y_str);

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, x, y);
}

void CKeybindManager::workspaceOpt(std::string args) {

    // current workspace
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!PWORKSPACE)
        return; // ????

    if (args == "allpseudo") {
        PWORKSPACE->m_bDefaultPseudo = !PWORKSPACE->m_bDefaultPseudo;

        // apply
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->m_iWorkspaceID != PWORKSPACE->m_iID)
                continue;

            w->m_bIsPseudotiled = PWORKSPACE->m_bDefaultPseudo;
        }
    } else if (args == "allfloat") {
        PWORKSPACE->m_bDefaultFloating = !PWORKSPACE->m_bDefaultFloating;
        // apply

        // we make a copy because changeWindowFloatingMode might invalidate the iterator
        std::deque<CWindow*> ptrs;
        for (auto& w : g_pCompositor->m_vWindows)
            ptrs.push_back(w.get());

        for (auto& w : ptrs) {
            if (!w->m_bIsMapped || w->m_iWorkspaceID != PWORKSPACE->m_iID || w->isHidden())
                continue;

            if (!w->m_bRequestsFloat && w->m_bIsFloating != PWORKSPACE->m_bDefaultFloating) {
                const auto SAVEDPOS  = w->m_vRealPosition.value();
                const auto SAVEDSIZE = w->m_vRealSize.value();

                w->m_bIsFloating = PWORKSPACE->m_bDefaultFloating;
                g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(w);

                if (PWORKSPACE->m_bDefaultFloating) {
                    w->m_vRealPosition.setValueAndWarp(SAVEDPOS);
                    w->m_vRealSize.setValueAndWarp(SAVEDSIZE);
                    g_pXWaylandManager->setWindowSize(w, SAVEDSIZE);
                    w->m_vRealSize     = w->m_vRealSize.value() + Vector2D(4, 4);
                    w->m_vRealPosition = w->m_vRealPosition.value() - Vector2D(2, 2);
                }
            }
        }
    } else {
        Debug::log(ERR, "Invalid arg in workspaceOpt, opt \"{}\" doesn't exist.", args);
        return;
    }

    // recalc mon
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(g_pCompositor->m_pLastMonitor->ID);
}

void CKeybindManager::renameWorkspace(std::string args) {
    try {
        const auto FIRSTSPACEPOS = args.find_first_of(' ');
        if (FIRSTSPACEPOS != std::string::npos) {
            int         workspace = std::stoi(args.substr(0, FIRSTSPACEPOS));
            std::string name      = args.substr(FIRSTSPACEPOS + 1);
            g_pCompositor->renameWorkspace(workspace, name);
        } else {
            g_pCompositor->renameWorkspace(std::stoi(args), "");
        }
    } catch (std::exception& e) { Debug::log(ERR, "Invalid arg in renameWorkspace, expected numeric id only or a numeric id and string name. \"{}\": \"{}\"", args, e.what()); }
}

void CKeybindManager::exitHyprland(std::string argz) {
    g_pInputManager->m_bExitTriggered = true;
}

void CKeybindManager::moveCurrentWorkspaceToMonitor(std::string args) {
    CMonitor* PMONITOR = g_pCompositor->getMonitorFromString(args);

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveCurrentWorkspaceToMonitor: monitor doesnt exist");
        return;
    }

    // get the current workspace
    const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!PCURRENTWORKSPACE) {
        Debug::log(ERR, "moveCurrentWorkspaceToMonitor invalid workspace!");
        return;
    }

    g_pCompositor->moveWorkspaceToMonitor(PCURRENTWORKSPACE, PMONITOR);
}

void CKeybindManager::moveWorkspaceToMonitor(std::string args) {
    if (!args.contains(' '))
        return;

    std::string workspace = args.substr(0, args.find_first_of(' '));
    std::string monitor   = args.substr(args.find_first_of(' ') + 1);

    const auto  PMONITOR = g_pCompositor->getMonitorFromString(monitor);

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveWorkspaceToMonitor: monitor doesnt exist");
        return;
    }

    std::string workspaceName;
    const int   WORKSPACEID = getWorkspaceIDFromString(workspace, workspaceName);

    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(ERR, "moveWorkspaceToMonitor invalid workspace!");
        return;
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    if (!PWORKSPACE) {
        Debug::log(ERR, "moveWorkspaceToMonitor workspace doesn't exist!");
        return;
    }

    g_pCompositor->moveWorkspaceToMonitor(PWORKSPACE, PMONITOR);
}

void CKeybindManager::focusWorkspaceOnCurrentMonitor(std::string args) {
    std::string workspaceName;
    int         workspaceID = getWorkspaceIDFromString(args, workspaceName);

    if (workspaceID == WORKSPACE_INVALID) {
        Debug::log(ERR, "focusWorkspaceOnCurrentMonitor invalid workspace!");
        return;
    }

    const auto PCURRMONITOR = g_pCompositor->m_pLastMonitor;

    if (!PCURRMONITOR) {
        Debug::log(ERR, "focusWorkspaceOnCurrentMonitor monitor doesn't exist!");
        return;
    }

    auto pWorkspace = g_pCompositor->getWorkspaceByID(workspaceID);

    if (!pWorkspace) {
        pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, PCURRMONITOR->ID);
        // we can skip the moving, since it's already on the current monitor
        changeworkspace(pWorkspace->getConfigName());
        return;
    }

    static auto PBACKANDFORTH = CConfigValue<Hyprlang::INT>("binds:workspace_back_and_forth");

    if (*PBACKANDFORTH && PCURRMONITOR->activeWorkspace == workspaceID && pWorkspace->m_sPrevWorkspace.iID != -1) {
        const int  PREVWORKSPACEID   = pWorkspace->m_sPrevWorkspace.iID;
        const auto PREVWORKSPACENAME = pWorkspace->m_sPrevWorkspace.name;
        // Workspace to focus is previous workspace
        pWorkspace = g_pCompositor->getWorkspaceByID(PREVWORKSPACEID);
        if (!pWorkspace)
            pWorkspace = g_pCompositor->createNewWorkspace(PREVWORKSPACEID, PCURRMONITOR->ID, PREVWORKSPACENAME);

        workspaceID = pWorkspace->m_iID;
    }

    if (pWorkspace->m_iMonitorID != PCURRMONITOR->ID) {
        const auto POLDMONITOR = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        if (!POLDMONITOR) { // wat
            Debug::log(ERR, "focusWorkspaceOnCurrentMonitor old monitor doesn't exist!");
            return;
        }
        if (POLDMONITOR->activeWorkspace == workspaceID) {
            g_pCompositor->swapActiveWorkspaces(POLDMONITOR, PCURRMONITOR);
            return;
        } else {
            g_pCompositor->moveWorkspaceToMonitor(pWorkspace, PCURRMONITOR, true);
        }
    }

    changeworkspace(pWorkspace->getConfigName());
}

void CKeybindManager::toggleSpecialWorkspace(std::string args) {

    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");

    std::string workspaceName = "";
    int         workspaceID   = getWorkspaceIDFromString("special:" + args, workspaceName);

    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID)) {
        Debug::log(ERR, "Invalid workspace passed to special");
        return;
    }

    bool       requestedWorkspaceIsAlreadyOpen = false;
    const auto PMONITOR                        = *PFOLLOWMOUSE == 1 ? g_pCompositor->getMonitorFromCursor() : g_pCompositor->m_pLastMonitor;
    int        specialOpenOnMonitor            = PMONITOR->specialWorkspaceID;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->specialWorkspaceID == workspaceID) {
            requestedWorkspaceIsAlreadyOpen = true;
            break;
        }
    }

    if (requestedWorkspaceIsAlreadyOpen && specialOpenOnMonitor == workspaceID) {
        // already open on this monitor
        Debug::log(LOG, "Toggling special workspace {} to closed", workspaceID);
        PMONITOR->setSpecialWorkspace(nullptr);
    } else {
        Debug::log(LOG, "Toggling special workspace {} to open", workspaceID);
        auto PSPECIALWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceID);

        if (!PSPECIALWORKSPACE)
            PSPECIALWORKSPACE = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->ID, workspaceName);

        PMONITOR->setSpecialWorkspace(PSPECIALWORKSPACE);
    }
}

void CKeybindManager::forceRendererReload(std::string args) {
    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (!m->output)
            continue;

        auto rule = g_pConfigManager->getMonitorRuleFor(*m);
        if (!g_pHyprRenderer->applyMonitorRule(m.get(), &rule, true)) {
            overAgain = true;
            break;
        }
    }

    if (overAgain)
        forceRendererReload(args);
}

void CKeybindManager::resizeActive(std::string args) {
    if (!g_pCompositor->m_pLastWindow || g_pCompositor->m_pLastWindow->m_bIsFullscreen)
        return;

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(args, g_pCompositor->m_pLastWindow->m_vRealSize.goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return;

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - g_pCompositor->m_pLastWindow->m_vRealSize.goal());

    if (g_pCompositor->m_pLastWindow->m_vRealSize.goal().x > 1 && g_pCompositor->m_pLastWindow->m_vRealSize.goal().y > 1)
        g_pCompositor->m_pLastWindow->setHidden(false);
}

void CKeybindManager::moveActive(std::string args) {
    if (!g_pCompositor->m_pLastWindow || g_pCompositor->m_pLastWindow->m_bIsFullscreen)
        return;

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(args, g_pCompositor->m_pLastWindow->m_vRealPosition.goal());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - g_pCompositor->m_pLastWindow->m_vRealPosition.goal());
}

void CKeybindManager::moveWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "moveWindow: no window");
        return;
    }

    if (PWINDOW->m_bIsFullscreen)
        return;

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_vRealPosition.goal());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - PWINDOW->m_vRealPosition.goal(), PWINDOW);
}

void CKeybindManager::resizeWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "resizeWindow: no window");
        return;
    }

    if (PWINDOW->m_bIsFullscreen)
        return;

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_vRealSize.goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return;

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PWINDOW->m_vRealSize.goal(), CORNER_NONE, PWINDOW);

    if (PWINDOW->m_vRealSize.goal().x > 1 && PWINDOW->m_vRealSize.goal().y > 1)
        PWINDOW->setHidden(false);
}

void CKeybindManager::circleNext(std::string arg) {

    if (!g_pCompositor->m_pLastWindow) {
        // if we have a clear focus, find the first window and get the next focusable.
        if (g_pCompositor->getWindowsOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace) > 0) {
            const auto PWINDOW = g_pCompositor->getFirstWindowOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace);

            switchToWindow(PWINDOW);
        }

        return;
    }

    CVarList            args{arg, 0, 's', true};

    std::optional<bool> floatStatus = {};
    if (args.contains("tile") || args.contains("tiled"))
        floatStatus = false;
    else if (args.contains("float") || args.contains("floating"))
        floatStatus = true;

    if (args.contains("prev") || args.contains("p") || args.contains("last") || args.contains("l"))
        switchToWindow(g_pCompositor->getPrevWindowOnWorkspace(g_pCompositor->m_pLastWindow, true, floatStatus));
    else
        switchToWindow(g_pCompositor->getNextWindowOnWorkspace(g_pCompositor->m_pLastWindow, true, floatStatus));
}

void CKeybindManager::focusWindow(std::string regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW)
        return;

    Debug::log(LOG, "Focusing to window name: {}", PWINDOW->m_szTitle);

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
    if (!PWORKSPACE) {
        Debug::log(ERR, "BUG THIS: null workspace in focusWindow");
        return;
    }

    if (g_pCompositor->m_pLastMonitor->activeWorkspace != PWINDOW->m_iWorkspaceID) {
        Debug::log(LOG, "Fake executing workspace to move focus");
        changeworkspace(PWORKSPACE->getConfigName());
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        const auto FSWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        const auto FSMODE   = PWORKSPACE->m_efFullscreenMode;

        if (PWINDOW->m_bIsFloating) {
            // don't make floating implicitly fs
            if (!PWINDOW->m_bCreatedOverFullscreen) {
                g_pCompositor->changeWindowZOrder(PWINDOW, true);
                g_pCompositor->updateFullscreenFadeOnWorkspace(PWORKSPACE);
            }

            g_pCompositor->focusWindow(PWINDOW);
        } else {
            if (FSWINDOW != PWINDOW && !PWINDOW->m_bPinned)
                g_pCompositor->setWindowFullscreen(FSWINDOW, false, FULLSCREEN_FULL);

            g_pCompositor->focusWindow(PWINDOW);

            if (FSWINDOW != PWINDOW && !PWINDOW->m_bPinned)
                g_pCompositor->setWindowFullscreen(PWINDOW, true, FSMODE);
        }
    } else
        g_pCompositor->focusWindow(PWINDOW);

    g_pCompositor->warpCursorTo(PWINDOW->middle());
}

void CKeybindManager::setSubmap(std::string submap) {
    if (submap == "reset" || submap == "") {
        m_szCurrentSelectedSubmap = "";
        Debug::log(LOG, "Reset active submap to the default one.");
        g_pEventManager->postEvent(SHyprIPCEvent{"submap", ""});
        EMIT_HOOK_EVENT("submap", m_szCurrentSelectedSubmap);
        return;
    }

    for (auto& k : g_pKeybindManager->m_lKeybinds) {
        if (k.submap == submap) {
            m_szCurrentSelectedSubmap = submap;
            Debug::log(LOG, "Changed keybind submap to {}", submap);
            g_pEventManager->postEvent(SHyprIPCEvent{"submap", submap});
            EMIT_HOOK_EVENT("submap", m_szCurrentSelectedSubmap);
            return;
        }
    }

    Debug::log(ERR, "Cannot set submap {}, submap doesn't exist (wasn't registered!)", submap);
}

void CKeybindManager::pass(std::string regexp) {

    // find the first window passing the regex
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW) {
        Debug::log(ERR, "pass: window not found");
        return;
    }

    const auto PLASTSRF = g_pCompositor->m_pLastFocus;

    const auto KEYBOARD = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

    if (!KEYBOARD) {
        Debug::log(ERR, "No kb in pass?");
        return;
    }

    const auto XWTOXW       = PWINDOW->m_bIsX11 && g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsX11;
    const auto SL           = Vector2D(g_pCompositor->m_sSeat.seat->pointer_state.sx, g_pCompositor->m_sSeat.seat->pointer_state.sy);
    uint32_t   keycodes[32] = {0};

    // pass all mf shit
    if (!XWTOXW) {
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_enter(g_pCompositor->m_sSeat.seat, PWINDOW->m_pWLSurface.wlr(), keycodes, 0, &KEYBOARD->modifiers);
        else
            wlr_seat_pointer_enter(g_pCompositor->m_sSeat.seat, PWINDOW->m_pWLSurface.wlr(), 1, 1);
    }

    wlr_keyboard_modifiers kbmods = {g_pInputManager->accumulateModsFromAllKBs(), 0, 0, 0};
    wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &kbmods);

    if (g_pKeybindManager->m_iPassPressed == 1) {
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_PRESSED);
        else
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
    } else if (g_pKeybindManager->m_iPassPressed == 0)
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_RELEASED);
        else
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
    else {
        // dynamic call of the dispatcher
        if (g_pKeybindManager->m_uLastCode != 0) {
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_PRESSED);
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_RELEASED);
        } else {
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    if (XWTOXW)
        return;

    // Massive hack:
    // this will make wlroots NOT send the leave event to XWayland apps, provided we are not on an XWayland window already.
    // please kill me
    if (PWINDOW->m_bIsX11) {
        if (g_pKeybindManager->m_uLastCode != 0) {
            g_pCompositor->m_sSeat.seat->keyboard_state.focused_client  = nullptr;
            g_pCompositor->m_sSeat.seat->keyboard_state.focused_surface = nullptr;
        } else {
            g_pCompositor->m_sSeat.seat->pointer_state.focused_client  = nullptr;
            g_pCompositor->m_sSeat.seat->pointer_state.focused_surface = nullptr;
        }
    }

    if (g_pKeybindManager->m_uLastCode != 0)
        wlr_seat_keyboard_enter(g_pCompositor->m_sSeat.seat, PLASTSRF, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);
    else
        wlr_seat_pointer_enter(g_pCompositor->m_sSeat.seat, PWINDOW->m_pWLSurface.wlr(), SL.x, SL.y);
}

void CKeybindManager::layoutmsg(std::string msg) {
    SLayoutMessageHeader hd = {g_pCompositor->m_pLastWindow};
    g_pLayoutManager->getCurrentLayout()->layoutMessage(hd, msg);
}

void CKeybindManager::toggleOpaque(std::string unused) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW)
        return;

    PWINDOW->m_sAdditionalConfigData.forceOpaque           = !PWINDOW->m_sAdditionalConfigData.forceOpaque;
    PWINDOW->m_sAdditionalConfigData.forceOpaqueOverridden = true;

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void CKeybindManager::dpms(std::string arg) {
    bool        enable = arg.starts_with("on");
    std::string port   = "";

    if (arg.starts_with("toggle"))
        enable = !std::any_of(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](const auto& other) { return !other->dpmsStatus; }); // enable if any is off

    if (arg.find_first_of(' ') != std::string::npos)
        port = arg.substr(arg.find_first_of(' ') + 1);

    for (auto& m : g_pCompositor->m_vMonitors) {

        if (!port.empty() && m->szName != port)
            continue;

        wlr_output_state_set_enabled(m->state.wlr(), enable);

        m->dpmsStatus = enable;

        if (!m->state.commit()) {
            Debug::log(ERR, "Couldn't commit output {}", m->szName);
        }

        if (enable)
            g_pHyprRenderer->damageMonitor(m.get());
    }

    g_pCompositor->m_bDPMSStateON = enable;
}

void CKeybindManager::swapnext(std::string arg) {

    CWindow* toSwap = nullptr;

    if (!g_pCompositor->m_pLastWindow)
        return;

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    const auto PLASTCYCLED = g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow->m_pLastCycledWindow) &&
            g_pCompositor->m_pLastWindow->m_pLastCycledWindow->m_iWorkspaceID == PLASTWINDOW->m_iWorkspaceID ?
        g_pCompositor->m_pLastWindow->m_pLastCycledWindow :
        nullptr;

    if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
        toSwap = g_pCompositor->getPrevWindowOnWorkspace(PLASTCYCLED ? PLASTCYCLED : PLASTWINDOW, true);
    else
        toSwap = g_pCompositor->getNextWindowOnWorkspace(PLASTCYCLED ? PLASTCYCLED : PLASTWINDOW, true);

    // sometimes we may come back to ourselves.
    if (toSwap == PLASTWINDOW) {
        if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
            toSwap = g_pCompositor->getPrevWindowOnWorkspace(PLASTWINDOW), true;
        else
            toSwap = g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW, true);
    }

    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, toSwap);

    PLASTWINDOW->m_pLastCycledWindow = toSwap;

    g_pCompositor->focusWindow(PLASTWINDOW);
}

void CKeybindManager::swapActiveWorkspaces(std::string args) {
    const auto MON1 = args.substr(0, args.find_first_of(' '));
    const auto MON2 = args.substr(args.find_first_of(' ') + 1);

    const auto PMON1 = g_pCompositor->getMonitorFromString(MON1);
    const auto PMON2 = g_pCompositor->getMonitorFromString(MON2);

    if (!PMON1 || !PMON2 || PMON1 == PMON2)
        return;

    g_pCompositor->swapActiveWorkspaces(PMON1, PMON2);
}

void CKeybindManager::pinActive(std::string args) {

    CWindow* PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW) {
        Debug::log(ERR, "pin: window not found");
        return;
    }

    if (!PWINDOW->m_bIsFloating || PWINDOW->m_bIsFullscreen)
        return;

    PWINDOW->m_bPinned      = !PWINDOW->m_bPinned;
    PWINDOW->m_iWorkspaceID = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID)->activeWorkspace;

    PWINDOW->updateDynamicRules();
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    PWORKSPACE->m_pLastFocusedWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS);

    g_pEventManager->postEvent(SHyprIPCEvent{"pin", std::format("{:x},{}", (uintptr_t)PWINDOW, (int)PWINDOW->m_bPinned)});
    EMIT_HOOK_EVENT("pin", PWINDOW);
}

void CKeybindManager::mouse(std::string args) {
    const auto ARGS    = CVarList(args.substr(1), 2, ' ');
    const auto PRESSED = args[0] == '1';

    if (ARGS[0] == "movewindow") {
        if (PRESSED) {
            g_pKeybindManager->m_bIsMouseBindActive = true;

            const auto mouseCoords = g_pInputManager->getMouseCoordsInternal();
            CWindow*   pWindow     = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

            if (pWindow && !pWindow->m_bIsFullscreen)
                pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_START, mouseCoords);

            if (!g_pInputManager->currentlyDraggedWindow)
                g_pInputManager->currentlyDraggedWindow = pWindow;

            g_pInputManager->dragMode = MBIND_MOVE;
            g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();
        } else {
            g_pKeybindManager->m_bIsMouseBindActive = false;

            if (g_pInputManager->currentlyDraggedWindow) {
                g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
                g_pInputManager->currentlyDraggedWindow = nullptr;
                g_pInputManager->dragMode               = MBIND_INVALID;
            }
        }
    } else if (ARGS[0] == "resizewindow") {
        if (PRESSED) {
            g_pKeybindManager->m_bIsMouseBindActive = true;

            g_pInputManager->currentlyDraggedWindow =
                g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

            try {
                switch (std::stoi(ARGS[1])) {
                    case 1: g_pInputManager->dragMode = MBIND_RESIZE_FORCE_RATIO; break;
                    case 2: g_pInputManager->dragMode = MBIND_RESIZE_BLOCK_RATIO; break;
                    default: g_pInputManager->dragMode = MBIND_RESIZE;
                }
            } catch (std::exception& e) { g_pInputManager->dragMode = MBIND_RESIZE; }
            g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();
        } else {
            g_pKeybindManager->m_bIsMouseBindActive = false;

            if (g_pInputManager->currentlyDraggedWindow) {
                g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
                g_pInputManager->currentlyDraggedWindow = nullptr;
                g_pInputManager->dragMode               = MBIND_INVALID;
            }
        }
    }
}

void CKeybindManager::bringActiveToTop(std::string args) {
    if (g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsFloating)
        g_pCompositor->changeWindowZOrder(g_pCompositor->m_pLastWindow, true);
}

void CKeybindManager::alterZOrder(std::string args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto POSITION    = args.substr(0, args.find_first_of(','));
    auto       PWINDOW     = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW && g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsFloating)
        PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW) {
        Debug::log(ERR, "alterZOrder: no window");
        return;
    }

    if (POSITION == "top")
        g_pCompositor->changeWindowZOrder(PWINDOW, 1);
    else if (POSITION == "bottom")
        g_pCompositor->changeWindowZOrder(PWINDOW, 0);
    else {
        Debug::log(ERR, "alterZOrder: bad position: {}", POSITION);
        return;
    }

    g_pInputManager->simulateMouseMovement();
}

void CKeybindManager::fakeFullscreenActive(std::string args) {
    if (g_pCompositor->m_pLastWindow) {
        // will also set the flag
        g_pCompositor->m_pLastWindow->m_bFakeFullscreenState = !g_pCompositor->m_pLastWindow->m_bFakeFullscreenState;
        g_pXWaylandManager->setWindowFullscreen(g_pCompositor->m_pLastWindow, g_pCompositor->m_pLastWindow->shouldSendFullscreenState());
    }
}

void CKeybindManager::lockGroups(std::string args) {
    if (args == "lock" || args.empty() || args == "lockgroups")
        g_pKeybindManager->m_bGroupsLocked = true;
    else if (args == "toggle")
        g_pKeybindManager->m_bGroupsLocked = !g_pKeybindManager->m_bGroupsLocked;
    else
        g_pKeybindManager->m_bGroupsLocked = false;

    g_pEventManager->postEvent(SHyprIPCEvent{"lockgroups", g_pKeybindManager->m_bGroupsLocked ? "1" : "0"});
}

void CKeybindManager::lockActiveGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW || !PWINDOW->m_sGroupData.pNextWindow)
        return;

    const auto PHEAD = PWINDOW->getGroupHead();

    if (args == "lock")
        PHEAD->m_sGroupData.locked = true;
    else if (args == "toggle")
        PHEAD->m_sGroupData.locked = !PHEAD->m_sGroupData.locked;
    else
        PHEAD->m_sGroupData.locked = false;

    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
}

void CKeybindManager::moveWindowIntoGroup(CWindow* pWindow, CWindow* pWindowInDirection) {
    if (pWindow->m_sGroupData.deny)
        return;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow); // This removes groupped property!

    static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
    pWindowInDirection     = *USECURRPOS ? pWindowInDirection : pWindowInDirection->getGroupTail();

    pWindowInDirection->insertWindowToGroup(pWindow);
    pWindowInDirection->setGroupCurrent(pWindow);
    pWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pWindow);
    g_pCompositor->focusWindow(pWindow);
    g_pCompositor->warpCursorTo(pWindow->middle());

    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));
}

void CKeybindManager::moveWindowOutOfGroup(CWindow* pWindow, const std::string& dir) {
    static auto BFOCUSREMOVEDWINDOW = CConfigValue<Hyprlang::INT>("group:focus_removed_window");
    const auto  PWINDOWPREV         = pWindow->getGroupPrevious();
    eDirection  direction;

    switch (dir[0]) {
        case 't':
        case 'u': direction = DIRECTION_UP; break;
        case 'd':
        case 'b': direction = DIRECTION_DOWN; break;
        case 'l': direction = DIRECTION_LEFT; break;
        case 'r': direction = DIRECTION_RIGHT; break;
        default: direction = DIRECTION_DEFAULT;
    }

    if (pWindow->m_sGroupData.pNextWindow == pWindow) {
        pWindow->destroyGroup();
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow);

        const auto GROUPSLOCKEDPREV        = g_pKeybindManager->m_bGroupsLocked;
        g_pKeybindManager->m_bGroupsLocked = true;

        g_pLayoutManager->getCurrentLayout()->onWindowCreated(pWindow, direction);

        g_pKeybindManager->m_bGroupsLocked = GROUPSLOCKEDPREV;
    }

    if (*BFOCUSREMOVEDWINDOW) {
        g_pCompositor->focusWindow(pWindow);
        g_pCompositor->warpCursorTo(pWindow->middle());
    } else {
        g_pCompositor->focusWindow(PWINDOWPREV);
        g_pCompositor->warpCursorTo(PWINDOWPREV->middle());
    }
}

void CKeybindManager::moveIntoGroup(std::string args) {
    char        arg = args[0];

    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked)
        return;

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW || PWINDOW->m_bIsFloating || PWINDOW->m_sGroupData.deny)
        return;

    auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    if (!PWINDOWINDIR || !PWINDOWINDIR->m_sGroupData.pNextWindow)
        return;

    // Do not move window into locked group if binds:ignore_group_lock is false
    if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_sGroupData.locked || (PWINDOW->m_sGroupData.pNextWindow && PWINDOW->getGroupHead()->m_sGroupData.locked)))
        return;

    moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);
}

void CKeybindManager::moveOutOfGroup(std::string args) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked)
        return;

    CWindow* PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW || !PWINDOW->m_sGroupData.pNextWindow)
        return;

    moveWindowOutOfGroup(PWINDOW);
}

void CKeybindManager::moveWindowOrGroup(std::string args) {
    char        arg = args[0];

    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PWINDOW = g_pCompositor->m_pLastWindow;
    if (!PWINDOW || PWINDOW->m_bIsFullscreen)
        return;

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked) {
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
        return;
    }

    const auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    const bool ISWINDOWGROUP       = PWINDOW->m_sGroupData.pNextWindow;
    const bool ISWINDOWGROUPLOCKED = ISWINDOWGROUP && PWINDOW->getGroupHead()->m_sGroupData.locked;
    const bool ISWINDOWGROUPSINGLE = ISWINDOWGROUP && PWINDOW->m_sGroupData.pNextWindow == PWINDOW;

    // note: PWINDOWINDIR is not null implies !PWINDOW->m_bIsFloating
    if (PWINDOWINDIR && PWINDOWINDIR->m_sGroupData.pNextWindow) { // target is group
        if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_sGroupData.locked || ISWINDOWGROUPLOCKED || PWINDOW->m_sGroupData.deny)) {
            g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
            g_pCompositor->warpCursorTo(PWINDOW->middle());
        } else
            moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);
    } else if (PWINDOWINDIR) { // target is regular window
        if ((!*PIGNOREGROUPLOCK && ISWINDOWGROUPLOCKED) || !ISWINDOWGROUP || (ISWINDOWGROUPSINGLE && PWINDOW->m_eGroupRules & GROUP_SET_ALWAYS)) {
            g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
            g_pCompositor->warpCursorTo(PWINDOW->middle());
        } else
            moveWindowOutOfGroup(PWINDOW, args);
    } else if ((*PIGNOREGROUPLOCK || !ISWINDOWGROUPLOCKED) && ISWINDOWGROUP) { // no target window
        moveWindowOutOfGroup(PWINDOW, args);
    } else if (!PWINDOWINDIR && !ISWINDOWGROUP) { // no target in dir and not in group
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
        g_pCompositor->warpCursorTo(PWINDOW->middle());
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
}

void CKeybindManager::setIgnoreGroupLock(std::string args) {
    static auto PIGNOREGROUPLOCK = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("binds:ignore_group_lock");

    if (args == "toggle")
        **PIGNOREGROUPLOCK = !**PIGNOREGROUPLOCK;
    else
        **PIGNOREGROUPLOCK = args == "on";

    g_pEventManager->postEvent(SHyprIPCEvent{"ignoregrouplock", std::to_string(**PIGNOREGROUPLOCK)});
}

void CKeybindManager::denyWindowFromGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;
    if (!PWINDOW || (PWINDOW && PWINDOW->m_sGroupData.pNextWindow))
        return;

    if (args == "toggle")
        PWINDOW->m_sGroupData.deny = !PWINDOW->m_sGroupData.deny;
    else
        PWINDOW->m_sGroupData.deny = args == "on";

    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
}

void CKeybindManager::global(std::string args) {
    const auto APPID = args.substr(0, args.find_first_of(':'));
    const auto NAME  = args.substr(args.find_first_of(':') + 1);

    if (NAME.empty())
        return;

    if (!g_pProtocolManager->m_pGlobalShortcutsProtocolManager->globalShortcutExists(APPID, NAME))
        return;

    g_pProtocolManager->m_pGlobalShortcutsProtocolManager->sendGlobalShortcutEvent(APPID, NAME, g_pKeybindManager->m_iPassPressed);
}

void CKeybindManager::moveGroupWindow(std::string args) {
    const auto BACK = args == "b" || args == "prev";

    if (!g_pCompositor->m_pLastWindow || !g_pCompositor->m_pLastWindow->m_sGroupData.pNextWindow)
        return;

    if ((!BACK && g_pCompositor->m_pLastWindow->m_sGroupData.pNextWindow->m_sGroupData.head) || (BACK && g_pCompositor->m_pLastWindow->m_sGroupData.head)) {
        std::swap(g_pCompositor->m_pLastWindow->m_sGroupData.head, g_pCompositor->m_pLastWindow->m_sGroupData.pNextWindow->m_sGroupData.head);
        std::swap(g_pCompositor->m_pLastWindow->m_sGroupData.locked, g_pCompositor->m_pLastWindow->m_sGroupData.pNextWindow->m_sGroupData.locked);
    } else
        g_pCompositor->m_pLastWindow->switchWithWindowInGroup(BACK ? g_pCompositor->m_pLastWindow->getGroupPrevious() : g_pCompositor->m_pLastWindow->m_sGroupData.pNextWindow);

    g_pCompositor->m_pLastWindow->updateWindowDecos();
}
