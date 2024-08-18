#include "../config/ConfigValue.hpp"
#include "../devices/IKeyboard.hpp"
#include "../managers/SeatManager.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/ShortcutsInhibit.hpp"
#include "../protocols/GlobalShortcuts.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../devices/IKeyboard.hpp"
#include "KeybindManager.hpp"
#include "PointerManager.hpp"
#include "Compositor.hpp"
#include "TokenManager.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "debug/Log.hpp"
#include "helpers/varlist/VarList.hpp"

#include <optional>
#include <iterator>
#include <string>
#include <string_view>
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

static std::vector<std::pair<std::string, std::string>> getHyprlandLaunchEnv() {
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!*PINITIALWSTRACKING || g_pConfigManager->isLaunchingExecOnce)
        return {};

    const auto PMONITOR = g_pCompositor->m_pLastMonitor;
    if (!PMONITOR || !PMONITOR->activeWorkspace)
        return {};

    std::vector<std::pair<std::string, std::string>> result;

    result.push_back(std::make_pair<>(
        "HL_INITIAL_WORKSPACE_TOKEN",
        g_pTokenManager->registerNewToken(
            SInitialWorkspaceToken{{}, PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace->getConfigName() : PMONITOR->activeWorkspace->getConfigName()},
            std::chrono::months(1337))));

    return result;
}

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
    m_mDispatchers["fullscreenstate"]                = fullscreenStateActive;
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
    m_mDispatchers["tagwindow"]                      = tagWindow;
    m_mDispatchers["submap"]                         = setSubmap;
    m_mDispatchers["pass"]                           = pass;
    m_mDispatchers["sendshortcut"]                   = sendshortcut;
    m_mDispatchers["layoutmsg"]                      = layoutmsg;
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
    m_mDispatchers["event"]                          = event;
    m_mDispatchers["global"]                         = global;

    m_tScrollTimer.reset();

    static auto P = g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) {
        // clear cuz realloc'd
        m_pActiveKeybind = nullptr;
        m_vPressedSpecialBinds.clear();
    });
}

CKeybindManager::~CKeybindManager() {
    if (m_pXKBTranslationState)
        xkb_state_unref(m_pXKBTranslationState);
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
    if (m_pXKBTranslationState) {
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
    xkb_keymap_unref(PKEYMAP);
}

bool CKeybindManager::ensureMouseBindState() {
    if (!g_pInputManager->currentlyDraggedWindow)
        return false;

    if (!g_pInputManager->currentlyDraggedWindow.expired()) {
        changeMouseBindMode(MBIND_INVALID);
        return true;
    }

    return false;
}

void updateRelativeCursorCoords() {
    static auto PNOWARPS = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    if (*PNOWARPS)
        return;

    if (g_pCompositor->m_pLastWindow)
        g_pCompositor->m_pLastWindow->m_vRelativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - g_pCompositor->m_pLastWindow->m_vPosition;
}

bool CKeybindManager::tryMoveFocusToMonitor(CMonitor* monitor) {
    if (!monitor)
        return false;

    const auto LASTMONITOR = g_pCompositor->m_pLastMonitor.get();
    if (!LASTMONITOR)
        return false;
    if (LASTMONITOR == monitor) {
        Debug::log(LOG, "Tried to move to active monitor");
        return false;
    }

    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    const auto  PWORKSPACE        = g_pCompositor->m_pLastMonitor->activeWorkspace;
    const auto  PNEWMAINWORKSPACE = monitor->activeWorkspace;

    g_pInputManager->unconstrainMouse();
    PNEWMAINWORKSPACE->rememberPrevWorkspace(PWORKSPACE);

    const auto PNEWWORKSPACE = monitor->activeSpecialWorkspace ? monitor->activeSpecialWorkspace : PNEWMAINWORKSPACE;

    const auto PNEWWINDOW = PNEWWORKSPACE->getLastFocusedWindow();
    if (PNEWWINDOW) {
        updateRelativeCursorCoords();
        g_pCompositor->focusWindow(PNEWWINDOW);
        PNEWWINDOW->warpCursor();

        if (*PNOWARPS == 0 || *PFOLLOWMOUSE < 2) {
            g_pInputManager->m_pForcedFocus = PNEWWINDOW;
            g_pInputManager->simulateMouseMovement();
            g_pInputManager->m_pForcedFocus.reset();
        }
    } else {
        g_pCompositor->focusWindow(nullptr);
        g_pCompositor->warpCursorTo(monitor->middle());
    }
    g_pCompositor->setActiveMonitor(monitor);

    return true;
}

void CKeybindManager::switchToWindow(PHLWINDOW PWINDOWTOCHANGETO) {
    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    const auto  PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (PWINDOWTOCHANGETO == PLASTWINDOW || !PWINDOWTOCHANGETO)
        return;

    // remove constraints
    g_pInputManager->unconstrainMouse();

    if (PLASTWINDOW && PLASTWINDOW->m_pWorkspace == PWINDOWTOCHANGETO->m_pWorkspace && PLASTWINDOW->isFullscreen()) {
        const auto PWORKSPACE = PLASTWINDOW->m_pWorkspace;
        const auto MODE       = PWORKSPACE->m_efFullscreenMode;

        if (!PWINDOWTOCHANGETO->m_bPinned)
            g_pCompositor->setWindowFullscreenInternal(PLASTWINDOW, FSMODE_NONE);

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);

        if (!PWINDOWTOCHANGETO->m_bPinned)
            g_pCompositor->setWindowFullscreenInternal(PWINDOWTOCHANGETO, MODE);
    } else {
        updateRelativeCursorCoords();
        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        PWINDOWTOCHANGETO->warpCursor();

        // Move mouse focus to the new window if required by current follow_mouse and warp modes
        if (*PNOWARPS == 0 || *PFOLLOWMOUSE < 2) {
            g_pInputManager->m_pForcedFocus = PWINDOWTOCHANGETO;
            g_pInputManager->simulateMouseMovement();
            g_pInputManager->m_pForcedFocus.reset();
        }

        if (PLASTWINDOW && PLASTWINDOW->m_iMonitorID != PWINDOWTOCHANGETO->m_iMonitorID) {
            // event
            const auto PNEWMON = g_pCompositor->getMonitorFromID(PWINDOWTOCHANGETO->m_iMonitorID);

            g_pCompositor->setActiveMonitor(PNEWMON);
        }
    }
};

