#pragma once

#include "../defines.hpp"
#include <deque>
#include <set>
#include "../Compositor.hpp"
#include <unordered_map>
#include <functional>
#include <xkbcommon/xkbcommon.h>
#include "../devices/IPointer.hpp"

class CInputManager;
class CConfigManager;
class CPluginSystem;
class IKeyboard;

struct SKeybind {
    std::string            key            = "";
    std::set<xkb_keysym_t> sMkKeys        = {};
    uint32_t               keycode        = 0;
    bool                   catchAll       = false;
    uint32_t               modmask        = 0;
    std::set<xkb_keysym_t> sMkMods        = {};
    std::string            handler        = "";
    std::string            arg            = "";
    bool                   locked         = false;
    std::string            submap         = "";
    std::string            description    = "";
    bool                   release        = false;
    bool                   repeat         = false;
    bool                   mouse          = false;
    bool                   nonConsuming   = false;
    bool                   transparent    = false;
    bool                   ignoreMods     = false;
    bool                   multiKey       = false;
    bool                   hasDescription = false;
    bool                   dontInhibit    = false;

    // DO NOT INITIALIZE
    bool shadowed = false;
};

enum eFocusWindowMode {
    MODE_CLASS_REGEX = 0,
    MODE_INITIAL_CLASS_REGEX,
    MODE_TITLE_REGEX,
    MODE_INITIAL_TITLE_REGEX,
    MODE_ADDRESS,
    MODE_PID,
    MODE_ACTIVE_WINDOW
};

struct SPressedKeyWithMods {
    std::string  keyName            = "";
    xkb_keysym_t keysym             = 0;
    uint32_t     keycode            = 0;
    uint32_t     modmaskAtPressTime = 0;
    bool         sent               = false;
    std::string  submapAtPress      = "";
};

struct SParsedKey {
    std::string key      = "";
    uint32_t    keycode  = 0;
    bool        catchAll = false;
};

enum eMultiKeyCase {
    MK_NO_MATCH = 0,
    MK_PARTIAL_MATCH,
    MK_FULL_MATCH
};

struct SDispatchResult {
    bool        passEvent = false;
    bool        success   = true;
    std::string error;
};

class CKeybindManager {
  public:
    CKeybindManager();
    ~CKeybindManager();

    bool                                                                         onKeyEvent(std::any, SP<IKeyboard>);
    bool                                                                         onAxisEvent(const IPointer::SAxisEvent&);
    bool                                                                         onMouseEvent(const IPointer::SButtonEvent&);
    void                                                                         resizeWithBorder(const IPointer::SButtonEvent&);
    void                                                                         onSwitchEvent(const std::string&);
    void                                                                         onSwitchOnEvent(const std::string&);
    void                                                                         onSwitchOffEvent(const std::string&);

    void                                                                         addKeybind(SKeybind);
    void                                                                         removeKeybind(uint32_t, const SParsedKey&);
    uint32_t                                                                     stringToModMask(std::string);
    uint32_t                                                                     keycodeToModifier(xkb_keycode_t);
    void                                                                         clearKeybinds();
    void                                                                         shadowKeybinds(const xkb_keysym_t& doesntHave = 0, const uint32_t doesntHaveCode = 0);
    std::string                                                                  getCurrentSubmap();

    std::unordered_map<std::string, std::function<SDispatchResult(std::string)>> m_mDispatchers;

    wl_event_source*                                                             m_pActiveKeybindEventSource = nullptr;

    bool                                                                         m_bGroupsLocked = false;

    std::list<SKeybind>                                                          m_lKeybinds;

    //since we cant find keycode through keyname in xkb:
    //on sendshortcut call, we once search for keyname (e.g. "g") the correct keycode (e.g. 42)
    //and cache it in this map to make sendshortcut calls faster
    //we also store the keyboard pointer (in the string) to differentiate between different keyboard (layouts)
    std::unordered_map<std::string, xkb_keycode_t> m_mKeyToCodeCache;

    static SDispatchResult                         changeMouseBindMode(const eMouseBindMode mode);

  private:
    std::deque<SPressedKeyWithMods> m_dPressedKeys;

    inline static std::string       m_szCurrentSelectedSubmap = "";

    SKeybind*                       m_pActiveKeybind = nullptr;

    uint32_t                        m_uTimeLastMs    = 0;
    uint32_t                        m_uLastCode      = 0;
    uint32_t                        m_uLastMouseCode = 0;

    std::vector<SKeybind*>          m_vPressedSpecialBinds;

    int                             m_iPassPressed = -1; // used for pass

    CTimer                          m_tScrollTimer;

    SDispatchResult                 handleKeybinds(const uint32_t, const SPressedKeyWithMods&, bool);

