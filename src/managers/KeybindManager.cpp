#include "../config/ConfigValue.hpp"
#include "../devices/IKeyboard.hpp"
#include "../managers/SeatManager.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/ShortcutsInhibit.hpp"
#include "../protocols/GlobalShortcuts.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "KeybindManager.hpp"
#include "PointerManager.hpp"
#include "Compositor.hpp"
#include "TokenManager.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "debug/Log.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../render/Renderer.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../config/ConfigManager.hpp"

#include <optional>
#include <iterator>
#include <string>
#include <string_view>
#include <cstring>

#include <hyprutils/string/String.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::OS;

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

static std::vector<std::pair<std::string, std::string>> getHyprlandLaunchEnv(PHLWORKSPACE pInitialWorkspace) {
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!*PINITIALWSTRACKING || g_pConfigManager->m_isLaunchingExecOnce)
        return {};

    const auto PMONITOR = g_pCompositor->m_lastMonitor;
    if (!PMONITOR || !PMONITOR->activeWorkspace)
        return {};

    std::vector<std::pair<std::string, std::string>> result;

    if (!pInitialWorkspace) {
        if (PMONITOR->activeSpecialWorkspace)
            pInitialWorkspace = PMONITOR->activeSpecialWorkspace;
        else
            pInitialWorkspace = PMONITOR->activeWorkspace;
    }

    result.push_back(std::make_pair<>("HL_INITIAL_WORKSPACE_TOKEN",
                                      g_pTokenManager->registerNewToken(SInitialWorkspaceToken{{}, pInitialWorkspace->getConfigName()}, std::chrono::months(1337))));

    return result;
}

CKeybindManager::CKeybindManager() {
    // initialize all dispatchers

    m_mDispatchers["exec"]                           = spawn;
    m_mDispatchers["execr"]                          = spawnRaw;
    m_mDispatchers["killactive"]                     = closeActive;
    m_mDispatchers["forcekillactive"]                = killActive;
    m_mDispatchers["closewindow"]                    = closeWindow;
    m_mDispatchers["killwindow"]                     = killWindow;
    m_mDispatchers["signal"]                         = signalActive;
    m_mDispatchers["signalwindow"]                   = signalWindow;
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
    m_mDispatchers["toggleswallow"]                  = toggleSwallow;
    m_mDispatchers["submap"]                         = setSubmap;
    m_mDispatchers["pass"]                           = pass;
    m_mDispatchers["sendshortcut"]                   = sendshortcut;
    m_mDispatchers["sendkeystate"]                   = sendkeystate;
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
    m_mDispatchers["setprop"]                        = setProp;

    m_tScrollTimer.reset();

    m_pLongPressTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            if (!m_pLastLongPressKeybind || g_pSeatManager->keyboard.expired())
                return;

            const auto PACTIVEKEEB = g_pSeatManager->keyboard.lock();
            if (!PACTIVEKEEB->allowBinds)
                return;

            const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(m_pLastLongPressKeybind->handler);

            Debug::log(LOG, "Long press timeout passed, calling dispatcher.");
            DISPATCHER->second(m_pLastLongPressKeybind->arg);
        },
        nullptr);

    m_pRepeatKeyTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            if (m_vActiveKeybinds.size() == 0 || g_pSeatManager->keyboard.expired())
                return;

            const auto PACTIVEKEEB = g_pSeatManager->keyboard.lock();
            if (!PACTIVEKEEB->allowBinds)
                return;

            for (const auto& k : m_vActiveKeybinds) {
                const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(k->handler);

                Debug::log(LOG, "Keybind repeat triggered, calling dispatcher.");
                DISPATCHER->second(k->arg);
            }

            self->updateTimeout(std::chrono::milliseconds(1000 / PACTIVEKEEB->repeatRate));
        },
        nullptr);

    // null in --verify-config mode
    if (g_pEventLoopManager) {
        g_pEventLoopManager->addTimer(m_pLongPressTimer);
        g_pEventLoopManager->addTimer(m_pRepeatKeyTimer);
    }

    static auto P = g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) {
        // clear cuz realloc'd
        m_vActiveKeybinds.clear();
        m_pLastLongPressKeybind.reset();
        m_vPressedSpecialBinds.clear();
    });
}

CKeybindManager::~CKeybindManager() {
    if (m_pXKBTranslationState)
        xkb_state_unref(m_pXKBTranslationState);
    if (m_pLongPressTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_pLongPressTimer);
        m_pLongPressTimer.reset();
    }
    if (m_pRepeatKeyTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_pRepeatKeyTimer);
        m_pRepeatKeyTimer.reset();
    }
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_vKeybinds.emplace_back(makeShared<SKeybind>(kb));

    m_vActiveKeybinds.clear();
    m_pLastLongPressKeybind.reset();
}