bool CKeybindManager::onKeyEvent(std::any event, SP<IKeyboard> pKeyboard) {
    if (!g_pCompositor->m_bSessionActive || g_pCompositor->m_bUnsafeState) {
        m_dPressedKeys.clear();
        return true;
    }

    if (!m_pXKBTranslationState) {
        Debug::log(ERR, "BUG THIS: m_pXKBTranslationState NULL!");
        updateXKBTranslationState();

        if (!m_pXKBTranslationState)
            return true;
    }

    auto               e = std::any_cast<IKeyboard::SKeyEvent>(event);

    const auto         KEYCODE = e.keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t keysym         = xkb_state_key_get_one_sym(pKeyboard->resolveBindsBySym ? pKeyboard->xkbStaticState : m_pXKBTranslationState, KEYCODE);
    const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(pKeyboard->xkbState, KEYCODE);

    if (handleInternalKeybinds(internalKeysym))
        return true;

    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    m_uTimeLastMs    = e.timeMs;
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

    if (m_pActiveKeybindEventSource) {
        wl_event_source_remove(m_pActiveKeybindEventSource);
        m_pActiveKeybindEventSource = nullptr;
        m_pActiveKeybind            = nullptr;
    }

    bool suppressEvent = false;
    if (e.state == WL_KEYBOARD_KEY_STATE_PRESSED) {

        m_dPressedKeys.push_back(KEY);

        suppressEvent = handleKeybinds(MODS, KEY, true);

        if (suppressEvent)
            shadowKeybinds(keysym, KEYCODE);

        m_dPressedKeys.back().sent = !suppressEvent;
    } else { // key release

        bool foundInPressedKeys = false;
        for (auto it = m_dPressedKeys.begin(); it != m_dPressedKeys.end();) {
            if (it->keycode == KEYCODE) {
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

bool CKeybindManager::onAxisEvent(const IPointer::SAxisEvent& e) {
    const auto  MODS = g_pInputManager->accumulateModsFromAllKBs();

    static auto PDELAY = CConfigValue<Hyprlang::INT>("binds:scroll_event_delay");

    if (m_tScrollTimer.getMillis() < *PDELAY) {
        m_tScrollTimer.reset();
        return true; // timer hasn't passed yet!
    }

    m_tScrollTimer.reset();

    if (m_pActiveKeybindEventSource) {
        wl_event_source_remove(m_pActiveKeybindEventSource);
        m_pActiveKeybindEventSource = nullptr;
        m_pActiveKeybind            = nullptr;
    }

    bool found = false;
    if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (e.delta < 0)
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_down"}, true);
        else
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_up"}, true);
    } else if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        if (e.delta < 0)
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_left"}, true);
        else
            found = handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_right"}, true);
    }

    if (found)
        shadowKeybinds();

    return !found;
}

bool CKeybindManager::onMouseEvent(const IPointer::SButtonEvent& e) {
    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    bool       suppressEvent = false;

    m_uLastMouseCode = e.button;
    m_uLastCode      = 0;
    m_uTimeLastMs    = e.timeMs;

    bool       mouseBindWasActive = ensureMouseBindState();

    const auto KEY_NAME = "mouse:" + std::to_string(e.button);

    const auto KEY = SPressedKeyWithMods{
        .keyName            = KEY_NAME,
        .modmaskAtPressTime = MODS,
    };

    if (m_pActiveKeybindEventSource) {
        wl_event_source_remove(m_pActiveKeybindEventSource);
        m_pActiveKeybindEventSource = nullptr;
        m_pActiveKeybind            = nullptr;
    }

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
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

void CKeybindManager::resizeWithBorder(const IPointer::SButtonEvent& e) {
    changeMouseBindMode(e.state == WL_POINTER_BUTTON_STATE_PRESSED ? MBIND_RESIZE : MBIND_INVALID);
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

    if (!*ppActiveKeybind || g_pSeatManager->keyboard.expired())
        return 0;

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find((*ppActiveKeybind)->handler);

    Debug::log(LOG, "Keybind repeat triggered, calling dispatcher.");
    DISPATCHER->second((*ppActiveKeybind)->arg);

    wl_event_source_timer_update(g_pKeybindManager->m_pActiveKeybindEventSource, 1000 / g_pSeatManager->keyboard->repeatRate);

    return 0;
}

eMultiKeyCase CKeybindManager::mkKeysymSetMatches(const std::set<xkb_keysym_t> keybindKeysyms, const std::set<xkb_keysym_t> pressedKeysyms) {
    // Returns whether two sets of keysyms are equal, partially equal, or not
    // matching. (Partially matching means that pressed is a subset of bound)

    std::set<xkb_keysym_t> boundKeysNotPressed;
    std::set<xkb_keysym_t> pressedKeysNotBound;

    std::set_difference(keybindKeysyms.begin(), keybindKeysyms.end(), pressedKeysyms.begin(), pressedKeysyms.end(),
                        std::inserter(boundKeysNotPressed, boundKeysNotPressed.begin()));
    std::set_difference(pressedKeysyms.begin(), pressedKeysyms.end(), keybindKeysyms.begin(), keybindKeysyms.end(),
                        std::inserter(pressedKeysNotBound, pressedKeysNotBound.begin()));

    if (boundKeysNotPressed.empty() && pressedKeysNotBound.empty())
        return MK_FULL_MATCH;

    if (boundKeysNotPressed.size() && pressedKeysNotBound.empty())
        return MK_PARTIAL_MATCH;

    return MK_NO_MATCH;
}

eMultiKeyCase CKeybindManager::mkBindMatches(const SKeybind keybind) {
    if (mkKeysymSetMatches(keybind.sMkMods, m_sMkMods) != MK_FULL_MATCH)
        return MK_NO_MATCH;

    return mkKeysymSetMatches(keybind.sMkKeys, m_sMkKeys);
}

bool CKeybindManager::handleKeybinds(const uint32_t modmask, const SPressedKeyWithMods& key, bool pressed) {
    bool found = false;

    if (pressed) {
        if (keycodeToModifier(key.keycode))
            m_sMkMods.insert(key.keysym);
        else
            m_sMkKeys.insert(key.keysym);
    } else {
        if (keycodeToModifier(key.keycode))
            m_sMkMods.erase(key.keysym);
        else
            m_sMkKeys.erase(key.keysym);
    }

    static auto PDISABLEINHIBIT = CConfigValue<Hyprlang::INT>("binds:disable_keybind_grabbing");

    if (!*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited())
        Debug::log(LOG, "Keybind handling is disabled due to an inhibitor");

    for (auto& k : m_lKeybinds) {
        const bool SPECIALDISPATCHER = k.handler == "global" || k.handler == "pass" || k.handler == "sendshortcut" || k.handler == "mouse";
        const bool SPECIALTRIGGERED =
            std::find_if(m_vPressedSpecialBinds.begin(), m_vPressedSpecialBinds.end(), [&](const auto& other) { return other == &k; }) != m_vPressedSpecialBinds.end();
        const bool IGNORECONDITIONS =
            SPECIALDISPATCHER && !pressed && SPECIALTRIGGERED; // ignore mods. Pass, global dispatchers should be released immediately once the key is released.

        if (!k.dontInhibit && !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited())
            continue;

        if (!k.locked && g_pSessionLockManager->isSessionLocked())
            continue;

        if (!IGNORECONDITIONS && ((modmask != k.modmask && !k.ignoreMods) || k.submap != m_szCurrentSelectedSubmap || k.shadowed))
            continue;

        if (k.multiKey) {
            switch (mkBindMatches(k)) {
                case MK_NO_MATCH: continue;
                case MK_PARTIAL_MATCH: found = true; continue;
                case MK_FULL_MATCH: found = true;
            }
        } else if (!key.keyName.empty()) {
            if (key.keyName != k.key)
                continue;
        } else if (k.keycode != 0) {
            if (key.keycode != k.keycode)
                continue;
        } else if (k.catchAll) {
            if (found || key.submapAtPress != m_szCurrentSelectedSubmap)
                continue;
        } else {
            // in this case, we only have the keysym to go off of for this keybind, and it's invalid
            // since there might be something like keycode to match with other keybinds, try the next
            if (key.keysym == XKB_KEY_NoSymbol)
                continue;

            // oMg such performance hit!!11!
            // this little maneouver is gonna cost us 4µs
            const auto KBKEY      = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_NO_FLAGS);
            const auto KBKEYLOWER = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

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

            const auto PACTIVEKEEB = g_pSeatManager->keyboard.lock();

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

        if (k.multiKey && (mkBindMatches(k) == MK_FULL_MATCH))
            shadow = true;
        else {
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

    if (g_pCompositor->m_pAqBackend->hasSession()) {
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

        g_pCompositor->m_pAqBackend->session->switchVT(TTY);
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

    args = trim(args);

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

    const auto HLENV = getHyprlandLaunchEnv();

    int        socket[2];
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
        g_pCompositor->restoreNofile();

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

        grandchild = fork();
        if (grandchild == 0) {
            // run in grandchild
            for (auto& e : HLENV) {
                setenv(e.first.c_str(), e.second.c_str(), 1);
            }
            setenv("WAYLAND_DISPLAY", g_pCompositor->m_szWLDisplaySocket.c_str(), 1);
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
    g_pCompositor->closeWindow(g_pCompositor->m_pLastWindow.lock());
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

static void toggleActiveFloatingCore(std::string args, std::optional<bool> floatState) {
    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    if (floatState.has_value() && floatState == PWINDOW->m_bIsFloating)
        return;

    // remove drag status
    if (!g_pInputManager->currentlyDraggedWindow.expired())
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);

    if (PWINDOW->m_sGroupData.pNextWindow.lock() && PWINDOW->m_sGroupData.pNextWindow.lock() != PWINDOW) {
        const auto PCURRENT = PWINDOW->getGroupCurrent();

        PCURRENT->m_bIsFloating = !PCURRENT->m_bIsFloating;
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PCURRENT);

        PHLWINDOW curr = PCURRENT->m_sGroupData.pNextWindow.lock();
        while (curr != PCURRENT) {
            curr->m_bIsFloating = PCURRENT->m_bIsFloating;
            curr                = curr->m_sGroupData.pNextWindow.lock();
        }
    } else {
        PWINDOW->m_bIsFloating = !PWINDOW->m_bIsFloating;

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PWINDOW);
    }
    g_pCompositor->updateWorkspaceWindows(PWINDOW->workspaceID());
    g_pCompositor->updateWorkspaceWindowData(PWINDOW->workspaceID());
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->m_iMonitorID);
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();
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

void CKeybindManager::centerWindow(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW || !PWINDOW->m_bIsFloating || PWINDOW->isFullscreen())
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    auto       RESERVEDOFFSET = Vector2D();
    if (args == "1")
        RESERVEDOFFSET = (PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight) / 2.f;

    PWINDOW->m_vRealPosition = PMONITOR->middle() - PWINDOW->m_vRealSize.goal() / 2.f + RESERVEDOFFSET;
    PWINDOW->m_vPosition     = PWINDOW->m_vRealPosition.goal();
}

void CKeybindManager::toggleActivePseudo(std::string args) {
    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    PWINDOW->m_bIsPseudotiled = !PWINDOW->m_bIsPseudotiled;

    if (!PWINDOW->isFullscreen())
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);
}

SWorkspaceIDName getWorkspaceToChangeFromArgs(std::string args, PHLWORKSPACE PCURRENTWORKSPACE) {
    if (!args.starts_with("previous")) {
        return getWorkspaceIDNameFromString(args);
    }

    const bool             PER_MON = args.contains("_per_monitor");
    const SWorkspaceIDName PPREVWS = PCURRENTWORKSPACE->getPrevWorkspaceIDName(PER_MON);
    // Do nothing if there's no previous workspace, otherwise switch to it.
    if (PPREVWS.id == -1) {
        Debug::log(LOG, "No previous workspace to change to");
        return {WORKSPACE_NOT_CHANGED, ""};
    }

    const auto ID = PCURRENTWORKSPACE->m_iID;
    if (const auto PWORKSPACETOCHANGETO = g_pCompositor->getWorkspaceByID(PPREVWS.id); PWORKSPACETOCHANGETO) {
        if (PER_MON && PCURRENTWORKSPACE->m_iMonitorID != PWORKSPACETOCHANGETO->m_iMonitorID)
            return {WORKSPACE_NOT_CHANGED, ""};
        return {ID, PWORKSPACETOCHANGETO->m_szName};
    }

    return {ID, PPREVWS.name.empty() ? std::to_string(PPREVWS.id) : PPREVWS.name};
}

void CKeybindManager::changeworkspace(std::string args) {
    // Workspace_back_and_forth being enabled means that an attempt to switch to
    // the current workspace will instead switch to the previous.
    static auto PBACKANDFORTH         = CConfigValue<Hyprlang::INT>("binds:workspace_back_and_forth");
    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");
    static auto PWORKSPACECENTERON    = CConfigValue<Hyprlang::INT>("binds:workspace_center_on");

    const auto  PMONITOR = g_pCompositor->m_pLastMonitor.get();

    if (!PMONITOR)
        return;

    const auto PCURRENTWORKSPACE = PMONITOR->activeWorkspace;
    const bool EXPLICITPREVIOUS  = args.contains("previous");

    const auto& [workspaceToChangeTo, workspaceName] = getWorkspaceToChangeFromArgs(args, PCURRENTWORKSPACE);
    if (workspaceToChangeTo == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return;
    }

    if (workspaceToChangeTo == WORKSPACE_NOT_CHANGED) {
        return;
    }

    const auto PREVWS = PCURRENTWORKSPACE->getPrevWorkspaceIDName(args.contains("_per_monitor"));

    const bool BISWORKSPACECURRENT = workspaceToChangeTo == PCURRENTWORKSPACE->m_iID;
    if (BISWORKSPACECURRENT && (!(*PBACKANDFORTH || EXPLICITPREVIOUS) || PREVWS.id == -1))
        return;

    g_pInputManager->unconstrainMouse();
    g_pInputManager->m_bEmptyFocusCursorSet = false;

    auto pWorkspaceToChangeTo = g_pCompositor->getWorkspaceByID(BISWORKSPACECURRENT ? PREVWS.id : workspaceToChangeTo);
    if (!pWorkspaceToChangeTo)
        pWorkspaceToChangeTo =
            g_pCompositor->createNewWorkspace(BISWORKSPACECURRENT ? PREVWS.id : workspaceToChangeTo, PMONITOR->ID, BISWORKSPACECURRENT ? PREVWS.name : workspaceName);

    if (!BISWORKSPACECURRENT && pWorkspaceToChangeTo->m_bIsSpecialWorkspace) {
        PMONITOR->setSpecialWorkspace(pWorkspaceToChangeTo);
        g_pInputManager->simulateMouseMovement();
        return;
    }

    g_pInputManager->releaseAllMouseButtons();

    const auto PMONITORWORKSPACEOWNER = PMONITOR->ID == pWorkspaceToChangeTo->m_iMonitorID ? PMONITOR : g_pCompositor->getMonitorFromID(pWorkspaceToChangeTo->m_iMonitorID);

    if (!PMONITORWORKSPACEOWNER)
        return;

    updateRelativeCursorCoords();

    g_pCompositor->setActiveMonitor(PMONITORWORKSPACEOWNER);

    if (BISWORKSPACECURRENT) {
        if (*PALLOWWORKSPACECYCLES)
            pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);
        else if (!EXPLICITPREVIOUS && !*PBACKANDFORTH)
            pWorkspaceToChangeTo->rememberPrevWorkspace(nullptr);
    } else
        pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);

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

    if (!g_pInputManager->m_bLastFocusOnLS) {
        if (g_pCompositor->m_pLastFocus)
            g_pInputManager->sendMotionEventsToFocused();
        else
            g_pInputManager->simulateMouseMovement();
    }

    const static auto PWARPONWORKSPACECHANGE = CConfigValue<Hyprlang::INT>("cursor:warp_on_change_workspace");

    if (*PWARPONWORKSPACECHANGE) {
        auto PLAST     = pWorkspaceToChangeTo->getLastFocusedWindow();
        auto HLSurface = CWLSurface::fromResource(g_pSeatManager->state.pointerFocus.lock());

        if (PLAST && (!HLSurface || HLSurface->getWindow()))
            PLAST->warpCursor();
    }
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    const eFullscreenMode MODE = args == "1" ? FSMODE_MAXIMIZED : FSMODE_FULLSCREEN;

    if (PWINDOW->isEffectiveInternalFSMode(MODE))
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
    else
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, MODE);
}

