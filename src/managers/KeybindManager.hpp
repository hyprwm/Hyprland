#pragma once

#include "../defines.hpp"
#include <deque>
#include "../Compositor.hpp"
#include <unordered_map>
#include <functional>

class CInputManager;
class CConfigManager;
class CPluginSystem;

struct SKeybind {
    std::string key     = "";
    int         keycode = -1;
    uint32_t    modmask = 0;
    std::string handler = "";
    std::string arg     = "";
    bool        locked  = false;
    std::string submap  = "";
    bool        release = false;
    bool        repeat  = false;
    bool        mouse   = false;

    // DO NOT INITIALIZE
    bool shadowed = false;
};

enum eFocusWindowMode
{
    MODE_CLASS_REGEX = 0,
    MODE_TITLE_REGEX,
    MODE_ADDRESS,
    MODE_PID
};

class CKeybindManager {
  public:
    CKeybindManager();

    bool                                                              onKeyEvent(wlr_keyboard_key_event*, SKeyboard*);
    bool                                                              onAxisEvent(wlr_pointer_axis_event*);
    bool                                                              onMouseEvent(wlr_pointer_button_event*);
    void                                                              resizeWithBorder(wlr_pointer_button_event*);
    void                                                              onSwitchEvent(const std::string&);
    void                                                              onSwitchOnEvent(const std::string&);
    void                                                              onSwitchOffEvent(const std::string&);

    void                                                              addKeybind(SKeybind);
    void                                                              removeKeybind(uint32_t, const std::string&);
    uint32_t                                                          stringToModMask(std::string);
    void                                                              clearKeybinds();
    void                                                              shadowKeybinds(const xkb_keysym_t& doesntHave = 0, const int& doesntHaveCode = 0);

    std::unordered_map<std::string, std::function<void(std::string)>> m_mDispatchers;

    wl_event_source*                                                  m_pActiveKeybindEventSource = nullptr;

    bool                                                              m_bGroupsLocked = false;

    std::list<SKeybind>                                               m_lKeybinds;

  private:
    std::deque<xkb_keysym_t>  m_dPressedKeysyms;
    std::deque<int>           m_dPressedKeycodes;

    inline static std::string m_szCurrentSelectedSubmap = "";

    xkb_keysym_t              m_kHeldBack = 0;

    SKeybind*                 m_pActiveKeybind = nullptr;

    uint32_t                  m_uTimeLastMs    = 0;
    uint32_t                  m_uLastCode      = 0;
    uint32_t                  m_uLastMouseCode = 0;

    bool                      m_bIsMouseBindActive = false;

    int                       m_iPassPressed = -1; // used for pass

    CTimer                    m_tScrollTimer;

    bool                      handleKeybinds(const uint32_t&, const std::string&, const xkb_keysym_t&, const int&, bool, uint32_t);

    bool                      handleInternalKeybinds(xkb_keysym_t);
    bool                      handleVT(xkb_keysym_t);

    xkb_state*                m_pXKBTranslationState = nullptr;

    void                      updateXKBTranslationState();
    bool                      ensureMouseBindState();

    // -------------- Dispatchers -------------- //
    static void     killActive(std::string);
    static void     kill(std::string);
    static void     spawn(std::string);
    static uint64_t spawnRaw(std::string);
    static void     toggleActiveFloating(std::string);
    static void     toggleActivePseudo(std::string);
    static void     changeworkspace(std::string);
    static void     fullscreenActive(std::string);
    static void     fakeFullscreenActive(std::string);
    static void     moveActiveToWorkspace(std::string);
    static void     moveActiveToWorkspaceSilent(std::string);
    static void     moveFocusTo(std::string);
    static void     focusUrgentOrLast(std::string);
    static void     focusCurrentOrLast(std::string);
    static void     centerWindow(std::string);
    static void     moveActiveTo(std::string);
    static void     toggleGroup(std::string);
    static void     changeGroupActive(std::string);
    static void     alterSplitRatio(std::string);
    static void     focusMonitor(std::string);
    static void     toggleSplit(std::string);
    static void     moveCursorToCorner(std::string);
    static void     workspaceOpt(std::string);
    static void     renameWorkspace(std::string);
    static void     exitHyprland(std::string);
    static void     moveCurrentWorkspaceToMonitor(std::string);
    static void     moveWorkspaceToMonitor(std::string);
    static void     toggleSpecialWorkspace(std::string);
    static void     forceRendererReload(std::string);
    static void     resizeActive(std::string);
    static void     moveActive(std::string);
    static void     moveWindow(std::string);
    static void     resizeWindow(std::string);
    static void     circleNext(std::string);
    static void     focusWindow(std::string);
    static void     setSubmap(std::string);
    static void     pass(std::string);
    static void     layoutmsg(std::string);
    static void     toggleOpaque(std::string);
    static void     dpms(std::string);
    static void     swapnext(std::string);
    static void     swapActiveWorkspaces(std::string);
    static void     pinActive(std::string);
    static void     mouse(std::string);
    static void     bringActiveToTop(std::string);
    static void     lockGroups(std::string);
    static void     moveIntoGroup(std::string);
    static void     moveOutOfGroup(std::string);
    static void     global(std::string);

    friend class CCompositor;
    friend class CInputManager;
    friend class CConfigManager;
};

inline std::unique_ptr<CKeybindManager> g_pKeybindManager;