void CKeybindManager::removeKeybind(uint32_t mod, const SParsedKey& key) {
    std::erase_if(m_vKeybinds, [&mod, &key](const auto& el) { return el->modmask == mod && el->key == key.key && el->keycode == key.keycode && el->catchAll == key.catchAll; });

    m_vActiveKeybinds.clear();
    m_pLastLongPressKeybind.reset();
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
    FILE* const       KEYMAPFILE = FILEPATH == "" ? nullptr : fopen(absolutePath(FILEPATH, g_pConfigManager->m_configCurrentPath).c_str(), "r");

    auto              PKEYMAP = KEYMAPFILE ? xkb_keymap_new_from_file(PCONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS) :
                                             xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (KEYMAPFILE)
        fclose(KEYMAPFILE);

    if (!PKEYMAP) {
        g_pHyprError->queueCreate("[Runtime Error] Invalid keyboard layout passed. ( rules: " + RULES + ", model: " + MODEL + ", variant: " + VARIANT + ", options: " + OPTIONS +
                                      ", layout: " + LAYOUT + " )",
                                  CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));

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

static void updateRelativeCursorCoords() {
    static auto PNOWARPS = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    if (*PNOWARPS)
        return;

    if (g_pCompositor->m_lastWindow)
        g_pCompositor->m_lastWindow->m_relativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - g_pCompositor->m_lastWindow->m_position;
}

bool CKeybindManager::tryMoveFocusToMonitor(PHLMONITOR monitor) {
    if (!monitor)
        return false;

    const auto LASTMONITOR = g_pCompositor->m_lastMonitor.lock();
    if (!LASTMONITOR)
        return false;
    if (LASTMONITOR == monitor) {
        Debug::log(LOG, "Tried to move to active monitor");
        return false;
    }

    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    const auto  PWORKSPACE        = g_pCompositor->m_lastMonitor->activeWorkspace;
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

void CKeybindManager::switchToWindow(PHLWINDOW PWINDOWTOCHANGETO, bool preserveFocusHistory) {
    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    const auto  PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (PWINDOWTOCHANGETO == PLASTWINDOW || !PWINDOWTOCHANGETO)
        return;

    // remove constraints
    g_pInputManager->unconstrainMouse();

    if (PLASTWINDOW && PLASTWINDOW->m_workspace == PWINDOWTOCHANGETO->m_workspace && PLASTWINDOW->isFullscreen()) {
        const auto PWORKSPACE = PLASTWINDOW->m_workspace;
        const auto MODE       = PWORKSPACE->m_fullscreenMode;

        if (!PWINDOWTOCHANGETO->m_pinned)
            g_pCompositor->setWindowFullscreenInternal(PLASTWINDOW, FSMODE_NONE);

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO, nullptr, preserveFocusHistory);

        if (!PWINDOWTOCHANGETO->m_pinned)
            g_pCompositor->setWindowFullscreenInternal(PWINDOWTOCHANGETO, MODE);

        // warp the position + size animation, otherwise it looks weird.
        PWINDOWTOCHANGETO->m_realPosition->warp();
        PWINDOWTOCHANGETO->m_realSize->warp();
    } else {
        updateRelativeCursorCoords();
        g_pCompositor->focusWindow(PWINDOWTOCHANGETO, nullptr, preserveFocusHistory);
        PWINDOWTOCHANGETO->warpCursor();

        // Move mouse focus to the new window if required by current follow_mouse and warp modes
        if (*PNOWARPS == 0 || *PFOLLOWMOUSE < 2) {
            g_pInputManager->m_pForcedFocus = PWINDOWTOCHANGETO;
            g_pInputManager->simulateMouseMovement();
            g_pInputManager->m_pForcedFocus.reset();
        }

        if (PLASTWINDOW && PLASTWINDOW->m_monitor != PWINDOWTOCHANGETO->m_monitor) {
            // event
            const auto PNEWMON = PWINDOWTOCHANGETO->m_monitor.lock();

            g_pCompositor->setActiveMonitor(PNEWMON);
        }
    }
};

bool CKeybindManager::onKeyEvent(std::any event, SP<IKeyboard> pKeyboard) {
    if (!g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        m_dPressedKeys.clear();
        return true;
    }

    if (!pKeyboard->allowBinds)
        return true;

    if (!m_pXKBTranslationState) {
        Debug::log(ERR, "BUG THIS: m_pXKBTranslationState nullptr!");
        updateXKBTranslationState();

        if (!m_pXKBTranslationState)
            return true;
    }

    auto               e = std::any_cast<IKeyboard::SKeyEvent>(event);

    const auto         KEYCODE = e.keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t keysym         = xkb_state_key_get_one_sym(pKeyboard->resolveBindsBySym ? pKeyboard->xkbSymState : m_pXKBTranslationState, KEYCODE);
    const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(pKeyboard->xkbState, KEYCODE);

    if (keysym == XKB_KEY_Escape || internalKeysym == XKB_KEY_Escape)
        PROTO::data->abortDndIfPresent();

    // handleInternalKeybinds returns true when the key should be suppressed,
    // while this function returns true when the key event should be sent
    if (handleInternalKeybinds(internalKeysym))
        return false;

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
        .mousePosAtPress    = g_pInputManager->getMouseCoordsInternal(),
    };

    m_vActiveKeybinds.clear();

    m_pLastLongPressKeybind.reset();

    bool suppressEvent = false;
    if (e.state == WL_KEYBOARD_KEY_STATE_PRESSED) {

        m_dPressedKeys.push_back(KEY);

        suppressEvent = !handleKeybinds(MODS, KEY, true).passEvent;

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
            suppressEvent = !handleKeybinds(MODS, KEY, false).passEvent;
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

    m_vActiveKeybinds.clear();

    bool found = false;
    if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (e.delta < 0)
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_down"}, true).passEvent;
        else
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_up"}, true).passEvent;
    } else if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        if (e.delta < 0)
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_left"}, true).passEvent;
        else
            found = !handleKeybinds(MODS, SPressedKeyWithMods{.keyName = "mouse_right"}, true).passEvent;
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
        .mousePosAtPress    = g_pInputManager->getMouseCoordsInternal(),
    };

    m_vActiveKeybinds.clear();

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_dPressedKeys.push_back(KEY);

        suppressEvent = !handleKeybinds(MODS, KEY, true).passEvent;

        if (suppressEvent)
            shadowKeybinds();

        m_dPressedKeys.back().sent = !suppressEvent;
    } else {
        bool foundInPressedKeys = false;
        for (auto it = m_dPressedKeys.begin(); it != m_dPressedKeys.end();) {
            if (it->keyName == KEY_NAME) {
                suppressEvent      = !handleKeybinds(MODS, *it, false).passEvent;
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
            suppressEvent = !handleKeybinds(MODS, KEY, false).passEvent;
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

eMultiKeyCase CKeybindManager::mkBindMatches(const SP<SKeybind> keybind) {
    if (mkKeysymSetMatches(keybind->sMkMods, m_sMkMods) != MK_FULL_MATCH)
        return MK_NO_MATCH;

    return mkKeysymSetMatches(keybind->sMkKeys, m_sMkKeys);
}

std::string CKeybindManager::getCurrentSubmap() {
    return m_szCurrentSelectedSubmap;
}

SDispatchResult CKeybindManager::handleKeybinds(const uint32_t modmask, const SPressedKeyWithMods& key, bool pressed) {
    static auto     PDISABLEINHIBIT = CConfigValue<Hyprlang::INT>("binds:disable_keybind_grabbing");
    static auto     PDRAGTHRESHOLD  = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    bool            found = false;
    SDispatchResult res;

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

    for (auto& k : m_vKeybinds) {
        const bool SPECIALDISPATCHER = k->handler == "global" || k->handler == "pass" || k->handler == "sendshortcut" || k->handler == "mouse";
        const bool SPECIALTRIGGERED =
            std::find_if(m_vPressedSpecialBinds.begin(), m_vPressedSpecialBinds.end(), [&](const auto& other) { return other == k; }) != m_vPressedSpecialBinds.end();
        const bool IGNORECONDITIONS =
            SPECIALDISPATCHER && !pressed && SPECIALTRIGGERED; // ignore mods. Pass, global dispatchers should be released immediately once the key is released.

        if (!k->dontInhibit && !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited())
            continue;

        if (!k->locked && g_pSessionLockManager->isSessionLocked())
            continue;

        if (!IGNORECONDITIONS && ((modmask != k->modmask && !k->ignoreMods) || k->submap != m_szCurrentSelectedSubmap || k->shadowed))
            continue;

        if (k->multiKey) {
            switch (mkBindMatches(k)) {
                case MK_NO_MATCH: continue;
                case MK_PARTIAL_MATCH: found = true; continue;
                case MK_FULL_MATCH: found = true;
            }
        } else if (!key.keyName.empty()) {
            if (key.keyName != k->key)
                continue;
        } else if (k->keycode != 0) {
            if (key.keycode != k->keycode)
                continue;
        } else if (k->catchAll) {
            if (found || key.submapAtPress != m_szCurrentSelectedSubmap)
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
            if (k->click && (g_pInputManager->m_bDragThresholdReached || THRESHOLDREACHED))
                continue;
            else if (k->drag && !g_pInputManager->m_bDragThresholdReached && !THRESHOLDREACHED)
                continue;
        }

        if (k->longPress) {
            const auto PACTIVEKEEB = g_pSeatManager->keyboard.lock();

            m_pLongPressTimer->updateTimeout(std::chrono::milliseconds(PACTIVEKEEB->repeatDelay));
            m_pLastLongPressKeybind = k;

            continue;
        }

        const auto DISPATCHER = m_mDispatchers.find(k->mouse ? "mouse" : k->handler);

        if (SPECIALTRIGGERED && !pressed)
            std::erase_if(m_vPressedSpecialBinds, [&](const auto& other) { return other == k; });
        else if (SPECIALDISPATCHER && pressed)
            m_vPressedSpecialBinds.emplace_back(k);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_mDispatchers.end()) {
            Debug::log(ERR, "Invalid handler in a keybind! (handler {} does not exist)", k->handler);
        } else {
            // call the dispatcher
            Debug::log(LOG, "Keybind triggered, calling dispatcher ({}, {}, {}, {})", modmask, key.keyName, key.keysym, DISPATCHER->first);

            m_iPassPressed = (int)pressed;

            // if the dispatchers says to pass event then we will
            if (k->handler == "mouse")
                res = DISPATCHER->second((pressed ? "1" : "0") + k->arg);
            else
                res = DISPATCHER->second(k->arg);

            m_iPassPressed = -1;

            if (k->handler == "submap") {
                found = true; // don't process keybinds on submap change.
                break;
            }
        }

        if (k->repeat) {
            const auto PACTIVEKEEB = g_pSeatManager->keyboard.lock();

            m_vActiveKeybinds.emplace_back(k);
            m_pRepeatKeyTimer->updateTimeout(std::chrono::milliseconds(PACTIVEKEEB->repeatDelay));
        }

        if (!k->nonConsuming)
            found = true;
    }

    g_pInputManager->m_bDragThresholdReached = false;

    // if keybind wasn't found (or dispatcher said to) then pass event
    res.passEvent |= !found;

    if (!found && !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited()) {
        Debug::log(LOG, "Keybind handling is disabled due to an inhibitor");

        res.success = false;
        if (res.error.empty())
            res.error = "Keybind handling is disabled due to an inhibitor";
    }

    return res;
}

void CKeybindManager::shadowKeybinds(const xkb_keysym_t& doesntHave, const uint32_t doesntHaveCode) {
    // shadow disables keybinds after one has been triggered

    for (auto& k : m_vKeybinds) {

        bool shadow = false;

        if (k->handler == "global" || k->transparent)
            continue; // can't be shadowed

        if (k->multiKey && (mkBindMatches(k) == MK_FULL_MATCH))
            shadow = true;
        else {
            const auto KBKEY      = xkb_keysym_from_name(k->key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
            const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);

            for (auto const& pk : m_dPressedKeys) {
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

        // vtnr is bugged for some reason.
        unsigned int                   ttynum = 0;
        Hyprutils::OS::CFileDescriptor fd{open("/dev/tty", O_RDONLY | O_NOCTTY)};
        if (fd.isValid()) {
#if defined(VT_GETSTATE)
            struct vt_stat st;
            if (!ioctl(fd.get(), VT_GETSTATE, &st))
                ttynum = st.v_active;
#elif defined(VT_GETACTIVE)
            int vt;
            if (!ioctl(fd.get(), VT_GETACTIVE, &vt))
                ttynum = vt;
#endif
        }

        if (ttynum == TTY)
            return true;

        Debug::log(LOG, "Switching from VT {} to VT {}", ttynum, TTY);

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

// Dispatchers
SDispatchResult CKeybindManager::spawn(std::string args) {
    const uint64_t PROC = spawnWithRules(args, nullptr);
    return {.success = PROC > 0, .error = std::format("Failed to start process {}", args)};
}

uint64_t CKeybindManager::spawnWithRules(std::string args, PHLWORKSPACE pInitialWorkspace) {

    args = trim(args);

    std::string RULES = "";

    if (args[0] == '[') {
        // we have exec rules
        RULES = args.substr(1, args.substr(1).find_first_of(']'));
        args  = args.substr(args.find_first_of(']') + 1);
    }

    const uint64_t PROC = spawnRawProc(args, pInitialWorkspace);

    if (!RULES.empty()) {
        const auto RULESLIST = CVarList(RULES, 0, ';');

        for (auto const& r : RULESLIST) {
            g_pConfigManager->addExecRule({r, (unsigned long)PROC});
        }

        Debug::log(LOG, "Applied {} rule arguments for exec.", RULESLIST.size());
    }

    return PROC;
}

SDispatchResult CKeybindManager::spawnRaw(std::string args) {
    const uint64_t PROC = spawnRawProc(args, nullptr);
    return {.success = PROC > 0, .error = std::format("Failed to start process {}", args)};
}

uint64_t CKeybindManager::spawnRawProc(std::string args, PHLWORKSPACE pInitialWorkspace) {
    Debug::log(LOG, "Executing {}", args);

    const auto HLENV = getHyprlandLaunchEnv(pInitialWorkspace);

    int        socket[2];
    if (pipe(socket) != 0) {
        Debug::log(LOG, "Unable to create pipe for fork");
    }

    CFileDescriptor pipeSock[2] = {CFileDescriptor{socket[0]}, CFileDescriptor{socket[1]}};

    pid_t           child, grandchild;
    child = fork();
    if (child < 0) {
        Debug::log(LOG, "Fail to create the first fork");
        return 0;
    }
    if (child == 0) {
        // run in child
        g_pCompositor->restoreNofile();

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, nullptr);

        grandchild = fork();
        if (grandchild == 0) {
            // run in grandchild
            for (auto const& e : HLENV) {
                setenv(e.first.c_str(), e.second.c_str(), 1);
            }
            setenv("WAYLAND_DISPLAY", g_pCompositor->m_wlDisplaySocket.c_str(), 1);

            int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (devnull != -1) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
            // exit grandchild
            _exit(0);
        }
        write(pipeSock[1].get(), &grandchild, sizeof(grandchild));
        // exit child
        _exit(0);
    }
    // run in parent
    read(pipeSock[0].get(), &grandchild, sizeof(grandchild));
    // clear child and leave grandchild to init
    waitpid(child, nullptr, 0);
    if (grandchild < 0) {
        Debug::log(LOG, "Fail to create the second fork");
        return 0;
    }

    Debug::log(LOG, "Process Created with pid {}", grandchild);

    return grandchild;
}

SDispatchResult CKeybindManager::killActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW) {
        Debug::log(ERR, "killActive: no window found");
        return {.success = false, .error = "killActive: no window found"};
    }

    kill(PWINDOW->getPID(), SIGKILL);

    return {};
}

SDispatchResult CKeybindManager::closeActive(std::string args) {
    g_pCompositor->closeWindow(g_pCompositor->m_lastWindow.lock());

    return {};
}

SDispatchResult CKeybindManager::closeWindow(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(args);

    if (!PWINDOW) {
        Debug::log(ERR, "closeWindow: no window found");
        return {.success = false, .error = "closeWindow: no window found"};
    }

    g_pCompositor->closeWindow(PWINDOW);

    return {};
}

SDispatchResult CKeybindManager::killWindow(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(args);

    if (!PWINDOW) {
        Debug::log(ERR, "killWindow: no window found");
        return {.success = false, .error = "killWindow: no window found"};
    }

    kill(PWINDOW->getPID(), SIGKILL);

    return {};
}

SDispatchResult CKeybindManager::signalActive(std::string args) {
    if (!isNumber(args))
        return {.success = false, .error = "signalActive: signal has to be int"};

    try {
        const auto SIGNALNUM = std::stoi(args);
        if (SIGNALNUM < 1 || SIGNALNUM > 31) {
            Debug::log(ERR, "signalActive: invalid signal number {}", SIGNALNUM);
            return {.success = false, .error = std::format("signalActive: invalid signal number {}", SIGNALNUM)};
        }
        kill(g_pCompositor->m_lastWindow.lock()->getPID(), SIGNALNUM);
    } catch (const std::exception& e) {
        Debug::log(ERR, "signalActive: invalid signal format \"{}\"", args);
        return {.success = false, .error = std::format("signalActive: invalid signal format \"{}\"", args)};
    }

    kill(g_pCompositor->m_lastWindow.lock()->getPID(), std::stoi(args));

    return {};
}

SDispatchResult CKeybindManager::signalWindow(std::string args) {
    const auto WINDOWREGEX = args.substr(0, args.find_first_of(','));
    const auto SIGNAL      = args.substr(args.find_first_of(',') + 1);

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "signalWindow: no window");
        return {.success = false, .error = "signalWindow: no window"};
    }

    if (!std::all_of(SIGNAL.begin(), SIGNAL.end(), ::isdigit))
        return {.success = false, .error = "signalWindow: signal has to be int"};

    try {
        const auto SIGNALNUM = std::stoi(SIGNAL);
        if (SIGNALNUM < 1 || SIGNALNUM > 31) {
            Debug::log(ERR, "signalWindow: invalid signal number {}", SIGNALNUM);
            return {.success = false, .error = std::format("signalWindow: invalid signal number {}", SIGNALNUM)};
        }
        kill(PWINDOW->getPID(), SIGNALNUM);
    } catch (const std::exception& e) {
        Debug::log(ERR, "signalWindow: invalid signal format \"{}\"", SIGNAL);
        return {.success = false, .error = std::format("signalWindow: invalid signal format \"{}\"", SIGNAL)};
    }

    return {};
}

void CKeybindManager::clearKeybinds() {
    m_vKeybinds.clear();
}

static SDispatchResult toggleActiveFloatingCore(std::string args, std::optional<bool> floatState) {
    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    if (floatState.has_value() && floatState == PWINDOW->m_isFloating)
        return {};

    // remove drag status
    if (!g_pInputManager->currentlyDraggedWindow.expired())
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);

    if (PWINDOW->m_groupData.pNextWindow.lock() && PWINDOW->m_groupData.pNextWindow.lock() != PWINDOW) {
        const auto PCURRENT = PWINDOW->getGroupCurrent();

        PCURRENT->m_isFloating = !PCURRENT->m_isFloating;
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PCURRENT);

        PHLWINDOW curr = PCURRENT->m_groupData.pNextWindow.lock();
        while (curr != PCURRENT) {
            curr->m_isFloating = PCURRENT->m_isFloating;
            curr               = curr->m_groupData.pNextWindow.lock();
        }
    } else {
        PWINDOW->m_isFloating = !PWINDOW->m_isFloating;

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PWINDOW);
    }

    if (PWINDOW->m_workspace) {
        PWINDOW->m_workspace->updateWindows();
        PWINDOW->m_workspace->updateWindowData();
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->monitorID());
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    return {};
}