void CKeybindManager::fullscreenStateActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();
    const auto ARGS    = CVarList(args, 2, ' ');

    if (!PWINDOW)
        return;

    PWINDOW->m_sWindowData.syncFullscreen = CWindowOverridableVar(false, PRIORITY_SET_PROP);

    int internalMode, clientMode;
    try {
        internalMode = std::stoi(ARGS[0]);
    } catch (std::exception& e) { internalMode = -1; }
    try {
        clientMode = std::stoi(ARGS[1]);
    } catch (std::exception& e) { clientMode = -1; }

    const sFullscreenState STATE = sFullscreenState{.internal = (internalMode != -1 ? (eFullscreenMode)internalMode : PWINDOW->m_sFullscreenState.internal),
                                                    .client   = (clientMode != -1 ? (eFullscreenMode)clientMode : PWINDOW->m_sFullscreenState.client)};

    if (internalMode != -1 && clientMode != -1 && PWINDOW->m_sFullscreenState.internal == STATE.internal && PWINDOW->m_sFullscreenState.client == STATE.client)
        g_pCompositor->setWindowFullscreenState(PWINDOW, sFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
    else if (internalMode != -1 && clientMode == -1 && PWINDOW->m_sFullscreenState.internal == STATE.internal)
        g_pCompositor->setWindowFullscreenState(PWINDOW, sFullscreenState{.internal = FSMODE_NONE, .client = PWINDOW->m_sFullscreenState.client});
    else if (internalMode == -1 && clientMode != -1 && PWINDOW->m_sFullscreenState.client == STATE.client)
        g_pCompositor->setWindowFullscreenState(PWINDOW, sFullscreenState{.internal = PWINDOW->m_sFullscreenState.internal, .client = FSMODE_NONE});
    else
        g_pCompositor->setWindowFullscreenState(PWINDOW, STATE);

    PWINDOW->m_sWindowData.syncFullscreen = CWindowOverridableVar(PWINDOW->m_sFullscreenState.internal == PWINDOW->m_sFullscreenState.client, PRIORITY_SET_PROP);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {

    PHLWINDOW PWINDOW = nullptr;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow.lock();
    }

    if (!PWINDOW)
        return;

    const auto& [WORKSPACEID, workspaceName] = getWorkspaceIDNameFromString(args);
    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(LOG, "Invalid workspace in moveActiveToWorkspace");
        return;
    }

    if (WORKSPACEID == PWINDOW->workspaceID()) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    auto        pWorkspace            = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    CMonitor*   pMonitor              = nullptr;
    const auto  POLDWS                = PWINDOW->m_pWorkspace;
    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");

    updateRelativeCursorCoords();

    g_pHyprRenderer->damageWindow(PWINDOW);

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
        pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        g_pCompositor->setActiveMonitor(pMonitor);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->m_iMonitorID, workspaceName, false);
        pMonitor   = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    }

    POLDWS->m_pLastFocusedWindow = g_pCompositor->getFirstWindowOnWorkspace(POLDWS->m_iID);

    if (pWorkspace->m_bIsSpecialWorkspace)
        pMonitor->setSpecialWorkspace(pWorkspace);
    else if (POLDWS->m_bIsSpecialWorkspace)
        g_pCompositor->getMonitorFromID(POLDWS->m_iMonitorID)->setSpecialWorkspace(nullptr);

    if (*PALLOWWORKSPACECYCLES)
        pWorkspace->rememberPrevWorkspace(POLDWS);

    pMonitor->changeWorkspace(pWorkspace);

    g_pCompositor->focusWindow(PWINDOW);
    PWINDOW->warpCursor();
}

