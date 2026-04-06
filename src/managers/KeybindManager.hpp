#pragma once

#include "../defines.hpp"
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <xkbcommon/xkbcommon.h>
#include "../devices/IPointer.hpp"
#include "eventLoop/EventLoopTimer.hpp"
#include "../helpers/time/Timer.hpp"

class CInputManager;
class CPluginSystem;
class IKeyboard;

enum eMouseBindMode : int8_t;

struct SSubmap {
    std::string name  = "";
    std::string reset = "";
    bool        operator==(const SSubmap& other) const {
        return name == other.name;
    }
};

struct SKeybind {
    std::string                     key             = "";
    std::vector<xkb_keysym_t>       sMkKeys         = {};
    uint32_t                        keycode         = 0;
    bool                            catchAll        = false;
    uint32_t                        modmask         = 0;
    std::vector<xkb_keysym_t>       sMkMods         = {};
    std::string                     handler         = "";
    std::string                     arg             = "";
    bool                            locked          = false;
    SSubmap                         submap          = {};
    std::string                     description     = "";
    bool                            release         = false;
    bool                            repeat          = false;
    bool                            longPress       = false;
    bool                            mouse           = false;
    bool                            nonConsuming    = false;
    bool                            transparent     = false;
    bool                            ignoreMods      = false;
    bool                            multiKey        = false;
    bool                            hasDescription  = false;
    bool                            dontInhibit     = false;
    bool                            click           = false;
    bool                            drag            = false;
    bool                            submapUniversal = false;
    bool                            deviceInclusive = false;
    std::unordered_set<std::string> devices         = {};

    std::string                     displayKey = "";

    // DO NOT INITIALIZE
    bool shadowed = false;
};

enum eFocusWindowMode : uint8_t {
    MODE_CLASS_REGEX = 0,
    MODE_INITIAL_CLASS_REGEX,
    MODE_TITLE_REGEX,
    MODE_INITIAL_TITLE_REGEX,
    MODE_TAG_REGEX,
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
    SSubmap      submapAtPress      = {};
    Vector2D     mousePosAtPress    = {};
};

struct SParsedKey {
    std::string key      = "";
    uint32_t    keycode  = 0;
    bool        catchAll = false;
};

enum eMultiKeyCase : uint8_t {
    MK_NO_MATCH = 0,
    MK_PARTIAL_MATCH,
    MK_FULL_MATCH
};

namespace Config::Legacy {
    class CConfigManager;
}

namespace Config::Lua {
    class CConfigManager;
}

class CKeybindManager {
  public:
    CKeybindManager();
    ~CKeybindManager();

    bool                                                                         onKeyEvent(std::any, SP<IKeyboard>);
    bool                                                                         onAxisEvent(const IPointer::SAxisEvent&, SP<IPointer>);
    bool                                                                         onMouseEvent(const IPointer::SButtonEvent&, SP<IPointer>);
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
    SSubmap                                                                      getCurrentSubmap();

    std::unordered_map<std::string, std::function<SDispatchResult(std::string)>> m_dispatchers;

    bool                                                                         m_groupsLocked = false;

    std::vector<SP<SKeybind>>                                                    m_keybinds;

    //since we can't find keycode through keyname in xkb:
    //on sendshortcut call, we once search for keyname (e.g. "g") the correct keycode (e.g. 42)
    //and cache it in this map to make sendshortcut calls faster
    //we also store the keyboard pointer (in the string) to differentiate between different keyboard (layouts)
    std::unordered_map<std::string, xkb_keycode_t> m_keyToCodeCache;

    static SDispatchResult                         changeMouseBindMode(const eMouseBindMode mode);

  private:
    std::vector<SPressedKeyWithMods> m_pressedKeys;

    std::vector<WP<SKeybind>>        m_activeKeybinds;
    WP<SKeybind>                     m_lastLongPressKeybind;

    SP<CEventLoopTimer>              m_longPressTimer;
    SP<CEventLoopTimer>              m_repeatKeyTimer;
    uint32_t                         m_repeatKeyRate = 50;

    std::vector<WP<SKeybind>>        m_pressedSpecialBinds;

    CTimer                           m_scrollTimer;

    SDispatchResult                  handleKeybinds(const uint32_t, const SPressedKeyWithMods&, bool, SP<IKeyboard>, SP<IHID>);

    std::set<xkb_keysym_t>           m_mkKeys = {};
    std::set<xkb_keysym_t>           m_mkMods = {};
    eMultiKeyCase                    mkBindMatches(const SP<SKeybind>);
    eMultiKeyCase                    mkKeysymSetMatches(const std::vector<xkb_keysym_t>, const std::set<xkb_keysym_t>);

    bool                             handleInternalKeybinds(xkb_keysym_t);
    bool                             handleVT(xkb_keysym_t);

    xkb_state*                       m_xkbTranslationState = nullptr;

    void                             updateXKBTranslationState();
    bool                             ensureMouseBindState();

    friend class CCompositor;
    friend class CInputManager;
    friend class Config::Legacy::CConfigManager;
    friend class Config::Lua::CConfigManager;
    friend class CWorkspace;
    friend class CPointerManager;
};

inline UP<CKeybindManager> g_pKeybindManager;