SDispatchResult CKeybindManager::toggleActiveFloating(std::string args) {
    return toggleActiveFloatingCore(args, std::nullopt);
}

SDispatchResult CKeybindManager::setActiveFloating(std::string args) {
    return toggleActiveFloatingCore(args, true);
}

SDispatchResult CKeybindManager::setActiveTiled(std::string args) {
    return toggleActiveFloatingCore(args, false);
}

SDispatchResult CKeybindManager::centerWindow(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW || !PWINDOW->m_isFloating || PWINDOW->isFullscreen())
        return {.success = false, .error = "No floating window found"};

    const auto PMONITOR = PWINDOW->m_monitor.lock();

    auto       RESERVEDOFFSET = Vector2D();
    if (args == "1")
        RESERVEDOFFSET = (PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight) / 2.f;

    *PWINDOW->m_realPosition = PMONITOR->middle() - PWINDOW->m_realSize->goal() / 2.f + RESERVEDOFFSET;
    PWINDOW->m_position      = PWINDOW->m_realPosition->goal();

    return {};
}

SDispatchResult CKeybindManager::toggleActivePseudo(std::string args) {
    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    PWINDOW->m_isPseudotiled = !PWINDOW->m_isPseudotiled;

    if (!PWINDOW->isFullscreen())
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);

    return {};
}

static SWorkspaceIDName getWorkspaceToChangeFromArgs(std::string args, PHLWORKSPACE PCURRENTWORKSPACE, PHLMONITORREF PMONITOR) {
    if (!args.starts_with("previous")) {
        return getWorkspaceIDNameFromString(args);
    }

    const bool             PER_MON = args.contains("_per_monitor");
    const SWorkspaceIDName PPREVWS = PER_MON ? PMONITOR->getPrevWorkspaceIDName(PCURRENTWORKSPACE->m_id) : PCURRENTWORKSPACE->getPrevWorkspaceIDName();
    // Do nothing if there's no previous workspace, otherwise switch to it.
    if (PPREVWS.id == -1 || PPREVWS.id == PCURRENTWORKSPACE->m_id) {
        Debug::log(LOG, "No previous workspace to change to");
        return {.id = WORKSPACE_NOT_CHANGED};
    }

    if (const auto PWORKSPACETOCHANGETO = g_pCompositor->getWorkspaceByID(PPREVWS.id); PWORKSPACETOCHANGETO) {
        return {.id = PWORKSPACETOCHANGETO->m_id, .name = PWORKSPACETOCHANGETO->m_name};
    }

    return {.id = PPREVWS.id, .name = PPREVWS.name.empty() ? std::to_string(PPREVWS.id) : PPREVWS.name};
}

SDispatchResult CKeybindManager::changeworkspace(std::string args) {
    // Workspace_back_and_forth being enabled means that an attempt to switch to
    // the current workspace will instead switch to the previous.
    static auto PBACKANDFORTH                 = CConfigValue<Hyprlang::INT>("binds:workspace_back_and_forth");
    static auto PALLOWWORKSPACECYCLES         = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");
    static auto PWORKSPACECENTERON            = CConfigValue<Hyprlang::INT>("binds:workspace_center_on");
    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Hyprlang::INT>("binds:hide_special_on_workspace_change");

    const auto  PMONITOR = g_pCompositor->m_lastMonitor.lock();

    if (!PMONITOR)
        return {.success = false, .error = "Last monitor not found"};

    const auto PCURRENTWORKSPACE = PMONITOR->activeWorkspace;
    const bool EXPLICITPREVIOUS  = args.contains("previous");

    const auto& [workspaceToChangeTo, workspaceName] = getWorkspaceToChangeFromArgs(args, PCURRENTWORKSPACE, PMONITOR);
    if (workspaceToChangeTo == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return {.success = false, .error = "Error in changeworkspace, invalid value"};
    }

    if (workspaceToChangeTo == WORKSPACE_NOT_CHANGED)
        return {};

    const SWorkspaceIDName PPREVWS = args.contains("_per_monitor") ? PMONITOR->getPrevWorkspaceIDName(PCURRENTWORKSPACE->m_id) : PCURRENTWORKSPACE->getPrevWorkspaceIDName();

    const bool             BISWORKSPACECURRENT = workspaceToChangeTo == PCURRENTWORKSPACE->m_id;
    if (BISWORKSPACECURRENT && (!(*PBACKANDFORTH || EXPLICITPREVIOUS) || PPREVWS.id == -1)) {
        if (*PHIDESPECIALONWORKSPACECHANGE)
            PMONITOR->setSpecialWorkspace(nullptr);

        return {.success = false, .error = "Previous workspace doesn't exist"};
    }

    g_pInputManager->unconstrainMouse();
    g_pInputManager->m_bEmptyFocusCursorSet = false;

    auto pWorkspaceToChangeTo = g_pCompositor->getWorkspaceByID(BISWORKSPACECURRENT ? PPREVWS.id : workspaceToChangeTo);
    if (!pWorkspaceToChangeTo)
        pWorkspaceToChangeTo =
            g_pCompositor->createNewWorkspace(BISWORKSPACECURRENT ? PPREVWS.id : workspaceToChangeTo, PMONITOR->ID, BISWORKSPACECURRENT ? PPREVWS.name : workspaceName);

    if (!BISWORKSPACECURRENT && pWorkspaceToChangeTo->m_isSpecialWorkspace) {
        PMONITOR->setSpecialWorkspace(pWorkspaceToChangeTo);
        g_pInputManager->simulateMouseMovement();
        return {};
    }

    g_pInputManager->releaseAllMouseButtons();

    const auto PMONITORWORKSPACEOWNER = PMONITOR == pWorkspaceToChangeTo->m_monitor ? PMONITOR : pWorkspaceToChangeTo->m_monitor.lock();

    if (!PMONITORWORKSPACEOWNER)
        return {.success = false, .error = "Workspace to switch to has no monitor"};

    updateRelativeCursorCoords();

    g_pCompositor->setActiveMonitor(PMONITORWORKSPACEOWNER);

    if (BISWORKSPACECURRENT) {
        if (*PALLOWWORKSPACECYCLES)
            pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);
        else if (!EXPLICITPREVIOUS && !*PBACKANDFORTH)
            pWorkspaceToChangeTo->rememberPrevWorkspace(nullptr);
    } else
        pWorkspaceToChangeTo->rememberPrevWorkspace(PCURRENTWORKSPACE);

    if (*PHIDESPECIALONWORKSPACECHANGE)
        PMONITORWORKSPACEOWNER->setSpecialWorkspace(nullptr);
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
        if (g_pCompositor->m_lastFocus)
            g_pInputManager->sendMotionEventsToFocused();
        else
            g_pInputManager->simulateMouseMovement();
    }

    const static auto PWARPONWORKSPACECHANGE = CConfigValue<Hyprlang::INT>("cursor:warp_on_change_workspace");

    if (*PWARPONWORKSPACECHANGE > 0) {
        auto PLAST     = pWorkspaceToChangeTo->getLastFocusedWindow();
        auto HLSurface = CWLSurface::fromResource(g_pSeatManager->state.pointerFocus.lock());

        if (PLAST && (!HLSurface || HLSurface->getWindow()))
            PLAST->warpCursor(*PWARPONWORKSPACECHANGE == 2);
    }

    return {};
}

SDispatchResult CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    const eFullscreenMode MODE = args == "1" ? FSMODE_MAXIMIZED : FSMODE_FULLSCREEN;

    if (PWINDOW->isEffectiveInternalFSMode(MODE))
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
    else
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, MODE);

    return {};
}

SDispatchResult CKeybindManager::fullscreenStateActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();
    const auto ARGS    = CVarList(args, 2, ' ');

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    PWINDOW->m_windowData.syncFullscreen = CWindowOverridableVar(false, PRIORITY_SET_PROP);

    int internalMode, clientMode;
    try {
        internalMode = std::stoi(ARGS[0]);
    } catch (std::exception& e) { internalMode = -1; }
    try {
        clientMode = std::stoi(ARGS[1]);
    } catch (std::exception& e) { clientMode = -1; }

    const SFullscreenState STATE = SFullscreenState{.internal = (internalMode != -1 ? (eFullscreenMode)internalMode : PWINDOW->m_fullscreenState.internal),
                                                    .client   = (clientMode != -1 ? (eFullscreenMode)clientMode : PWINDOW->m_fullscreenState.client)};

    if (internalMode != -1 && clientMode != -1 && PWINDOW->m_fullscreenState.internal == STATE.internal && PWINDOW->m_fullscreenState.client == STATE.client)
        g_pCompositor->setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
    else if (internalMode != -1 && clientMode == -1 && PWINDOW->m_fullscreenState.internal == STATE.internal)
        g_pCompositor->setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = FSMODE_NONE, .client = PWINDOW->m_fullscreenState.client});
    else if (internalMode == -1 && clientMode != -1 && PWINDOW->m_fullscreenState.client == STATE.client)
        g_pCompositor->setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = PWINDOW->m_fullscreenState.internal, .client = FSMODE_NONE});
    else
        g_pCompositor->setWindowFullscreenState(PWINDOW, STATE);

    PWINDOW->m_windowData.syncFullscreen = CWindowOverridableVar(PWINDOW->m_fullscreenState.internal == PWINDOW->m_fullscreenState.client, PRIORITY_SET_PROP);

    return {};
}

SDispatchResult CKeybindManager::moveActiveToWorkspace(std::string args) {

    PHLWINDOW PWINDOW = nullptr;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_lastWindow.lock();
    }

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    const auto& [WORKSPACEID, workspaceName] = getWorkspaceIDNameFromString(args);
    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(LOG, "Invalid workspace in moveActiveToWorkspace");
        return {.success = false, .error = "Invalid workspace in moveActiveToWorkspace"};
    }

    if (WORKSPACEID == PWINDOW->workspaceID()) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return {.success = false, .error = "Not moving to workspace because it didn't change."};
    }

    auto        pWorkspace            = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    PHLMONITOR  pMonitor              = nullptr;
    const auto  POLDWS                = PWINDOW->m_workspace;
    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");

    updateRelativeCursorCoords();

    g_pHyprRenderer->damageWindow(PWINDOW);

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
        pMonitor = pWorkspace->m_monitor.lock();
        g_pCompositor->setActiveMonitor(pMonitor);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->monitorID(), workspaceName, false);
        pMonitor   = pWorkspace->m_monitor.lock();
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    }

    POLDWS->m_lastFocusedWindow = POLDWS->getFirstWindow();

    if (pWorkspace->m_isSpecialWorkspace)
        pMonitor->setSpecialWorkspace(pWorkspace);
    else if (POLDWS->m_isSpecialWorkspace)
        POLDWS->m_monitor.lock()->setSpecialWorkspace(nullptr);

    if (*PALLOWWORKSPACECYCLES)
        pWorkspace->rememberPrevWorkspace(POLDWS);

    pMonitor->changeWorkspace(pWorkspace);

    g_pCompositor->focusWindow(PWINDOW);
    PWINDOW->warpCursor();

    return {};
}

SDispatchResult CKeybindManager::moveActiveToWorkspaceSilent(std::string args) {
    PHLWINDOW PWINDOW = nullptr;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args    = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_lastWindow.lock();
    }

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    const auto& [WORKSPACEID, workspaceName] = getWorkspaceIDNameFromString(args);
    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(ERR, "Error in moveActiveToWorkspaceSilent, invalid value");
        return {.success = false, .error = "Error in moveActiveToWorkspaceSilent, invalid value"};
    }

    if (WORKSPACEID == PWINDOW->workspaceID())
        return {};

    g_pHyprRenderer->damageWindow(PWINDOW);

    auto       pWorkspace = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    const auto OLDMIDDLE  = PWINDOW->middle();

    if (pWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    } else {
        pWorkspace = g_pCompositor->createNewWorkspace(WORKSPACEID, PWINDOW->monitorID(), workspaceName, false);
        g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, pWorkspace);
    }

    if (PWINDOW == g_pCompositor->m_lastWindow) {
        if (const auto PATCOORDS = g_pCompositor->vectorToWindowUnified(OLDMIDDLE, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, PWINDOW); PATCOORDS)
            g_pCompositor->focusWindow(PATCOORDS);
        else
            g_pInputManager->refocus();
    }

    return {};
}