void CKeybindManager::moveActiveToWorkspaceSilent(std::string args) {
    PHLWINDOW  PWINDOW = nullptr;

    const auto ORIGINALARGS = args;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow.lock();
    }

    if (!PWINDOW)
        return;

    const auto& [WORKSPACEID, workspaceName] = getWorkspaceIDNameFromString(args);
    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in moveActiveToWorkspaceSilent, invalid value");
        return;
    }

    if (WORKSPACEID == PWINDOW->workspaceID())
        return;

    g_pHyprRenderer->damageWindow(PWINDOW);

    auto       pWorkspace = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    const auto OLDMIDDLE  = PWINDOW->middle();

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->m_iMonitorID, workspaceName, false);
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
    static auto PFULLCYCLE       = CConfigValue<Hyprlang::INT>("binds:movefocus_cycles_fullscreen");
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");
    char        arg              = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move focus in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
    if (!PLASTWINDOW) {
        if (*PMONITORFALLBACK)
            tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg));

        return;
    }

    const auto PWINDOWTOCHANGETO = *PFULLCYCLE && PLASTWINDOW->isFullscreen() ?
        (arg == 'd' || arg == 'b' || arg == 'r' ? g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW, true) : g_pCompositor->getPrevWindowOnWorkspace(PLASTWINDOW, true)) :
        g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);

    // Found window in direction, switch to it
    if (PWINDOWTOCHANGETO) {
        switchToWindow(PWINDOWTOCHANGETO);
        return;
    }

    Debug::log(LOG, "No window found in direction {}, looking for a monitor", arg);

    if (*PMONITORFALLBACK && tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg)))
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
    const auto PWINDOWPREV   = g_pCompositor->m_pLastWindow.lock() ? (g_pCompositor->m_vWindowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_vWindowFocusHistory[1].lock()) :
                                                                     (g_pCompositor->m_vWindowFocusHistory.empty() ? nullptr : g_pCompositor->m_vWindowFocusHistory[0].lock());

    if (!PWINDOWURGENT && !PWINDOWPREV)
        return;

    switchToWindow(PWINDOWURGENT ? PWINDOWURGENT : PWINDOWPREV);
}

void CKeybindManager::focusCurrentOrLast(std::string args) {
    const auto PWINDOWPREV = g_pCompositor->m_pLastWindow.lock() ? (g_pCompositor->m_vWindowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_vWindowFocusHistory[1].lock()) :
                                                                   (g_pCompositor->m_vWindowFocusHistory.empty() ? nullptr : g_pCompositor->m_vWindowFocusHistory[0].lock());

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
    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
    if (!PLASTWINDOW || PLASTWINDOW->isFullscreen())
        return;

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (!PWINDOWTOCHANGETO)
        return;

    updateRelativeCursorCoords();
    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, PWINDOWTOCHANGETO);
    PLASTWINDOW->warpCursor();
}