    std::set<xkb_keysym_t>          m_sMkKeys = {};
    std::set<xkb_keysym_t>          m_sMkMods = {};
    eMultiKeyCase                   mkBindMatches(const SKeybind);
    eMultiKeyCase                   mkKeysymSetMatches(const std::set<xkb_keysym_t>, const std::set<xkb_keysym_t>);

    bool                            handleInternalKeybinds(xkb_keysym_t);
    bool                            handleVT(xkb_keysym_t);

    xkb_state*                      m_pXKBTranslationState = nullptr;

    void                            updateXKBTranslationState();
    bool                            ensureMouseBindState();

    static bool                     tryMoveFocusToMonitor(CMonitor* monitor);
    static void                     moveWindowOutOfGroup(PHLWINDOW pWindow, const std::string& dir = "");
    static void                     moveWindowIntoGroup(PHLWINDOW pWindow, PHLWINDOW pWindowInDirection);
    static void                     switchToWindow(PHLWINDOW PWINDOWTOCHANGETO);
    static uint64_t                 spawnRawProc(std::string, PHLWORKSPACE pInitialWorkspace);
    static uint64_t                 spawnWithRules(std::string, PHLWORKSPACE pInitialWorkspace);

    // -------------- Dispatchers -------------- //
    static SDispatchResult killActive(std::string);
    static SDispatchResult kill(std::string);
    static SDispatchResult spawn(std::string);
    static SDispatchResult spawnRaw(std::string);
    static SDispatchResult toggleActiveFloating(std::string);
    static SDispatchResult toggleActivePseudo(std::string);
    static SDispatchResult setActiveFloating(std::string);
    static SDispatchResult setActiveTiled(std::string);
    static SDispatchResult changeworkspace(std::string);
    static SDispatchResult fullscreenActive(std::string);
    static SDispatchResult fullscreenStateActive(std::string args);
    static SDispatchResult moveActiveToWorkspace(std::string);
    static SDispatchResult moveActiveToWorkspaceSilent(std::string);
    static SDispatchResult moveFocusTo(std::string);
    static SDispatchResult focusUrgentOrLast(std::string);
    static SDispatchResult focusCurrentOrLast(std::string);
    static SDispatchResult centerWindow(std::string);
    static SDispatchResult moveActiveTo(std::string);
    static SDispatchResult swapActive(std::string);
    static SDispatchResult toggleGroup(std::string);
    static SDispatchResult changeGroupActive(std::string);
    static SDispatchResult alterSplitRatio(std::string);
    static SDispatchResult focusMonitor(std::string);
    static SDispatchResult toggleSplit(std::string);
    static SDispatchResult swapSplit(std::string);
    static SDispatchResult moveCursorToCorner(std::string);
    static SDispatchResult moveCursor(std::string);
    static SDispatchResult workspaceOpt(std::string);
    static SDispatchResult renameWorkspace(std::string);
    static SDispatchResult exitHyprland(std::string);
    static SDispatchResult moveCurrentWorkspaceToMonitor(std::string);
    static SDispatchResult moveWorkspaceToMonitor(std::string);
    static SDispatchResult focusWorkspaceOnCurrentMonitor(std::string);
    static SDispatchResult toggleSpecialWorkspace(std::string);
    static SDispatchResult forceRendererReload(std::string);
    static SDispatchResult resizeActive(std::string);
    static SDispatchResult moveActive(std::string);
    static SDispatchResult moveWindow(std::string);
    static SDispatchResult resizeWindow(std::string);
    static SDispatchResult circleNext(std::string);
    static SDispatchResult focusWindow(std::string);
    static SDispatchResult tagWindow(std::string);
    static SDispatchResult setSubmap(std::string);
    static SDispatchResult pass(std::string);
    static SDispatchResult sendshortcut(std::string);
    static SDispatchResult layoutmsg(std::string);
    static SDispatchResult dpms(std::string);
    static SDispatchResult swapnext(std::string);
    static SDispatchResult swapActiveWorkspaces(std::string);
    static SDispatchResult pinActive(std::string);
    static SDispatchResult mouse(std::string);
    static SDispatchResult bringActiveToTop(std::string);
    static SDispatchResult alterZOrder(std::string);
    static SDispatchResult lockGroups(std::string);
    static SDispatchResult lockActiveGroup(std::string);
    static SDispatchResult moveIntoGroup(std::string);
    static SDispatchResult moveOutOfGroup(std::string);
    static SDispatchResult moveGroupWindow(std::string);
    static SDispatchResult moveWindowOrGroup(std::string);
    static SDispatchResult setIgnoreGroupLock(std::string);
    static SDispatchResult denyWindowFromGroup(std::string);
    static SDispatchResult global(std::string);
    static SDispatchResult event(std::string);

    friend class CCompositor;
    friend class CInputManager;
    friend class CConfigManager;
    friend class CWorkspace;
    friend class CPointerManager;
};

inline std::unique_ptr<CKeybindManager> g_pKeybindManager;