SDispatchResult CKeybindManager::moveFocusTo(std::string args) {
    static auto PFULLCYCLE       = CConfigValue<Hyprlang::INT>("binds:movefocus_cycles_fullscreen");
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");
    static auto PGROUPCYCLE      = CConfigValue<Hyprlang::INT>("binds:movefocus_cycles_groupfirst");
    char        arg              = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move focus in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return {.success = false, .error = std::format("Cannot move focus in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg)};
    }

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();
    if (!PLASTWINDOW) {
        if (*PMONITORFALLBACK)
            tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg));

        return {};
    }

    const auto PWINDOWTOCHANGETO = *PFULLCYCLE && PLASTWINDOW->isFullscreen() ?
        g_pCompositor->getWindowCycle(PLASTWINDOW, true, {}, false, arg != 'd' && arg != 'b' && arg != 'r') :
        g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);

    // Prioritize focus change within groups if the window is a part of it.
    if (*PGROUPCYCLE && PLASTWINDOW->m_groupData.pNextWindow) {
        auto isTheOnlyGroupOnWs = !PWINDOWTOCHANGETO && g_pCompositor->m_monitors.size() == 1;
        if (arg == 'l' && (PLASTWINDOW != PLASTWINDOW->getGroupHead() || isTheOnlyGroupOnWs)) {
            PLASTWINDOW->setGroupCurrent(PLASTWINDOW->getGroupPrevious());
            return {};
        }

        else if (arg == 'r' && (PLASTWINDOW != PLASTWINDOW->getGroupTail() || isTheOnlyGroupOnWs)) {
            PLASTWINDOW->setGroupCurrent(PLASTWINDOW->m_groupData.pNextWindow.lock());
            return {};
        }
    }

    // Found window in direction, switch to it
    if (PWINDOWTOCHANGETO) {
        switchToWindow(PWINDOWTOCHANGETO);
        return {};
    }

    Debug::log(LOG, "No window found in direction {}, looking for a monitor", arg);

    if (*PMONITORFALLBACK && tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(arg)))
        return {};

    static auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");
    if (*PNOFALLBACK)
        return {.success = false, .error = std::format("Nothing to focus to in direction {}", arg)};

    Debug::log(LOG, "No monitor found in direction {}, getting the inverse edge", arg);

    const auto PMONITOR = PLASTWINDOW->m_monitor.lock();

    if (!PMONITOR)
        return {.success = false, .error = "last window has no monitor?"};

    if (arg == 'l' || arg == 'r') {
        if (STICKS(PLASTWINDOW->m_position.x, PMONITOR->vecPosition.x) && STICKS(PLASTWINDOW->m_size.x, PMONITOR->vecSize.x))
            return {.success = false, .error = "move does not make sense, would return back"};
    } else if (STICKS(PLASTWINDOW->m_position.y, PMONITOR->vecPosition.y) && STICKS(PLASTWINDOW->m_size.y, PMONITOR->vecSize.y))
        return {.success = false, .error = "move does not make sense, would return back"};

    CBox box = PMONITOR->logicalBox();
    switch (arg) {
        case 'l':
            box.x += box.w;
            box.w = 1;
            break;
        case 'r':
            box.x -= 1;
            box.w = 1;
            break;
        case 'u':
        case 't':
            box.y += box.h;
            box.h = 1;
            break;
        case 'd':
        case 'b':
            box.y -= 1;
            box.h = 1;
            break;
    }

    const auto PWINDOWCANDIDATE = g_pCompositor->getWindowInDirection(box, PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace, arg,
                                                                      PLASTWINDOW, PLASTWINDOW->m_isFloating);
    if (PWINDOWCANDIDATE)
        switchToWindow(PWINDOWCANDIDATE);

    return {};
}

SDispatchResult CKeybindManager::focusUrgentOrLast(std::string args) {
    const auto PWINDOWURGENT = g_pCompositor->getUrgentWindow();
    const auto PWINDOWPREV   = g_pCompositor->m_lastWindow.lock() ? (g_pCompositor->m_windowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_windowFocusHistory[1].lock()) :
                                                                    (g_pCompositor->m_windowFocusHistory.empty() ? nullptr : g_pCompositor->m_windowFocusHistory[0].lock());

    if (!PWINDOWURGENT && !PWINDOWPREV)
        return {.success = false, .error = "Window not found"};

    switchToWindow(PWINDOWURGENT ? PWINDOWURGENT : PWINDOWPREV);

    return {};
}

SDispatchResult CKeybindManager::focusCurrentOrLast(std::string args) {
    const auto PWINDOWPREV = g_pCompositor->m_lastWindow.lock() ? (g_pCompositor->m_windowFocusHistory.size() < 2 ? nullptr : g_pCompositor->m_windowFocusHistory[1].lock()) :
                                                                  (g_pCompositor->m_windowFocusHistory.empty() ? nullptr : g_pCompositor->m_windowFocusHistory[0].lock());

    if (!PWINDOWPREV)
        return {.success = false, .error = "Window not found"};

    switchToWindow(PWINDOWPREV);

    return {};
}

SDispatchResult CKeybindManager::swapActive(std::string args) {
    char arg = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return {.success = false, .error = std::format("Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg)};
    }

    Debug::log(LOG, "Swapping active window in direction {}", arg);
    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "Window to swap with not found"};

    if (PLASTWINDOW->isFullscreen())
        return {.success = false, .error = "Can't swap fullscreen window"};

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (!PWINDOWTOCHANGETO)
        return {.success = false, .error = "Window to swap with not found"};

    updateRelativeCursorCoords();
    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, PWINDOWTOCHANGETO);
    PLASTWINDOW->warpCursor();

    return {};
}

SDispatchResult CKeybindManager::moveActiveTo(std::string args) {
    char arg    = args[0];
    bool silent = args.ends_with(" silent");
    if (silent)
        args = args.substr(0, args.length() - 7);

    if (args.starts_with("mon:")) {
        const auto PNEWMONITOR = g_pCompositor->getMonitorFromString(args.substr(4));
        if (!PNEWMONITOR)
            return {.success = false, .error = std::format("Monitor {} not found", args.substr(4))};

        if (silent)
            moveActiveToWorkspaceSilent(PNEWMONITOR->activeWorkspace->getConfigName());
        else
            moveActiveToWorkspace(PNEWMONITOR->activeWorkspace->getConfigName());

        return {};
    }

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return {.success = false, .error = std::format("Cannot move window in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg)};
    }

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "Window to move not found"};

    if (PLASTWINDOW->isFullscreen())
        return {.success = false, .error = "Can't move fullscreen window"};

    if (PLASTWINDOW->m_isFloating) {
        std::optional<float> vPosx, vPosy;
        const auto           PMONITOR   = PLASTWINDOW->m_monitor.lock();
        const auto           BORDERSIZE = PLASTWINDOW->getRealBorderSize();

        switch (arg) {
            case 'l': vPosx = PMONITOR->vecReservedTopLeft.x + BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 'r': vPosx = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PLASTWINDOW->m_realSize->goal().x - BORDERSIZE + PMONITOR->vecPosition.x; break;
            case 't':
            case 'u': vPosy = PMONITOR->vecReservedTopLeft.y + BORDERSIZE + PMONITOR->vecPosition.y; break;
            case 'b':
            case 'd': vPosy = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PLASTWINDOW->m_realSize->goal().y - BORDERSIZE + PMONITOR->vecPosition.y; break;
        }

        *PLASTWINDOW->m_realPosition = Vector2D(vPosx.value_or(PLASTWINDOW->m_realPosition->goal().x), vPosy.value_or(PLASTWINDOW->m_realPosition->goal().y));

        return {};
    }

    // If the window to change to is on the same workspace, switch them
    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);
    if (PWINDOWTOCHANGETO) {
        updateRelativeCursorCoords();

        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PLASTWINDOW, args, silent);
        if (!silent)
            PLASTWINDOW->warpCursor();
        return {};
    }

    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");
    if (!*PMONITORFALLBACK)
        return {};

    // Otherwise, we always want to move to the next monitor in that direction
    const auto PMONITORTOCHANGETO = g_pCompositor->getMonitorInDirection(arg);
    if (!PMONITORTOCHANGETO)
        return {.success = false, .error = "Nowhere to move active window to"};

    const auto PWORKSPACE = PMONITORTOCHANGETO->activeWorkspace;
    if (silent)
        moveActiveToWorkspaceSilent(PWORKSPACE->getConfigName());
    else
        moveActiveToWorkspace(PWORKSPACE->getConfigName());

    return {};
}

SDispatchResult CKeybindManager::toggleGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    if (PWINDOW->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    if (PWINDOW->m_groupData.pNextWindow.expired())
        PWINDOW->createGroup();
    else
        PWINDOW->destroyGroup();

    return {};
}

SDispatchResult CKeybindManager::changeGroupActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    if (PWINDOW->m_groupData.pNextWindow.expired())
        return {.success = false, .error = "No next window in group"};

    if (PWINDOW->m_groupData.pNextWindow.lock() == PWINDOW)
        return {.success = false, .error = "Only one window in group"};

    if (isNumber(args, false)) {
        // index starts from '1'; '0' means last window
        const int INDEX = std::stoi(args);
        if (INDEX > PWINDOW->getGroupSize())
            return {.success = false, .error = "Index too big, there aren't that many windows in this group"};
        if (INDEX == 0)
            PWINDOW->setGroupCurrent(PWINDOW->getGroupTail());
        else
            PWINDOW->setGroupCurrent(PWINDOW->getGroupWindowByIndex(INDEX - 1));
        return {};
    }

    if (args != "b" && args != "prev") {
        PWINDOW->setGroupCurrent(PWINDOW->m_groupData.pNextWindow.lock());
    } else {
        PWINDOW->setGroupCurrent(PWINDOW->getGroupPrevious());
    }

    return {};
}

SDispatchResult CKeybindManager::toggleSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_lastWindow.lock();

    if (!header.pWindow)
        return {.success = false, .error = "Window not found"};

    const auto PWORKSPACE = header.pWindow->m_workspace;

    if (PWORKSPACE->m_hasFullscreenWindow)
        return {.success = false, .error = "Can't split windows that already split"};

    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "togglesplit");

    return {};
}

SDispatchResult CKeybindManager::swapSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_lastWindow.lock();

    if (!header.pWindow)
        return {.success = false, .error = "Window not found"};

    const auto PWORKSPACE = header.pWindow->m_workspace;

    if (PWORKSPACE->m_hasFullscreenWindow)
        return {.success = false, .error = "Can't split windows that already split"};

    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "swapsplit");

    return {};
}

SDispatchResult CKeybindManager::alterSplitRatio(std::string args) {
    std::optional<float> splitResult;
    bool                 exact = false;

    if (args.starts_with("exact")) {
        exact       = true;
        splitResult = getPlusMinusKeywordResult(args.substr(5), 0);
    } else
        splitResult = getPlusMinusKeywordResult(args, 0);

    if (!splitResult.has_value()) {
        Debug::log(ERR, "Splitratio invalid in alterSplitRatio!");
        return {.success = false, .error = "Splitratio invalid in alterSplitRatio!"};
    }

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "Window not found"};

    g_pLayoutManager->getCurrentLayout()->alterSplitRatio(PLASTWINDOW, splitResult.value(), exact);

    return {};
}

SDispatchResult CKeybindManager::focusMonitor(std::string arg) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(arg);
    tryMoveFocusToMonitor(PMONITOR);

    return {};
}