void CKeybindManager::moveActiveTo(std::string args) {
    char arg    = args[0];
    bool silent = args.ends_with(" silent");
    if (silent)
        args = args.substr(0, args.length() - 7);

    if (args.starts_with("mon:")) {
        const auto PNEWMONITOR = g_pCompositor->getMonitorFromString(args.substr(4));
        if (!PNEWMONITOR)
            return;

        if (silent)
            moveActiveToWorkspaceSilent(PNEWMONITOR->activeWorkspace->getConfigName());
        else
            moveActiveToWorkspace(PNEWMONITOR->activeWorkspace->getConfigName());

        return;
    }

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PLASTWINDOW || PLASTWINDOW->isFullscreen())
        return;

    if (PLASTWINDOW->m_bIsFloating) {
        std::optional<float> vPosx, vPosy;
        const auto           PMONITOR   = g_pCompositor->getMonitorFromID(PLASTWINDOW->m_iMonitorID);
        const auto           BORDERSIZE = PLASTWINDOW->getRealBorderSize();

        switch (arg) {
            case 'l': vPosx = PMONITOR->vecReservedTopLeft.x + BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 'r': vPosx = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PLASTWINDOW->m_vRealSize.goal().x - BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 't':
            case 'u': vPosy = PMONITOR->vecReservedTopLeft.y + BORDERSIZE + PMONITOR->vecPosition.y; break;
            case 'b':
            case 'd': vPosy = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PLASTWINDOW->m_vRealSize.goal().y - BORDERSIZE + PMONITOR->vecPosition.y; break;
        }

        PLASTWINDOW->m_vRealPosition = Vector2D(vPosx.value_or(PLASTWINDOW->m_vRealPosition.goal().x), vPosy.value_or(PLASTWINDOW->m_vRealPosition.goal().y));

        return;
    }

    // If the window to change to is on the same workspace, switch them
    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (PWINDOWTOCHANGETO) {
        updateRelativeCursorCoords();

        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PLASTWINDOW, args, silent);
        if (!silent)
            PLASTWINDOW->warpCursor();
        return;
    }

    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");
    if (!*PMONITORFALLBACK)
        return;

    // Otherwise, we always want to move to the next monitor in that direction
    const auto PMONITORTOCHANGETO = g_pCompositor->getMonitorInDirection(arg);
    if (!PMONITORTOCHANGETO)
        return;

    const auto PWORKSPACE = PMONITORTOCHANGETO->activeWorkspace;
    if (silent)
        moveActiveToWorkspaceSilent(PWORKSPACE->getConfigName());
    else
        moveActiveToWorkspace(PWORKSPACE->getConfigName());
}

void CKeybindManager::toggleGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    if (PWINDOW->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    if (PWINDOW->m_sGroupData.pNextWindow.expired())
        PWINDOW->createGroup();
    else
        PWINDOW->destroyGroup();
}

void CKeybindManager::changeGroupActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    if (PWINDOW->m_sGroupData.pNextWindow.expired())
        return;

    if (PWINDOW->m_sGroupData.pNextWindow.lock() == PWINDOW)
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
        PWINDOW->setGroupCurrent(PWINDOW->m_sGroupData.pNextWindow.lock());
    } else {
        PWINDOW->setGroupCurrent(PWINDOW->getGroupPrevious());
    }
}

void CKeybindManager::toggleSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow.lock();

    if (!header.pWindow)
        return;

    const auto PWORKSPACE = header.pWindow->m_pWorkspace;

    if (PWORKSPACE->m_bHasFullscreenWindow)
        return;

    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "togglesplit");
}

void CKeybindManager::swapSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow.lock();

    if (!header.pWindow)
        return;

    const auto PWORKSPACE = header.pWindow->m_pWorkspace;

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

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

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

    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW)
        return;

    switch (CORNER) {
        case 0:
            // bottom left
            g_pCompositor->warpCursorTo({PWINDOW->m_vRealPosition.value().x, PWINDOW->m_vRealPosition.value().y + PWINDOW->m_vRealSize.value().y}, true);
            break;
        case 1:
            // bottom right
            g_pCompositor->warpCursorTo({PWINDOW->m_vRealPosition.value().x + PWINDOW->m_vRealSize.value().x, PWINDOW->m_vRealPosition.value().y + PWINDOW->m_vRealSize.value().y},
                                        true);
            break;
        case 2:
            // top right
            g_pCompositor->warpCursorTo({PWINDOW->m_vRealPosition.value().x + PWINDOW->m_vRealSize.value().x, PWINDOW->m_vRealPosition.value().y}, true);
            break;
        case 3:
            // top left
            g_pCompositor->warpCursorTo({PWINDOW->m_vRealPosition.value().x, PWINDOW->m_vRealPosition.value().y}, true);
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

    g_pCompositor->warpCursorTo({x, y}, true);
}

void CKeybindManager::workspaceOpt(std::string args) {

    // current workspace
    const auto PWORKSPACE = g_pCompositor->m_pLastMonitor->activeWorkspace;

    if (!PWORKSPACE)
        return; // ????

    if (args == "allpseudo") {
        PWORKSPACE->m_bDefaultPseudo = !PWORKSPACE->m_bDefaultPseudo;

        // apply
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->m_pWorkspace != PWORKSPACE)
                continue;

            w->m_bIsPseudotiled = PWORKSPACE->m_bDefaultPseudo;
        }
    } else if (args == "allfloat") {
        PWORKSPACE->m_bDefaultFloating = !PWORKSPACE->m_bDefaultFloating;
        // apply

        // we make a copy because changeWindowFloatingMode might invalidate the iterator
        std::deque<PHLWINDOW> ptrs;
        for (auto& w : g_pCompositor->m_vWindows)
            ptrs.push_back(w);

        for (auto& w : ptrs) {
            if (!w->m_bIsMapped || w->m_pWorkspace != PWORKSPACE || w->isHidden())
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
    g_pCompositor->stopCompositor();
}

void CKeybindManager::moveCurrentWorkspaceToMonitor(std::string args) {
    CMonitor* PMONITOR = g_pCompositor->getMonitorFromString(args);

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveCurrentWorkspaceToMonitor: monitor doesnt exist");
        return;
    }

    // get the current workspace
    const auto PCURRENTWORKSPACE = g_pCompositor->m_pLastMonitor->activeWorkspace;
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

    const auto WORKSPACEID = getWorkspaceIDNameFromString(workspace).id;

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
    auto workspaceID = getWorkspaceIDNameFromString(args).id;
    if (workspaceID == WORKSPACE_INVALID) {
        Debug::log(ERR, "focusWorkspaceOnCurrentMonitor invalid workspace!");
        return;
    }

    const auto PCURRMONITOR = g_pCompositor->m_pLastMonitor.get();

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
    const auto  PREVWS        = pWorkspace->getPrevWorkspaceIDName(false);

    if (*PBACKANDFORTH && PCURRMONITOR->activeWorkspaceID() == workspaceID && PREVWS.id != -1) {
        // Workspace to focus is previous workspace
        pWorkspace = g_pCompositor->getWorkspaceByID(PREVWS.id);
        if (!pWorkspace)
            pWorkspace = g_pCompositor->createNewWorkspace(PREVWS.id, PCURRMONITOR->ID, PREVWS.name);

        workspaceID = pWorkspace->m_iID;
    }

    if (pWorkspace->m_iMonitorID != PCURRMONITOR->ID) {
        const auto POLDMONITOR = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        if (!POLDMONITOR) { // wat
            Debug::log(ERR, "focusWorkspaceOnCurrentMonitor old monitor doesn't exist!");
            return;
        }
        if (POLDMONITOR->activeWorkspaceID() == workspaceID) {
            g_pCompositor->swapActiveWorkspaces(POLDMONITOR, PCURRMONITOR);
            return;
        } else {
            g_pCompositor->moveWorkspaceToMonitor(pWorkspace, PCURRMONITOR, true);
        }
    }

    changeworkspace(pWorkspace->getConfigName());
}

