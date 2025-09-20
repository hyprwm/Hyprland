#pragma once

#include "IHID.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

#include <optional>
#include <xkbcommon/xkbcommon.h>
#include <hyprutils/os/FileDescriptor.hpp>

AQUAMARINE_FORWARD(IKeyboard);

enum eKeyboardModifiers {
    HL_MODIFIER_SHIFT = (1 << 0),
    HL_MODIFIER_CAPS  = (1 << 1),
    HL_MODIFIER_CTRL  = (1 << 2),
    HL_MODIFIER_ALT   = (1 << 3),
    HL_MODIFIER_MOD2  = (1 << 4),
    HL_MODIFIER_MOD3  = (1 << 5),
    HL_MODIFIER_META  = (1 << 6),
    HL_MODIFIER_MOD5  = (1 << 7),
};

class IKeyboard : public IHID {
  public:
    virtual ~IKeyboard();
    virtual uint32_t   getCapabilities();
    virtual eHIDType   getType();
    virtual bool       isVirtual() = 0;
    virtual wl_client* getClient() {
        return nullptr;
    };
    virtual SP<Aquamarine::IKeyboard> aq() = 0;

    struct SKeyEvent {
        uint32_t              timeMs     = 0;
        uint32_t              keycode    = 0;
        bool                  updateMods = false;
        wl_keyboard_key_state state      = WL_KEYBOARD_KEY_STATE_PRESSED;
    };

    struct SKeymapEvent {
        xkb_keymap* keymap = nullptr;
    };

    struct SModifiersEvent {
        uint32_t depressed = 0;
        uint32_t latched   = 0;
        uint32_t locked    = 0;
        uint32_t group     = 0;
    };

    struct {
        CSignalT<SKeyEvent>       key;
        CSignalT<SModifiersEvent> modifiers;
        CSignalT<SKeymapEvent>    keymap;
        CSignalT<>                repeatInfo;
    } m_keyboardEvents;

    struct SStringRuleNames {
        std::string layout  = "";
        std::string model   = "";
        std::string variant = "";
        std::string options = "";
        std::string rules   = "";
    };

    void                              setKeymap(const SStringRuleNames& rules);
    void                              updateXKBTranslationState(xkb_keymap* const keymap = nullptr);
    std::optional<xkb_layout_index_t> getActiveLayoutIndex();
    std::string                       getActiveLayout();
    std::optional<uint32_t>           getLEDs();
    void                              updateLEDs();
    void                              updateLEDs(uint32_t leds);
    uint32_t                          getModifiers();
    void                              updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    bool                              updateModifiersState(); // rets whether changed
    void                              updateXkbStateWithKey(uint32_t xkbKey, bool pressed);
    void                              updateKeymapFD();
    bool                              getPressed(uint32_t key);
    bool                              shareStates();
    void                              setShareStatesAuto(bool shareStates);

    bool                              m_active     = false;
    bool                              m_enabled    = true;
    bool                              m_allowBinds = true;

    // permission flag: whether this keyboard is allowed to be processed
    bool m_allowed = true;

    // if the keymap is overridden by the implementation,
    // don't try to set keyboard rules anymore, to avoid overwriting the requested one.
    // e.g. Virtual keyboards with custom maps.
    bool               m_keymapOverridden = false;

    xkb_layout_index_t m_activeLayout = 0;
    xkb_state*         m_xkbState     = nullptr;

    // never gets modifiers or layout changes sent, used for keybinds
    xkb_state*  m_xkbStaticState = nullptr;
    xkb_state*  m_xkbSymState    = nullptr; // same as static but gets layouts

    xkb_keymap* m_xkbKeymap = nullptr;

    struct {
        uint32_t depressed = 0, latched = 0, locked = 0, group = 0;
    } m_modifiersState;

    std::array<xkb_led_index_t, 3> m_ledIndexes = {XKB_MOD_INVALID};
    std::array<xkb_mod_index_t, 8> m_modIndexes = {XKB_MOD_INVALID};
    uint32_t                       m_leds       = 0;

    std::string                    m_xkbFilePath     = "";
    std::string                    m_xkbKeymapString = "";
    Hyprutils::OS::CFileDescriptor m_xkbKeymapFD;

    std::string                    m_xkbKeymapV1String = "";
    Hyprutils::OS::CFileDescriptor m_xkbKeymapV1FD;

    SStringRuleNames               m_currentRules;
    int                            m_repeatRate        = 0;
    int                            m_repeatDelay       = 0;
    int                            m_numlockOn         = -1;
    bool                           m_resolveBindsBySym = false;

    WP<IKeyboard>                  m_self;

  private:
    void                  clearManuallyAllocd();

    std::vector<uint32_t> m_pressed;

  protected:
    bool updatePressed(uint32_t key, bool pressed);
    bool m_shareStates     = true;
    bool m_shareStatesAuto = true;
};