SDispatchResult CKeybindManager::moveCursorToCorner(std::string arg) {
    if (!isNumber(arg)) {
        Debug::log(ERR, "moveCursorToCorner, arg has to be a number.");
        return {.success = false, .error = "moveCursorToCorner, arg has to be a number."};
    }

    const auto CORNER = std::stoi(arg);

    if (CORNER < 0 || CORNER > 3) {
        Debug::log(ERR, "moveCursorToCorner, corner not 0 - 3.");
        return {.success = false, .error = "moveCursorToCorner, corner not 0 - 3."};
    }

    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    switch (CORNER) {
        case 0:
            // bottom left
            g_pCompositor->warpCursorTo({PWINDOW->m_realPosition->value().x, PWINDOW->m_realPosition->value().y + PWINDOW->m_realSize->value().y}, true);
            break;
        case 1:
            // bottom right
            g_pCompositor->warpCursorTo({PWINDOW->m_realPosition->value().x + PWINDOW->m_realSize->value().x, PWINDOW->m_realPosition->value().y + PWINDOW->m_realSize->value().y},
                                        true);
            break;
        case 2:
            // top right
            g_pCompositor->warpCursorTo({PWINDOW->m_realPosition->value().x + PWINDOW->m_realSize->value().x, PWINDOW->m_realPosition->value().y}, true);
            break;
        case 3:
            // top left
            g_pCompositor->warpCursorTo({PWINDOW->m_realPosition->value().x, PWINDOW->m_realPosition->value().y}, true);
            break;
    }

    return {};
}

SDispatchResult CKeybindManager::moveCursor(std::string args) {
    std::string x_str, y_str;
    int         x, y;

    size_t      i = args.find_first_of(' ');
    if (i == std::string::npos) {
        Debug::log(ERR, "moveCursor, takes 2 arguments.");
        return {.success = false, .error = "moveCursor, takes 2 arguments"};
    }

    x_str = args.substr(0, i);
    y_str = args.substr(i + 1);

    if (!isNumber(x_str)) {
        Debug::log(ERR, "moveCursor, x argument has to be a number.");
        return {.success = false, .error = "moveCursor, x argument has to be a number."};
    }
    if (!isNumber(y_str)) {
        Debug::log(ERR, "moveCursor, y argument has to be a number.");
        return {.success = false, .error = "moveCursor, y argument has to be a number."};
    }

    x = std::stoi(x_str);
    y = std::stoi(y_str);

    g_pCompositor->warpCursorTo({x, y}, true);

    return {};
}

SDispatchResult CKeybindManager::workspaceOpt(std::string args) {

    // current workspace
    const auto PWORKSPACE = g_pCompositor->m_lastMonitor->activeWorkspace;

    if (!PWORKSPACE)
        return {.success = false, .error = "Workspace not found"}; // ????

    if (args == "allpseudo") {
        PWORKSPACE->m_defaultPseudo = !PWORKSPACE->m_defaultPseudo;

        // apply
        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->m_workspace != PWORKSPACE)
                continue;

            w->m_isPseudotiled = PWORKSPACE->m_defaultPseudo;
        }
    } else if (args == "allfloat") {
        PWORKSPACE->m_defaultFloating = !PWORKSPACE->m_defaultFloating;
        // apply

        // we make a copy because changeWindowFloatingMode might invalidate the iterator
        std::vector<PHLWINDOW> ptrs(g_pCompositor->m_windows.begin(), g_pCompositor->m_windows.end());

        for (auto const& w : ptrs) {
            if (!w->m_isMapped || w->m_workspace != PWORKSPACE || w->isHidden())
                continue;

            if (!w->m_requestsFloat && w->m_isFloating != PWORKSPACE->m_defaultFloating) {
                const auto SAVEDPOS  = w->m_realPosition->goal();
                const auto SAVEDSIZE = w->m_realSize->goal();

                w->m_isFloating = PWORKSPACE->m_defaultFloating;
                g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(w);

                if (PWORKSPACE->m_defaultFloating) {
                    w->m_realPosition->setValueAndWarp(SAVEDPOS);
                    w->m_realSize->setValueAndWarp(SAVEDSIZE);
                    *w->m_realSize     = w->m_realSize->value() + Vector2D(4, 4);
                    *w->m_realPosition = w->m_realPosition->value() - Vector2D(2, 2);
                }
            }
        }
    } else {
        Debug::log(ERR, "Invalid arg in workspaceOpt, opt \"{}\" doesn't exist.", args);
        return {.success = false, .error = std::format("Invalid arg in workspaceOpt, opt \"{}\" doesn't exist.", args)};
    }

    // recalc mon
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(g_pCompositor->m_lastMonitor->ID);

    return {};
}

SDispatchResult CKeybindManager::renameWorkspace(std::string args) {
    try {
        const auto FIRSTSPACEPOS = args.find_first_of(' ');
        if (FIRSTSPACEPOS != std::string::npos) {
            int         workspace = std::stoi(args.substr(0, FIRSTSPACEPOS));
            std::string name      = args.substr(FIRSTSPACEPOS + 1);
            if (const auto& PWS = g_pCompositor->getWorkspaceByID(workspace); PWS)
                PWS->rename(name);
            else
                return {.success = false, .error = "No such workspace"};
        } else if (const auto& PWS = g_pCompositor->getWorkspaceByID(std::stoi(args)); PWS)
            PWS->rename("");
        else
            return {.success = false, .error = "No such workspace"};
    } catch (std::exception& e) {
        Debug::log(ERR, R"(Invalid arg in renameWorkspace, expected numeric id only or a numeric id and string name. "{}": "{}")", args, e.what());
        return {.success = false, .error = std::format(R"(Invalid arg in renameWorkspace, expected numeric id only or a numeric id and string name. "{}": "{}")", args, e.what())};
    }

    return {};
}

SDispatchResult CKeybindManager::exitHyprland(std::string argz) {
    g_pConfigManager->dispatchExecShutdown();

    if (g_pCompositor->m_finalRequests)
        return {}; // Exiting deferred until requests complete

    g_pCompositor->stopCompositor();
    return {};
}

SDispatchResult CKeybindManager::moveCurrentWorkspaceToMonitor(std::string args) {
    PHLMONITOR PMONITOR = g_pCompositor->getMonitorFromString(args);

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveCurrentWorkspaceToMonitor: monitor doesnt exist");
        return {.success = false, .error = "Ignoring moveCurrentWorkspaceToMonitor: monitor doesnt exist"};
    }

    // get the current workspace
    const auto PCURRENTWORKSPACE = g_pCompositor->m_lastMonitor->activeWorkspace;
    if (!PCURRENTWORKSPACE) {
        Debug::log(ERR, "moveCurrentWorkspaceToMonitor invalid workspace!");
        return {.success = false, .error = "moveCurrentWorkspaceToMonitor invalid workspace!"};
    }

    g_pCompositor->moveWorkspaceToMonitor(PCURRENTWORKSPACE, PMONITOR);

    return {};
}

SDispatchResult CKeybindManager::moveWorkspaceToMonitor(std::string args) {
    if (!args.contains(' '))
        return {.success = false, .error = "Invalid arguments, expected: workspace monitor"};

    std::string workspace = args.substr(0, args.find_first_of(' '));
    std::string monitor   = args.substr(args.find_first_of(' ') + 1);

    const auto  PMONITOR = g_pCompositor->getMonitorFromString(monitor);

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveWorkspaceToMonitor: monitor doesnt exist");
        return {.success = false, .error = "Ignoring moveWorkspaceToMonitor: monitor doesnt exist"};
    }

    const auto WORKSPACEID = getWorkspaceIDNameFromString(workspace).id;

    if (WORKSPACEID == WORKSPACE_INVALID) {
        Debug::log(ERR, "moveWorkspaceToMonitor invalid workspace!");
        return {.success = false, .error = "moveWorkspaceToMonitor invalid workspace!"};
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    if (!PWORKSPACE) {
        Debug::log(ERR, "moveWorkspaceToMonitor workspace doesn't exist!");
        return {.success = false, .error = "moveWorkspaceToMonitor workspace doesn't exist!"};
    }

    g_pCompositor->moveWorkspaceToMonitor(PWORKSPACE, PMONITOR);

    return {};
}

SDispatchResult CKeybindManager::focusWorkspaceOnCurrentMonitor(std::string args) {
    auto [workspaceID, workspaceName] = getWorkspaceIDNameFromString(args);
    if (workspaceID == WORKSPACE_INVALID) {
        Debug::log(ERR, "focusWorkspaceOnCurrentMonitor invalid workspace!");
        return {.success = false, .error = "focusWorkspaceOnCurrentMonitor invalid workspace!"};
    }

    const auto PCURRMONITOR = g_pCompositor->m_lastMonitor.lock();

    if (!PCURRMONITOR) {
        Debug::log(ERR, "focusWorkspaceOnCurrentMonitor monitor doesn't exist!");
        return {.success = false, .error = "focusWorkspaceOnCurrentMonitor monitor doesn't exist!"};
    }

    auto pWorkspace = g_pCompositor->getWorkspaceByID(workspaceID);

    if (!pWorkspace) {
        pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, PCURRMONITOR->ID, workspaceName);
        // we can skip the moving, since it's already on the current monitor
        changeworkspace(pWorkspace->getConfigName());
        return {};
    }

    static auto PBACKANDFORTH = CConfigValue<Hyprlang::INT>("binds:workspace_back_and_forth");
    const auto  PREVWS        = pWorkspace->getPrevWorkspaceIDName();

    if (*PBACKANDFORTH && PCURRMONITOR->activeWorkspaceID() == workspaceID && PREVWS.id != -1) {
        // Workspace to focus is previous workspace
        pWorkspace = g_pCompositor->getWorkspaceByID(PREVWS.id);
        if (!pWorkspace)
            pWorkspace = g_pCompositor->createNewWorkspace(PREVWS.id, PCURRMONITOR->ID, PREVWS.name);

        workspaceID = pWorkspace->m_id;
    }

    if (pWorkspace->m_monitor != PCURRMONITOR) {
        const auto POLDMONITOR = pWorkspace->m_monitor.lock();
        if (!POLDMONITOR) { // wat
            Debug::log(ERR, "focusWorkspaceOnCurrentMonitor old monitor doesn't exist!");
            return {.success = false, .error = "focusWorkspaceOnCurrentMonitor old monitor doesn't exist!"};
        }
        if (POLDMONITOR->activeWorkspaceID() == workspaceID) {
            g_pCompositor->swapActiveWorkspaces(POLDMONITOR, PCURRMONITOR);
            return {};
        } else {
            g_pCompositor->moveWorkspaceToMonitor(pWorkspace, PCURRMONITOR, true);
        }
    }

    changeworkspace(pWorkspace->getConfigName());

    return {};
}

SDispatchResult CKeybindManager::toggleSpecialWorkspace(std::string args) {
    const auto& [workspaceID, workspaceName] = getWorkspaceIDNameFromString("special:" + args);
    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID)) {
        Debug::log(ERR, "Invalid workspace passed to special");
        return {.success = false, .error = "Invalid workspace passed to special"};
    }

    bool       requestedWorkspaceIsAlreadyOpen = false;
    const auto PMONITOR                        = g_pCompositor->m_lastMonitor;
    auto       specialOpenOnMonitor            = PMONITOR->activeSpecialWorkspaceID();

    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->activeSpecialWorkspaceID() == workspaceID) {
            requestedWorkspaceIsAlreadyOpen = true;
            break;
        }
    }

    updateRelativeCursorCoords();

    PHLWORKSPACEREF focusedWorkspace;

    if (requestedWorkspaceIsAlreadyOpen && specialOpenOnMonitor == workspaceID) {
        // already open on this monitor
        Debug::log(LOG, "Toggling special workspace {} to closed", workspaceID);
        PMONITOR->setSpecialWorkspace(nullptr);

        focusedWorkspace = PMONITOR->activeWorkspace;
    } else {
        Debug::log(LOG, "Toggling special workspace {} to open", workspaceID);
        auto PSPECIALWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceID);

        if (!PSPECIALWORKSPACE)
            PSPECIALWORKSPACE = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->ID, workspaceName);

        PMONITOR->setSpecialWorkspace(PSPECIALWORKSPACE);

        focusedWorkspace = PSPECIALWORKSPACE;
    }

    const static auto PWARPONTOGGLESPECIAL = CConfigValue<Hyprlang::INT>("cursor:warp_on_toggle_special");

    if (*PWARPONTOGGLESPECIAL > 0) {
        auto PLAST     = focusedWorkspace->getLastFocusedWindow();
        auto HLSurface = CWLSurface::fromResource(g_pSeatManager->state.pointerFocus.lock());

        if (PLAST && (!HLSurface || HLSurface->getWindow()))
            PLAST->warpCursor(*PWARPONTOGGLESPECIAL == 2);
    }

    return {};
}