void CKeybindManager::toggleSpecialWorkspace(std::string args) {
    const auto& [workspaceID, workspaceName] = getWorkspaceIDNameFromString("special:" + args);
    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID)) {
        Debug::log(ERR, "Invalid workspace passed to special");
        return;
    }

    bool       requestedWorkspaceIsAlreadyOpen = false;
    const auto PMONITOR                        = g_pCompositor->m_pLastMonitor;
    auto       specialOpenOnMonitor            = PMONITOR->activeSpecialWorkspaceID();

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->activeSpecialWorkspaceID() == workspaceID) {
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
    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PLASTWINDOW || PLASTWINDOW->isFullscreen())
        return;

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(args, PLASTWINDOW->m_vRealSize.goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return;

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PLASTWINDOW->m_vRealSize.goal());

    if (PLASTWINDOW->m_vRealSize.goal().x > 1 && PLASTWINDOW->m_vRealSize.goal().y > 1)
        PLASTWINDOW->setHidden(false);
}

void CKeybindManager::moveActive(std::string args) {
    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PLASTWINDOW || PLASTWINDOW->isFullscreen())
        return;

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(args, PLASTWINDOW->m_vRealPosition.goal());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - PLASTWINDOW->m_vRealPosition.goal());
}

void CKeybindManager::moveWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "moveWindow: no window");
        return;
    }

    if (PWINDOW->isFullscreen())
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

    if (PWINDOW->isFullscreen())
        return;

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_vRealSize.goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return;

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PWINDOW->m_vRealSize.goal(), CORNER_NONE, PWINDOW);

    if (PWINDOW->m_vRealSize.goal().x > 1 && PWINDOW->m_vRealSize.goal().y > 1)
        PWINDOW->setHidden(false);
}

void CKeybindManager::circleNext(std::string arg) {

    if (g_pCompositor->m_pLastWindow.expired()) {
        // if we have a clear focus, find the first window and get the next focusable.
        if (g_pCompositor->getWindowsOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspaceID()) > 0) {
            const auto PWINDOW = g_pCompositor->getFirstWindowOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspaceID());

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
        switchToWindow(g_pCompositor->getPrevWindowOnWorkspace(g_pCompositor->m_pLastWindow.lock(), true, floatStatus));
    else
        switchToWindow(g_pCompositor->getNextWindowOnWorkspace(g_pCompositor->m_pLastWindow.lock(), true, floatStatus));
}

void CKeybindManager::focusWindow(std::string regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW)
        return;

    Debug::log(LOG, "Focusing to window name: {}", PWINDOW->m_szTitle);

    const auto PWORKSPACE = PWINDOW->m_pWorkspace;
    if (!PWORKSPACE) {
        Debug::log(ERR, "BUG THIS: null workspace in focusWindow");
        return;
    }

    updateRelativeCursorCoords();

    if (g_pCompositor->m_pLastMonitor && g_pCompositor->m_pLastMonitor->activeWorkspace != PWINDOW->m_pWorkspace &&
        g_pCompositor->m_pLastMonitor->activeSpecialWorkspace != PWINDOW->m_pWorkspace) {
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
                g_pCompositor->setWindowFullscreenClient(FSWINDOW, FSMODE_NONE);

            g_pCompositor->focusWindow(PWINDOW);

            if (FSWINDOW != PWINDOW && !PWINDOW->m_bPinned)
                g_pCompositor->setWindowFullscreenClient(PWINDOW, FSMODE);
        }
    } else
        g_pCompositor->focusWindow(PWINDOW);

    PWINDOW->warpCursor();
}

void CKeybindManager::tagWindow(std::string args) {
    PHLWINDOW PWINDOW = nullptr;
    CVarList  vars{args, 0, 's', true};

    if (vars.size() == 1)
        PWINDOW = g_pCompositor->m_pLastWindow.lock();
    else if (vars.size() == 2)
        PWINDOW = g_pCompositor->getWindowByRegex(vars[1]);
    else
        return;

    if (PWINDOW && PWINDOW->m_tags.applyTag(vars[0])) {
        PWINDOW->updateDynamicRules();
        g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW->m_pSelf.lock());
    }
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

    if (!g_pSeatManager->keyboard) {
        Debug::log(ERR, "No kb in pass?");
        return;
    }

    const auto XWTOXW        = PWINDOW->m_bIsX11 && g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bIsX11;
    const auto LASTMOUSESURF = g_pSeatManager->state.pointerFocus.lock();
    const auto LASTKBSURF    = g_pSeatManager->state.keyboardFocus.lock();

    // pass all mf shit
    if (!XWTOXW) {
        if (g_pKeybindManager->m_uLastCode != 0)
            g_pSeatManager->setKeyboardFocus(PWINDOW->m_pWLSurface->resource());
        else
            g_pSeatManager->setPointerFocus(PWINDOW->m_pWLSurface->resource(), {1, 1});
    }

    g_pSeatManager->sendKeyboardMods(g_pInputManager->accumulateModsFromAllKBs(), 0, 0, 0);

    if (g_pKeybindManager->m_iPassPressed == 1) {
        if (g_pKeybindManager->m_uLastCode != 0)
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
        else
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
    } else if (g_pKeybindManager->m_iPassPressed == 0)
        if (g_pKeybindManager->m_uLastCode != 0)
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        else
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
    else {
        // dynamic call of the dispatcher
        if (g_pKeybindManager->m_uLastCode != 0) {
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        } else {
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    if (XWTOXW)
        return;

    // Massive hack:
    // this will make g_pSeatManager NOT send the leave event to XWayland apps, provided we are not on an XWayland window already.
    // please kill me
    if (PWINDOW->m_bIsX11) {
        if (g_pKeybindManager->m_uLastCode != 0) {
            g_pSeatManager->state.keyboardFocus.reset();
            g_pSeatManager->state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->state.pointerFocus.reset();
            g_pSeatManager->state.pointerFocusResource.reset();
        }
    }

    const auto SL = PWINDOW->m_vRealPosition.goal() - g_pInputManager->getMouseCoordsInternal();

    if (g_pKeybindManager->m_uLastCode != 0)
        g_pSeatManager->setKeyboardFocus(LASTKBSURF);
    else
        g_pSeatManager->setPointerFocus(LASTMOUSESURF, SL);
}

void CKeybindManager::sendshortcut(std::string args) {
    // args=<NEW_MODKEYS><NEW_KEY>[,WINDOW_RULES]
    const auto ARGS = CVarList(args, 3);
    if (ARGS.size() != 3) {
        Debug::log(ERR, "sendshortcut: invalid args");
        return;
    }

    const auto MOD     = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto KEY     = ARGS[1];
    uint32_t   keycode = 0;
    bool       isMouse = 0;

    // similar to parseKey in ConfigManager
    if (isNumber(KEY) && std::stoi(KEY) > 9)
        keycode = std::stoi(KEY);
    else if (KEY.compare(0, 5, "code:") == 0 && isNumber(KEY.substr(5)))
        keycode = std::stoi(KEY.substr(5));
    else if (KEY.compare(0, 6, "mouse:") == 0 && isNumber(KEY.substr(6))) {
        keycode = std::stoi(KEY.substr(6));
        isMouse = 1;
        if (keycode < 272) {
            Debug::log(ERR, "sendshortcut: invalid mouse button");
            return;
        }
    } else {

        // here, we need to find the keycode from the key name
        // this is not possible through xkb's lib, so we need to iterate through all keycodes
        // once found, we save it to the cache

        const auto KEYSYM = xkb_keysym_from_name(KEY.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        keycode           = 0;

        const auto KB = g_pSeatManager->keyboard;

        if (!KB) {
            Debug::log(ERR, "sendshortcut: no kb");
            return;
        }

        const auto KEYPAIRSTRING = std::format("{}{}", (uintptr_t)KB.get(), KEY);

        if (!g_pKeybindManager->m_mKeyToCodeCache.contains(KEYPAIRSTRING)) {
            xkb_keymap*   km = KB->xkbKeymap;
            xkb_state*    ks = KB->xkbState;

            xkb_keycode_t keycode_min, keycode_max;
            keycode_min = xkb_keymap_min_keycode(km);
            keycode_max = xkb_keymap_max_keycode(km);

            for (xkb_keycode_t kc = keycode_min; kc <= keycode_max; ++kc) {
                xkb_keysym_t sym = xkb_state_key_get_one_sym(ks, kc);

                if (sym == KEYSYM) {
                    keycode                                             = kc;
                    g_pKeybindManager->m_mKeyToCodeCache[KEYPAIRSTRING] = keycode;
                }
            }

            if (!keycode) {
                Debug::log(ERR, "sendshortcut: key not found");
                return;
            }

        } else
            keycode = g_pKeybindManager->m_mKeyToCodeCache[KEYPAIRSTRING];
    }

    if (!keycode) {
        Debug::log(ERR, "sendshortcut: invalid key");
        return;
    }

    const std::string regexp      = ARGS[2];
    PHLWINDOW         PWINDOW     = nullptr;
    const auto        LASTSURFACE = g_pCompositor->m_pLastFocus.lock();

    //if regexp is not empty, send shortcut to current window
    //else, dont change focus
    if (regexp != "") {
        PWINDOW = g_pCompositor->getWindowByRegex(regexp);

        if (!PWINDOW) {
            Debug::log(ERR, "sendshortcut: window not found");
            return;
        }

        if (!g_pSeatManager->keyboard) {
            Debug::log(ERR, "No kb in sendshortcut?");
            return;
        }

        if (!isMouse)
            g_pSeatManager->setKeyboardFocus(PWINDOW->m_pWLSurface->resource());
        else
            g_pSeatManager->setPointerFocus(PWINDOW->m_pWLSurface->resource(), {1, 1});
    }

    //copied the rest from pass and modified it
    // if wl -> xwl, activate destination
    if (PWINDOW && PWINDOW->m_bIsX11 && g_pCompositor->m_pLastWindow && !g_pCompositor->m_pLastWindow->m_bIsX11)
        g_pXWaylandManager->activateSurface(PWINDOW->m_pWLSurface->resource(), true);
    // if xwl -> xwl, send to current. Timing issues make this not work.
    if (PWINDOW && PWINDOW->m_bIsX11 && g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsX11)
        PWINDOW = nullptr;

    g_pSeatManager->sendKeyboardMods(MOD, 0, 0, 0);

    if (g_pKeybindManager->m_iPassPressed == 1) {
        if (!isMouse)
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, keycode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
        else
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, keycode, WL_POINTER_BUTTON_STATE_PRESSED);
    } else if (g_pKeybindManager->m_iPassPressed == 0) {
        if (!isMouse)
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, keycode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        else
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, keycode, WL_POINTER_BUTTON_STATE_RELEASED);
    } else {
        // dynamic call of the dispatcher
        if (!isMouse) {
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, keycode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
            g_pSeatManager->sendKeyboardKey(g_pKeybindManager->m_uTimeLastMs, keycode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        } else {
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, keycode, WL_POINTER_BUTTON_STATE_PRESSED);
            g_pSeatManager->sendPointerButton(g_pKeybindManager->m_uTimeLastMs, keycode, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    if (!PWINDOW)
        return;

    if (PWINDOW->m_bIsX11) { //xwayland hack, see pass
        if (!isMouse) {
            g_pSeatManager->state.keyboardFocus.reset();
            g_pSeatManager->state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->state.pointerFocus.reset();
            g_pSeatManager->state.pointerFocusResource.reset();
        }
    }

    const auto SL = PWINDOW->m_vRealPosition.goal() - g_pInputManager->getMouseCoordsInternal();

    if (!isMouse)
        g_pSeatManager->setKeyboardFocus(LASTSURFACE);
    else
        g_pSeatManager->setPointerFocus(LASTSURFACE, SL);
}

void CKeybindManager::layoutmsg(std::string msg) {
    SLayoutMessageHeader hd = {g_pCompositor->m_pLastWindow.lock()};
    g_pLayoutManager->getCurrentLayout()->layoutMessage(hd, msg);
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

        m->output->state->resetExplicitFences();
        m->output->state->setEnabled(enable);

        m->dpmsStatus = enable;

        if (!m->state.commit()) {
            Debug::log(ERR, "Couldn't commit output {}", m->szName);
        }

        if (enable)
            g_pHyprRenderer->damageMonitor(m.get());

        m->events.dpmsChanged.emit();
    }

    g_pCompositor->m_bDPMSStateON = enable;

    g_pPointerManager->recheckEnteredOutputs();
}

void CKeybindManager::swapnext(std::string arg) {

    PHLWINDOW toSwap = nullptr;

    if (g_pCompositor->m_pLastWindow.expired())
        return;

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    const auto PLASTCYCLED =
        validMapped(g_pCompositor->m_pLastWindow->m_pLastCycledWindow) && g_pCompositor->m_pLastWindow->m_pLastCycledWindow->m_pWorkspace == PLASTWINDOW->m_pWorkspace ?
        g_pCompositor->m_pLastWindow->m_pLastCycledWindow.lock() :
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

    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW) {
        Debug::log(ERR, "pin: window not found");
        return;
    }

    if (!PWINDOW->m_bIsFloating || PWINDOW->isFullscreen())
        return;

    PWINDOW->m_bPinned = !PWINDOW->m_bPinned;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    if (!PMONITOR) {
        Debug::log(ERR, "pin: monitor not found");
        return;
    }

    PWINDOW->m_pWorkspace = PMONITOR->activeWorkspace;

    PWINDOW->updateDynamicRules();
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);

    const auto PWORKSPACE = PWINDOW->m_pWorkspace;

    PWORKSPACE->m_pLastFocusedWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS);

    g_pEventManager->postEvent(SHyprIPCEvent{"pin", std::format("{:x},{}", (uintptr_t)PWINDOW.get(), (int)PWINDOW->m_bPinned)});
    EMIT_HOOK_EVENT("pin", PWINDOW);
}

void CKeybindManager::mouse(std::string args) {
    const auto ARGS    = CVarList(args.substr(1), 2, ' ');
    const auto PRESSED = args[0] == '1';

    if (!PRESSED) {
        changeMouseBindMode(MBIND_INVALID);
        return;
    }

    if (ARGS[0] == "movewindow") {
        changeMouseBindMode(MBIND_MOVE);
    } else {
        try {
            switch (std::stoi(ARGS[1])) {
                case 1: changeMouseBindMode(MBIND_RESIZE_FORCE_RATIO); break;
                case 2: changeMouseBindMode(MBIND_RESIZE_BLOCK_RATIO); break;
                default: changeMouseBindMode(MBIND_RESIZE);
            }
        } catch (std::exception& e) { changeMouseBindMode(MBIND_RESIZE); }
    }
}