SDispatchResult CKeybindManager::forceRendererReload(std::string args) {
    bool overAgain = false;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!m->output)
            continue;

        auto rule = g_pConfigManager->getMonitorRuleFor(m);
        if (!m->applyMonitorRule(&rule, true)) {
            overAgain = true;
            break;
        }
    }

    if (overAgain)
        forceRendererReload(args);

    return {};
}

SDispatchResult CKeybindManager::resizeActive(std::string args) {
    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window found"};

    if (PLASTWINDOW->isFullscreen())
        return {.success = false, .error = "Window is fullscreen"};

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(args, PLASTWINDOW->m_realSize->goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return {.success = false, .error = "Invalid size provided"};

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PLASTWINDOW->m_realSize->goal());

    if (PLASTWINDOW->m_realSize->goal().x > 1 && PLASTWINDOW->m_realSize->goal().y > 1)
        PLASTWINDOW->setHidden(false);

    return {};
}

SDispatchResult CKeybindManager::moveActive(std::string args) {
    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window found"};

    if (PLASTWINDOW->isFullscreen())
        return {.success = false, .error = "Window is fullscreen"};

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(args, PLASTWINDOW->m_realPosition->goal());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - PLASTWINDOW->m_realPosition->goal());

    return {};
}

SDispatchResult CKeybindManager::moveWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "moveWindow: no window");
        return {.success = false, .error = "moveWindow: no window"};
    }

    if (PWINDOW->isFullscreen())
        return {.success = false, .error = "Window is fullscreen"};

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_realPosition->goal());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - PWINDOW->m_realPosition->goal(), PWINDOW);

    return {};
}

SDispatchResult CKeybindManager::resizeWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "resizeWindow: no window");
        return {.success = false, .error = "resizeWindow: no window"};
    }

    if (PWINDOW->isFullscreen())
        return {.success = false, .error = "Window is fullscreen"};

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_realSize->goal());

    if (SIZ.x < 1 || SIZ.y < 1)
        return {.success = false, .error = "Invalid size provided"};

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PWINDOW->m_realSize->goal(), CORNER_NONE, PWINDOW);

    if (PWINDOW->m_realSize->goal().x > 1 && PWINDOW->m_realSize->goal().y > 1)
        PWINDOW->setHidden(false);

    return {};
}

SDispatchResult CKeybindManager::circleNext(std::string arg) {
    if (g_pCompositor->m_lastWindow.expired()) {
        // if we have a clear focus, find the first window and get the next focusable.
        const auto PWS = g_pCompositor->m_lastMonitor->activeWorkspace;
        if (PWS && PWS->getWindows() > 0) {
            const auto PWINDOW = PWS->getFirstWindow();
            switchToWindow(PWINDOW);
        }

        return {};
    }

    CVarList            args{arg, 0, 's', true};

    std::optional<bool> floatStatus = {};
    if (args.contains("tile") || args.contains("tiled"))
        floatStatus = false;
    else if (args.contains("float") || args.contains("floating"))
        floatStatus = true;

    const auto  VISIBLE = args.contains("visible") || args.contains("v");
    const auto  PREV    = args.contains("prev") || args.contains("p") || args.contains("last") || args.contains("l");
    const auto  NEXT    = args.contains("next") || args.contains("n"); // prev is default in classic alt+tab
    const auto  HIST    = args.contains("hist") || args.contains("h");
    const auto& w       = HIST ? g_pCompositor->getWindowCycleHist(g_pCompositor->m_lastWindow, true, floatStatus, VISIBLE, NEXT) :
                                 g_pCompositor->getWindowCycle(g_pCompositor->m_lastWindow.lock(), true, floatStatus, VISIBLE, PREV);

    switchToWindow(w, HIST);

    return {};
}

SDispatchResult CKeybindManager::focusWindow(std::string regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW)
        return {.success = false, .error = "No such window found"};

    Debug::log(LOG, "Focusing to window name: {}", PWINDOW->m_title);

    const auto PWORKSPACE = PWINDOW->m_workspace;
    if (!PWORKSPACE) {
        Debug::log(ERR, "BUG THIS: null workspace in focusWindow");
        return {.success = false, .error = "BUG THIS: null workspace in focusWindow"};
    }

    updateRelativeCursorCoords();

    if (g_pCompositor->m_lastMonitor && g_pCompositor->m_lastMonitor->activeWorkspace != PWINDOW->m_workspace &&
        g_pCompositor->m_lastMonitor->activeSpecialWorkspace != PWINDOW->m_workspace) {
        Debug::log(LOG, "Fake executing workspace to move focus");
        changeworkspace(PWORKSPACE->getConfigName());
    }

    if (PWORKSPACE->m_hasFullscreenWindow) {
        const auto FSWINDOW = PWORKSPACE->getFullscreenWindow();
        const auto FSMODE   = PWORKSPACE->m_fullscreenMode;

        if (PWINDOW->m_isFloating) {
            // don't make floating implicitly fs
            if (!PWINDOW->m_createdOverFullscreen) {
                g_pCompositor->changeWindowZOrder(PWINDOW, true);
                g_pCompositor->updateFullscreenFadeOnWorkspace(PWORKSPACE);
            }

            g_pCompositor->focusWindow(PWINDOW);
        } else {
            if (FSWINDOW != PWINDOW && !PWINDOW->m_pinned)
                g_pCompositor->setWindowFullscreenClient(FSWINDOW, FSMODE_NONE);

            g_pCompositor->focusWindow(PWINDOW);

            if (FSWINDOW != PWINDOW && !PWINDOW->m_pinned)
                g_pCompositor->setWindowFullscreenClient(PWINDOW, FSMODE);

            // warp the position + size animation, otherwise it looks weird.
            PWINDOW->m_realPosition->warp();
            PWINDOW->m_realSize->warp();
        }
    } else
        g_pCompositor->focusWindow(PWINDOW);

    PWINDOW->warpCursor();

    return {};
}

SDispatchResult CKeybindManager::tagWindow(std::string args) {
    PHLWINDOW PWINDOW = nullptr;
    CVarList  vars{args, 0, 's', true};

    if (vars.size() == 1)
        PWINDOW = g_pCompositor->m_lastWindow.lock();
    else if (vars.size() == 2)
        PWINDOW = g_pCompositor->getWindowByRegex(vars[1]);
    else
        return {.success = false, .error = "Invalid number of arguments, expected 1 or 2 arguments"};

    if (PWINDOW && PWINDOW->m_tags.applyTag(vars[0])) {
        PWINDOW->updateDynamicRules();
        g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW->m_self.lock());
    }

    return {};
}

SDispatchResult CKeybindManager::toggleSwallow(std::string args) {
    PHLWINDOWREF pWindow = g_pCompositor->m_lastWindow;

    if (!valid(pWindow) || !valid(pWindow->m_swallowed))
        return {};

    if (pWindow->m_swallowed->m_currentlySwallowed) {
        // Unswallow
        pWindow->m_swallowed->m_currentlySwallowed = false;
        pWindow->m_swallowed->setHidden(false);
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(pWindow->m_swallowed.lock());
    } else {
        // Reswallow
        pWindow->m_swallowed->m_currentlySwallowed = true;
        pWindow->m_swallowed->setHidden(true);
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow->m_swallowed.lock());
    }

    return {};
}

SDispatchResult CKeybindManager::setSubmap(std::string submap) {
    if (submap == "reset" || submap == "") {
        m_szCurrentSelectedSubmap = "";
        Debug::log(LOG, "Reset active submap to the default one.");
        g_pEventManager->postEvent(SHyprIPCEvent{"submap", ""});
        EMIT_HOOK_EVENT("submap", m_szCurrentSelectedSubmap);
        return {};
    }

    for (const auto& k : g_pKeybindManager->m_vKeybinds) {
        if (k->submap == submap) {
            m_szCurrentSelectedSubmap = submap;
            Debug::log(LOG, "Changed keybind submap to {}", submap);
            g_pEventManager->postEvent(SHyprIPCEvent{"submap", submap});
            EMIT_HOOK_EVENT("submap", m_szCurrentSelectedSubmap);
            return {};
        }
    }

    Debug::log(ERR, "Cannot set submap {}, submap doesn't exist (wasn't registered!)", submap);
    return {.success = false, .error = std::format("Cannot set submap {}, submap doesn't exist (wasn't registered!)", submap)};
}

SDispatchResult CKeybindManager::pass(std::string regexp) {

    // find the first window passing the regex
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW) {
        Debug::log(ERR, "pass: window not found");
        return {.success = false, .error = "pass: window not found"};
    }

    if (!g_pSeatManager->keyboard) {
        Debug::log(ERR, "No kb in pass?");
        return {.success = false, .error = "No kb in pass?"};
    }

    const auto XWTOXW        = PWINDOW->m_isX11 && g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_isX11;
    const auto LASTMOUSESURF = g_pSeatManager->state.pointerFocus.lock();
    const auto LASTKBSURF    = g_pSeatManager->state.keyboardFocus.lock();

    // pass all mf shit
    if (!XWTOXW) {
        if (g_pKeybindManager->m_uLastCode != 0)
            g_pSeatManager->setKeyboardFocus(PWINDOW->m_wlSurface->resource());
        else
            g_pSeatManager->setPointerFocus(PWINDOW->m_wlSurface->resource(), {1, 1});
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
        return {};

    // Massive hack:
    // this will make g_pSeatManager NOT send the leave event to XWayland apps, provided we are not on an XWayland window already.
    // please kill me
    if (PWINDOW->m_isX11) {
        if (g_pKeybindManager->m_uLastCode != 0) {
            g_pSeatManager->state.keyboardFocus.reset();
            g_pSeatManager->state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->state.pointerFocus.reset();
            g_pSeatManager->state.pointerFocusResource.reset();
        }
    }

    const auto SL = PWINDOW->m_realPosition->goal() - g_pInputManager->getMouseCoordsInternal();

    if (g_pKeybindManager->m_uLastCode != 0)
        g_pSeatManager->setKeyboardFocus(LASTKBSURF);
    else
        g_pSeatManager->setPointerFocus(LASTMOUSESURF, SL);

    return {};
}

SDispatchResult CKeybindManager::sendshortcut(std::string args) {
    // args=<NEW_MODKEYS><NEW_KEY>[,WINDOW_RULES]
    const auto ARGS = CVarList(args, 3);
    if (ARGS.size() != 3) {
        Debug::log(ERR, "sendshortcut: invalid args");
        return {.success = false, .error = "sendshortcut: invalid args"};
    }

    const auto MOD     = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto KEY     = ARGS[1];
    uint32_t   keycode = 0;
    bool       isMouse = false;

    // similar to parseKey in ConfigManager
    if (isNumber(KEY) && std::stoi(KEY) > 9)
        keycode = std::stoi(KEY);
    else if (KEY.compare(0, 5, "code:") == 0 && isNumber(KEY.substr(5)))
        keycode = std::stoi(KEY.substr(5));
    else if (KEY.compare(0, 6, "mouse:") == 0 && isNumber(KEY.substr(6))) {
        keycode = std::stoi(KEY.substr(6));
        isMouse = true;
        if (keycode < 272) {
            Debug::log(ERR, "sendshortcut: invalid mouse button");
            return {.success = false, .error = "sendshortcut: invalid mouse button"};
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
            return {.success = false, .error = "sendshortcut: no kb"};
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
                return {.success = false, .error = "sendshortcut: key not found"};
            }

        } else
            keycode = g_pKeybindManager->m_mKeyToCodeCache[KEYPAIRSTRING];
    }

    if (!keycode) {
        Debug::log(ERR, "sendshortcut: invalid key");
        return {.success = false, .error = "sendshortcut: invalid key"};
    }

    const std::string regexp      = ARGS[2];
    PHLWINDOW         PWINDOW     = nullptr;
    const auto        LASTSURFACE = g_pCompositor->m_lastFocus.lock();

    //if regexp is not empty, send shortcut to current window
    //else, dont change focus
    if (regexp != "") {
        PWINDOW = g_pCompositor->getWindowByRegex(regexp);

        if (!PWINDOW) {
            Debug::log(ERR, "sendshortcut: window not found");
            return {.success = false, .error = "sendshortcut: window not found"};
        }

        if (!g_pSeatManager->keyboard) {
            Debug::log(ERR, "No kb in sendshortcut?");
            return {.success = false, .error = "No kb in sendshortcut?"};
        }

        if (!isMouse)
            g_pSeatManager->setKeyboardFocus(PWINDOW->m_wlSurface->resource());
        else
            g_pSeatManager->setPointerFocus(PWINDOW->m_wlSurface->resource(), {1, 1});
    }

    //copied the rest from pass and modified it
    // if wl -> xwl, activate destination
    if (PWINDOW && PWINDOW->m_isX11 && g_pCompositor->m_lastWindow && !g_pCompositor->m_lastWindow->m_isX11)
        g_pXWaylandManager->activateSurface(PWINDOW->m_wlSurface->resource(), true);
    // if xwl -> xwl, send to current. Timing issues make this not work.
    if (PWINDOW && PWINDOW->m_isX11 && g_pCompositor->m_lastWindow && g_pCompositor->m_lastWindow->m_isX11)
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

    g_pSeatManager->sendKeyboardMods(0, 0, 0, 0);

    if (!PWINDOW)
        return {};

    if (PWINDOW->m_isX11) { //xwayland hack, see pass
        if (!isMouse) {
            g_pSeatManager->state.keyboardFocus.reset();
            g_pSeatManager->state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->state.pointerFocus.reset();
            g_pSeatManager->state.pointerFocusResource.reset();
        }
    }

    const auto SL = PWINDOW->m_realPosition->goal() - g_pInputManager->getMouseCoordsInternal();

    if (!isMouse)
        g_pSeatManager->setKeyboardFocus(LASTSURFACE);
    else
        g_pSeatManager->setPointerFocus(LASTSURFACE, SL);

    return {};
}