void CKeybindManager::changeMouseBindMode(const eMouseBindMode MODE) {
    if (MODE != MBIND_INVALID) {
        if (!g_pInputManager->currentlyDraggedWindow.expired() || g_pInputManager->dragMode != MBIND_INVALID)
            return;

        const auto      MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const PHLWINDOW PWINDOW     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

        if (!PWINDOW)
            return;

        if (!PWINDOW->isFullscreen() && MODE == MBIND_MOVE)
            PWINDOW->checkInputOnDecos(INPUT_TYPE_DRAG_START, MOUSECOORDS);

        if (g_pInputManager->currentlyDraggedWindow.expired())
            g_pInputManager->currentlyDraggedWindow = PWINDOW;

        g_pInputManager->dragMode = MODE;

        g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();
    } else {
        if (g_pInputManager->currentlyDraggedWindow.expired() || g_pInputManager->dragMode == MBIND_INVALID)
            return;

        g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
        g_pInputManager->dragMode = MODE;
    }
}

void CKeybindManager::bringActiveToTop(std::string args) {
    if (g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bIsFloating)
        g_pCompositor->changeWindowZOrder(g_pCompositor->m_pLastWindow.lock(), true);
}

void CKeybindManager::alterZOrder(std::string args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto POSITION    = args.substr(0, args.find_first_of(','));
    auto       PWINDOW     = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW && g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bIsFloating)
        PWINDOW = g_pCompositor->m_pLastWindow.lock();

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
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW || !PWINDOW->m_sGroupData.pNextWindow.lock())
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

void CKeybindManager::moveWindowIntoGroup(PHLWINDOW pWindow, PHLWINDOW pWindowInDirection) {
    if (pWindow->m_sGroupData.deny)
        return;

    updateRelativeCursorCoords();

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow); // This removes groupped property!

    if (pWindow->m_iMonitorID != pWindowInDirection->m_iMonitorID) {
        pWindow->moveToWorkspace(pWindowInDirection->m_pWorkspace);
        pWindow->m_iMonitorID = pWindowInDirection->m_iMonitorID;
    }

    static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
    pWindowInDirection     = *USECURRPOS ? pWindowInDirection : pWindowInDirection->getGroupTail();

    pWindowInDirection->insertWindowToGroup(pWindow);
    pWindowInDirection->setGroupCurrent(pWindow);
    pWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pWindow);
    g_pCompositor->focusWindow(pWindow);
    pWindow->warpCursor();

    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));

    g_pEventManager->postEvent(SHyprIPCEvent{"moveintogroup", std::format("{:x}", (uintptr_t)pWindow.get())});
}

void CKeybindManager::moveWindowOutOfGroup(PHLWINDOW pWindow, const std::string& dir) {
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

    updateRelativeCursorCoords();

    if (pWindow->m_sGroupData.pNextWindow.lock() == pWindow) {
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
        pWindow->warpCursor();
    } else {
        g_pCompositor->focusWindow(PWINDOWPREV);
        PWINDOWPREV->warpCursor();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"moveoutofgroup", std::format("{:x}", (uintptr_t)pWindow.get())});
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

    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW || PWINDOW->m_bIsFloating || PWINDOW->m_sGroupData.deny)
        return;

    auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    if (!PWINDOWINDIR || !PWINDOWINDIR->m_sGroupData.pNextWindow.lock())
        return;

    // Do not move window into locked group if binds:ignore_group_lock is false
    if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_sGroupData.locked || (PWINDOW->m_sGroupData.pNextWindow.lock() && PWINDOW->getGroupHead()->m_sGroupData.locked)))
        return;

    moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);
}

void CKeybindManager::moveOutOfGroup(std::string args) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked)
        return;

    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PWINDOW || !PWINDOW->m_sGroupData.pNextWindow.lock())
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

    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();
    if (!PWINDOW || PWINDOW->isFullscreen())
        return;

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked) {
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
        return;
    }

    const auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    const bool ISWINDOWGROUP       = PWINDOW->m_sGroupData.pNextWindow.lock().get();
    const bool ISWINDOWGROUPLOCKED = ISWINDOWGROUP && PWINDOW->getGroupHead()->m_sGroupData.locked;
    const bool ISWINDOWGROUPSINGLE = ISWINDOWGROUP && PWINDOW->m_sGroupData.pNextWindow.lock() == PWINDOW;

    updateRelativeCursorCoords();

    // note: PWINDOWINDIR is not null implies !PWINDOW->m_bIsFloating
    if (PWINDOWINDIR && PWINDOWINDIR->m_sGroupData.pNextWindow) { // target is group
        if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_sGroupData.locked || ISWINDOWGROUPLOCKED || PWINDOW->m_sGroupData.deny)) {
            g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
            PWINDOW->warpCursor();
        } else
            moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);
    } else if (PWINDOWINDIR) { // target is regular window
        if ((!*PIGNOREGROUPLOCK && ISWINDOWGROUPLOCKED) || !ISWINDOWGROUP || (ISWINDOWGROUPSINGLE && PWINDOW->m_eGroupRules & GROUP_SET_ALWAYS)) {
            g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
            PWINDOW->warpCursor();
        } else
            moveWindowOutOfGroup(PWINDOW, args);
    } else if ((*PIGNOREGROUPLOCK || !ISWINDOWGROUPLOCKED) && ISWINDOWGROUP) { // no target window
        moveWindowOutOfGroup(PWINDOW, args);
    } else if (!PWINDOWINDIR && !ISWINDOWGROUP) { // no target in dir and not in group
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
        PWINDOW->warpCursor();
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
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();
    if (!PWINDOW || (PWINDOW && PWINDOW->m_sGroupData.pNextWindow.lock()))
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

    if (!PROTO::globalShortcuts->isTaken(APPID, NAME))
        return;

    PROTO::globalShortcuts->sendGlobalShortcutEvent(APPID, NAME, g_pKeybindManager->m_iPassPressed);
}

void CKeybindManager::moveGroupWindow(std::string args) {
    const auto BACK = args == "b" || args == "prev";

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!PLASTWINDOW || !PLASTWINDOW->m_sGroupData.pNextWindow.lock())
        return;

    if ((!BACK && PLASTWINDOW->m_sGroupData.pNextWindow->m_sGroupData.head) || (BACK && PLASTWINDOW->m_sGroupData.head)) {
        std::swap(PLASTWINDOW->m_sGroupData.head, PLASTWINDOW->m_sGroupData.pNextWindow->m_sGroupData.head);
        std::swap(PLASTWINDOW->m_sGroupData.locked, PLASTWINDOW->m_sGroupData.pNextWindow->m_sGroupData.locked);
    } else
        PLASTWINDOW->switchWithWindowInGroup(BACK ? PLASTWINDOW->getGroupPrevious() : PLASTWINDOW->m_sGroupData.pNextWindow.lock());

    PLASTWINDOW->updateWindowDecos();
}

void CKeybindManager::event(std::string args) {
    g_pEventManager->postEvent(SHyprIPCEvent{"custom", args});
}