SDispatchResult CKeybindManager::layoutmsg(std::string msg) {
    SLayoutMessageHeader hd = {g_pCompositor->m_lastWindow.lock()};
    g_pLayoutManager->getCurrentLayout()->layoutMessage(hd, msg);

    return {};
}

SDispatchResult CKeybindManager::dpms(std::string arg) {
    SDispatchResult res;
    bool            enable = arg.starts_with("on");
    std::string     port   = "";

    bool            isToggle = arg.starts_with("toggle");
    if (arg.find_first_of(' ') != std::string::npos)
        port = arg.substr(arg.find_first_of(' ') + 1);

    for (auto const& m : g_pCompositor->m_monitors) {

        if (!port.empty() && m->szName != port)
            continue;

        if (isToggle)
            enable = !m->dpmsStatus;

        m->output->state->resetExplicitFences();
        m->output->state->setEnabled(enable);

        m->dpmsStatus = enable;

        if (!m->state.commit()) {
            Debug::log(ERR, "Couldn't commit output {}", m->szName);
            res.success = false;
            res.error   = "Couldn't commit output {}";
        }

        if (enable)
            g_pHyprRenderer->damageMonitor(m);

        m->events.dpmsChanged.emit();
    }

    g_pCompositor->m_dpmsStateOn = enable;

    g_pPointerManager->recheckEnteredOutputs();

    return res;
}

SDispatchResult CKeybindManager::swapnext(std::string arg) {

    PHLWINDOW toSwap = nullptr;

    if (g_pCompositor->m_lastWindow.expired())
        return {};

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    const auto PLASTCYCLED =
        validMapped(g_pCompositor->m_lastWindow->m_lastCycledWindow) && g_pCompositor->m_lastWindow->m_lastCycledWindow->m_workspace == PLASTWINDOW->m_workspace ?
        g_pCompositor->m_lastWindow->m_lastCycledWindow.lock() :
        nullptr;

    const bool NEED_PREV = arg == "last" || arg == "l" || arg == "prev" || arg == "p";
    toSwap               = g_pCompositor->getWindowCycle(PLASTCYCLED ? PLASTCYCLED : PLASTWINDOW, true, std::nullopt, false, NEED_PREV);

    // sometimes we may come back to ourselves.
    if (toSwap == PLASTWINDOW)
        toSwap = g_pCompositor->getWindowCycle(PLASTWINDOW, true, std::nullopt, false, NEED_PREV);

    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, toSwap);

    PLASTWINDOW->m_lastCycledWindow = toSwap;

    g_pCompositor->focusWindow(PLASTWINDOW);

    return {};
}

SDispatchResult CKeybindManager::swapActiveWorkspaces(std::string args) {
    const auto MON1 = args.substr(0, args.find_first_of(' '));
    const auto MON2 = args.substr(args.find_first_of(' ') + 1);

    const auto PMON1 = g_pCompositor->getMonitorFromString(MON1);
    const auto PMON2 = g_pCompositor->getMonitorFromString(MON2);

    if (!PMON1 || !PMON2)
        return {.success = false, .error = "No such monitor found"};

    if (PMON1 == PMON2)
        return {};

    g_pCompositor->swapActiveWorkspaces(PMON1, PMON2);

    return {};
}

SDispatchResult CKeybindManager::pinActive(std::string args) {

    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW) {
        Debug::log(ERR, "pin: window not found");
        return {.success = false, .error = "pin: window not found"};
    }

    if (!PWINDOW->m_isFloating || PWINDOW->isFullscreen())
        return {.success = false, .error = "Window does not qualify to be pinned"};

    PWINDOW->m_pinned = !PWINDOW->m_pinned;

    const auto PMONITOR = PWINDOW->m_monitor.lock();

    if (!PMONITOR) {
        Debug::log(ERR, "pin: monitor not found");
        return {.success = false, .error = "pin: window not found"};
    }

    PWINDOW->m_workspace = PMONITOR->activeWorkspace;

    PWINDOW->updateDynamicRules();
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);

    const auto PWORKSPACE = PWINDOW->m_workspace;

    PWORKSPACE->m_lastFocusedWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS);

    g_pEventManager->postEvent(SHyprIPCEvent{"pin", std::format("{:x},{}", (uintptr_t)PWINDOW.get(), (int)PWINDOW->m_pinned)});
    EMIT_HOOK_EVENT("pin", PWINDOW);

    return {};
}

SDispatchResult CKeybindManager::mouse(std::string args) {
    const auto ARGS    = CVarList(args.substr(1), 2, ' ');
    const auto PRESSED = args[0] == '1';

    if (!PRESSED) {
        return changeMouseBindMode(MBIND_INVALID);
    }

    if (ARGS[0] == "movewindow") {
        return changeMouseBindMode(MBIND_MOVE);
    } else {
        try {
            switch (std::stoi(ARGS[1])) {
                case 1: return changeMouseBindMode(MBIND_RESIZE_FORCE_RATIO); break;
                case 2: return changeMouseBindMode(MBIND_RESIZE_BLOCK_RATIO); break;
                default: return changeMouseBindMode(MBIND_RESIZE);
            }
        } catch (std::exception& e) { return changeMouseBindMode(MBIND_RESIZE); }
    }
}

SDispatchResult CKeybindManager::changeMouseBindMode(const eMouseBindMode MODE) {
    if (MODE != MBIND_INVALID) {
        if (!g_pInputManager->currentlyDraggedWindow.expired() || g_pInputManager->dragMode != MBIND_INVALID)
            return {};

        const auto      MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const PHLWINDOW PWINDOW     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

        if (!PWINDOW)
            return SDispatchResult{.passEvent = true};

        if (!PWINDOW->isFullscreen() && MODE == MBIND_MOVE)
            PWINDOW->checkInputOnDecos(INPUT_TYPE_DRAG_START, MOUSECOORDS);

        if (g_pInputManager->currentlyDraggedWindow.expired())
            g_pInputManager->currentlyDraggedWindow = PWINDOW;

        g_pInputManager->dragMode = MODE;

        g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();
    } else {
        if (g_pInputManager->currentlyDraggedWindow.expired() || g_pInputManager->dragMode == MBIND_INVALID)
            return {};

        g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
        g_pInputManager->dragMode = MODE;
    }

    return {};
}

SDispatchResult CKeybindManager::bringActiveToTop(std::string args) {
    if (g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_isFloating)
        g_pCompositor->changeWindowZOrder(g_pCompositor->m_lastWindow.lock(), true);

    return {};
}

SDispatchResult CKeybindManager::alterZOrder(std::string args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto POSITION    = args.substr(0, args.find_first_of(','));
    auto       PWINDOW     = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW && g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_isFloating)
        PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW) {
        Debug::log(ERR, "alterZOrder: no window");
        return {.success = false, .error = "alterZOrder: no window"};
    }

    if (POSITION == "top")
        g_pCompositor->changeWindowZOrder(PWINDOW, true);
    else if (POSITION == "bottom")
        g_pCompositor->changeWindowZOrder(PWINDOW, false);
    else {
        Debug::log(ERR, "alterZOrder: bad position: {}", POSITION);
        return {.success = false, .error = "alterZOrder: bad position: {}"};
    }

    g_pInputManager->simulateMouseMovement();

    return {};
}

SDispatchResult CKeybindManager::lockGroups(std::string args) {
    if (args == "lock" || args.empty() || args == "lockgroups")
        g_pKeybindManager->m_bGroupsLocked = true;
    else if (args == "toggle")
        g_pKeybindManager->m_bGroupsLocked = !g_pKeybindManager->m_bGroupsLocked;
    else
        g_pKeybindManager->m_bGroupsLocked = false;

    g_pEventManager->postEvent(SHyprIPCEvent{"lockgroups", g_pKeybindManager->m_bGroupsLocked ? "1" : "0"});
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    return {};
}

SDispatchResult CKeybindManager::lockActiveGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "No window found"};

    if (!PWINDOW->m_groupData.pNextWindow.lock())
        return {.success = false, .error = "Not a group"};

    const auto PHEAD = PWINDOW->getGroupHead();

    if (args == "lock")
        PHEAD->m_groupData.locked = true;
    else if (args == "toggle")
        PHEAD->m_groupData.locked = !PHEAD->m_groupData.locked;
    else
        PHEAD->m_groupData.locked = false;

    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);

    return {};
}

void CKeybindManager::moveWindowIntoGroup(PHLWINDOW pWindow, PHLWINDOW pWindowInDirection) {
    if (pWindow->m_groupData.deny)
        return;

    updateRelativeCursorCoords();

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow); // This removes groupped property!

    if (pWindow->m_monitor != pWindowInDirection->m_monitor) {
        pWindow->moveToWorkspace(pWindowInDirection->m_workspace);
        pWindow->m_monitor = pWindowInDirection->m_monitor;
    }

    static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
    (*USECURRPOS ? pWindowInDirection : pWindowInDirection->getGroupTail())->insertWindowToGroup(pWindow);

    pWindowInDirection->setGroupCurrent(pWindow);
    pWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pWindow);
    g_pCompositor->focusWindow(pWindow);
    pWindow->warpCursor();

    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pWindow));

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

    if (pWindow->m_groupData.pNextWindow.lock() == pWindow) {
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

SDispatchResult CKeybindManager::moveIntoGroup(std::string args) {
    char        arg = args[0];

    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked)
        return {};

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return {.success = false, .error = std::format("Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg)};
    }

    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW || PWINDOW->m_groupData.deny)
        return {};

    auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    if (!PWINDOWINDIR || !PWINDOWINDIR->m_groupData.pNextWindow.lock())
        return {};

    // Do not move window into locked group if binds:ignore_group_lock is false
    if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_groupData.locked || (PWINDOW->m_groupData.pNextWindow.lock() && PWINDOW->getGroupHead()->m_groupData.locked)))
        return {};

    moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);

    return {};
}

SDispatchResult CKeybindManager::moveOutOfGroup(std::string args) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked)
        return {.success = false, .error = "Groups locked"};

    PHLWINDOW PWINDOW = nullptr;

    if (args != "active" && args.length() > 1)
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    else
        PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PWINDOW)
        return {.success = false, .error = "No window found"};

    if (!PWINDOW->m_groupData.pNextWindow.lock())
        return {.success = false, .error = "Window not in a group"};

    moveWindowOutOfGroup(PWINDOW);

    return {};
}

SDispatchResult CKeybindManager::moveWindowOrGroup(std::string args) {
    char        arg = args[0];

    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return {.success = false, .error = std::format("Cannot move into group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", arg)};
    }

    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();
    if (!PWINDOW)
        return {.success = false, .error = "No window found"};

    if (PWINDOW->isFullscreen())
        return {};

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_bGroupsLocked) {
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
        return {};
    }

    const auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, arg);

    const bool ISWINDOWGROUP       = PWINDOW->m_groupData.pNextWindow;
    const bool ISWINDOWGROUPLOCKED = ISWINDOWGROUP && PWINDOW->getGroupHead()->m_groupData.locked;
    const bool ISWINDOWGROUPSINGLE = ISWINDOWGROUP && PWINDOW->m_groupData.pNextWindow.lock() == PWINDOW;

    updateRelativeCursorCoords();

    // note: PWINDOWINDIR is not null implies !PWINDOW->m_isFloating
    if (PWINDOWINDIR && PWINDOWINDIR->m_groupData.pNextWindow) { // target is group
        if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->getGroupHead()->m_groupData.locked || ISWINDOWGROUPLOCKED || PWINDOW->m_groupData.deny)) {
            g_pLayoutManager->getCurrentLayout()->moveWindowTo(PWINDOW, args);
            PWINDOW->warpCursor();
        } else
            moveWindowIntoGroup(PWINDOW, PWINDOWINDIR);
    } else if (PWINDOWINDIR) { // target is regular window
        if ((!*PIGNOREGROUPLOCK && ISWINDOWGROUPLOCKED) || !ISWINDOWGROUP || (ISWINDOWGROUPSINGLE && PWINDOW->m_groupRules & GROUP_SET_ALWAYS)) {
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

    return {};
}

SDispatchResult CKeybindManager::setIgnoreGroupLock(std::string args) {
    static auto PIGNOREGROUPLOCK = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("binds:ignore_group_lock");

    if (args == "toggle")
        **PIGNOREGROUPLOCK = !**PIGNOREGROUPLOCK;
    else
        **PIGNOREGROUPLOCK = args == "on";

    g_pEventManager->postEvent(SHyprIPCEvent{"ignoregrouplock", std::to_string(**PIGNOREGROUPLOCK)});

    return {};
}

SDispatchResult CKeybindManager::denyWindowFromGroup(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();
    if (!PWINDOW || (PWINDOW && PWINDOW->m_groupData.pNextWindow.lock()))
        return {};

    if (args == "toggle")
        PWINDOW->m_groupData.deny = !PWINDOW->m_groupData.deny;
    else
        PWINDOW->m_groupData.deny = args == "on";

    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);

    return {};
}

SDispatchResult CKeybindManager::global(std::string args) {
    const auto APPID = args.substr(0, args.find_first_of(':'));
    const auto NAME  = args.substr(args.find_first_of(':') + 1);

    if (NAME.empty())
        return {};

    if (!PROTO::globalShortcuts->isTaken(APPID, NAME))
        return {};

    PROTO::globalShortcuts->sendGlobalShortcutEvent(APPID, NAME, g_pKeybindManager->m_iPassPressed);

    return {};
}

SDispatchResult CKeybindManager::moveGroupWindow(std::string args) {
    const auto BACK = args == "b" || args == "prev";

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window found"};

    if (!PLASTWINDOW->m_groupData.pNextWindow.lock())
        return {.success = false, .error = "Window not in a group"};

    if ((!BACK && PLASTWINDOW->m_groupData.pNextWindow->m_groupData.head) || (BACK && PLASTWINDOW->m_groupData.head)) {
        std::swap(PLASTWINDOW->m_groupData.head, PLASTWINDOW->m_groupData.pNextWindow->m_groupData.head);
        std::swap(PLASTWINDOW->m_groupData.locked, PLASTWINDOW->m_groupData.pNextWindow->m_groupData.locked);
    } else
        PLASTWINDOW->switchWithWindowInGroup(BACK ? PLASTWINDOW->getGroupPrevious() : PLASTWINDOW->m_groupData.pNextWindow.lock());

    PLASTWINDOW->updateWindowDecos();

    return {};
}

SDispatchResult CKeybindManager::event(std::string args) {
    g_pEventManager->postEvent(SHyprIPCEvent{"custom", args});
    return {};
}

#include <utility>
#include <type_traits>

SDispatchResult CKeybindManager::setProp(std::string args) {
    CVarList vars(args, 3, ' ');

    if (vars.size() < 3)
        return {.success = false, .error = "Not enough args"};

    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();
    const auto PWINDOW     = g_pCompositor->getWindowByRegex(vars[0]);

    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    const auto PROP = vars[1];
    const auto VAL  = vars[2];

    bool       noFocus = PWINDOW->m_windowData.noFocus.valueOrDefault();

    try {
        if (PROP == "animationstyle") {
            PWINDOW->m_windowData.animationStyle = CWindowOverridableVar(VAL, PRIORITY_SET_PROP);
        } else if (PROP == "maxsize") {
            PWINDOW->m_windowData.maxSize = CWindowOverridableVar(configStringToVector2D(VAL), PRIORITY_SET_PROP);
            PWINDOW->clampWindowSize(std::nullopt, PWINDOW->m_windowData.maxSize.value());
            PWINDOW->setHidden(false);
        } else if (PROP == "minsize") {
            PWINDOW->m_windowData.minSize = CWindowOverridableVar(configStringToVector2D(VAL), PRIORITY_SET_PROP);
            PWINDOW->clampWindowSize(PWINDOW->m_windowData.minSize.value(), std::nullopt);
            PWINDOW->setHidden(false);
        } else if (PROP == "alpha") {
            PWINDOW->m_windowData.alpha = CWindowOverridableVar(SAlphaValue{std::stof(VAL), PWINDOW->m_windowData.alpha.valueOrDefault().override}, PRIORITY_SET_PROP);
        } else if (PROP == "alphainactive") {
            PWINDOW->m_windowData.alphaInactive =
                CWindowOverridableVar(SAlphaValue{std::stof(VAL), PWINDOW->m_windowData.alphaInactive.valueOrDefault().override}, PRIORITY_SET_PROP);
        } else if (PROP == "alphafullscreen") {
            PWINDOW->m_windowData.alphaFullscreen =
                CWindowOverridableVar(SAlphaValue{std::stof(VAL), PWINDOW->m_windowData.alphaFullscreen.valueOrDefault().override}, PRIORITY_SET_PROP);
        } else if (PROP == "alphaoverride") {
            PWINDOW->m_windowData.alpha =
                CWindowOverridableVar(SAlphaValue{PWINDOW->m_windowData.alpha.valueOrDefault().alpha, (bool)configStringToInt(VAL).value_or(0)}, PRIORITY_SET_PROP);
        } else if (PROP == "alphainactiveoverride") {
            PWINDOW->m_windowData.alphaInactive =
                CWindowOverridableVar(SAlphaValue{PWINDOW->m_windowData.alphaInactive.valueOrDefault().alpha, (bool)configStringToInt(VAL).value_or(0)}, PRIORITY_SET_PROP);
        } else if (PROP == "alphafullscreenoverride") {
            PWINDOW->m_windowData.alphaFullscreen =
                CWindowOverridableVar(SAlphaValue{PWINDOW->m_windowData.alphaFullscreen.valueOrDefault().alpha, (bool)configStringToInt(VAL).value_or(0)}, PRIORITY_SET_PROP);
        } else if (PROP == "activebordercolor" || PROP == "inactivebordercolor") {
            CGradientValueData colorData = {};
            if (vars.size() > 4) {
                for (int i = 3; i < static_cast<int>(vars.size()); ++i) {
                    const auto TOKEN = vars[i];
                    if (TOKEN.ends_with("deg"))
                        colorData.m_angle = std::stoi(TOKEN.substr(0, TOKEN.size() - 3)) * (PI / 180.0);
                    else
                        configStringToInt(TOKEN).and_then([&colorData](const auto& e) {
                            colorData.m_colors.push_back(e);
                            return std::invoke_result_t<decltype(::configStringToInt), const std::string&>(1);
                        });
                }
            } else if (VAL != "-1")
                configStringToInt(VAL).and_then([&colorData](const auto& e) {
                    colorData.m_colors.push_back(e);
                    return std::invoke_result_t<decltype(::configStringToInt), const std::string&>(1);
                });

            colorData.updateColorsOk();

            if (PROP == "activebordercolor")
                PWINDOW->m_windowData.activeBorderColor = CWindowOverridableVar(colorData, PRIORITY_SET_PROP);
            else
                PWINDOW->m_windowData.inactiveBorderColor = CWindowOverridableVar(colorData, PRIORITY_SET_PROP);
        } else if (auto search = NWindowProperties::boolWindowProperties.find(PROP); search != NWindowProperties::boolWindowProperties.end()) {
            auto pWindowDataElement = search->second(PWINDOW);
            if (VAL == "toggle")
                pWindowDataElement->increment(true, PRIORITY_SET_PROP);
            else if (VAL == "unset")
                pWindowDataElement->unset(PRIORITY_SET_PROP);
            else
                *pWindowDataElement = CWindowOverridableVar((bool)configStringToInt(VAL).value_or(0), PRIORITY_SET_PROP);
        } else if (auto search = NWindowProperties::intWindowProperties.find(PROP); search != NWindowProperties::intWindowProperties.end()) {
            if (VAL == "unset")
                search->second(PWINDOW)->unset(PRIORITY_SET_PROP);
            else if (VAL.starts_with("relative")) {
                const Hyprlang::INT V = std::stoi(VAL.substr(VAL.find(' ')));
                search->second(PWINDOW)->increment(V, PRIORITY_SET_PROP);
            } else if (const auto V = configStringToInt(VAL); V)
                *(search->second(PWINDOW)) = CWindowOverridableVar((Hyprlang::INT)*V, PRIORITY_SET_PROP);
        } else if (auto search = NWindowProperties::floatWindowProperties.find(PROP); search != NWindowProperties::floatWindowProperties.end()) {
            if (VAL == "unset")
                search->second(PWINDOW)->unset(PRIORITY_SET_PROP);
            else if (VAL.starts_with("relative")) {
                const auto V = std::stof(VAL.substr(VAL.find(' ')));
                search->second(PWINDOW)->increment(V, PRIORITY_SET_PROP);
            } else {
                const auto V               = std::stof(VAL);
                *(search->second(PWINDOW)) = CWindowOverridableVar(V, PRIORITY_SET_PROP);
            }
        } else
            return {.success = false, .error = "Prop not found"};
    } catch (std::exception& e) { return {.success = false, .error = std::format("Error parsing prop value: {}", std::string(e.what()))}; }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (!(PWINDOW->m_windowData.noFocus.valueOrDefault() == noFocus)) {
        g_pCompositor->focusWindow(nullptr);
        g_pCompositor->focusWindow(PWINDOW);
        g_pCompositor->focusWindow(PLASTWINDOW);
    }

    for (auto const& m : g_pCompositor->m_monitors)
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

    return {};
}

SDispatchResult CKeybindManager::sendkeystate(std::string args) {
    // args=<NEW_MODKEYS><NEW_KEY><STATE>[,WINDOW_RULES]
    const auto ARGS = CVarList(args, 4);
    if (ARGS.size() != 4) {
        Debug::log(ERR, "sendkeystate: invalid args");
        return {.success = false, .error = "sendkeystate: invalid args"};
    }

    const auto STATE = ARGS[2];

    if (STATE != "down" && STATE != "repeat" && STATE != "up") {
        Debug::log(ERR, "sendkeystate: invalid state, must be 'down', 'repeat', or 'up'");
        return {.success = false, .error = "sendkeystate: invalid state, must be 'down', 'repeat', or 'up'"};
    }

    std::string modifiedArgs = ARGS[0] + "," + ARGS[1] + "," + ARGS[3];

    const int   oldPassPressed = g_pKeybindManager->m_iPassPressed;

    if (STATE == "down")
        g_pKeybindManager->m_iPassPressed = 1;
    else if (STATE == "up")
        g_pKeybindManager->m_iPassPressed = 0;
    else if (STATE == "repeat")
        g_pKeybindManager->m_iPassPressed = 1;

    auto result = sendshortcut(modifiedArgs);

    if (STATE == "repeat" && result.success)
        result = sendshortcut(modifiedArgs);

    g_pKeybindManager->m_iPassPressed = oldPassPressed;

    if (!result.success && !result.error.empty()) {
        size_t pos = result.error.find("sendshortcut:");
        if (pos != std::string::npos)
            result.error = "sendkeystate:" + result.error.substr(pos + 13);
    }

    return result;
}
