#pragma once

#include "IHID.hpp"
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

#include <xkbcommon/xkbcommon.h>

struct wlr_keyboard;

class IKeyboard : public IHID {
  public:
    virtual ~IKeyboard();
    virtual uint32_t      getCapabilities();
    virtual eHIDType      getType();
    virtual bool          isVirtual() = 0;
    virtual wlr_keyboard* wlr()       = 0;

    struct SKeyEvent {
        uint32_t              timeMs     = 0;
        uint32_t              keycode    = 0;
        bool                  updateMods = false;
        wl_keyboard_key_state state      = WL_KEYBOARD_KEY_STATE_PRESSED;
    };

    struct {
        CSignal key;
        CSignal modifiers;
        CSignal keymap;
        CSignal repeatInfo;
    } keyboardEvents;

    struct SStringRuleNames {
        std::string layout  = "";
        std::string model   = "";
        std::string variant = "";
        std::string options = "";
        std::string rules   = "";
    };

    void               updateXKBTranslationState(xkb_keymap* const keymap = nullptr);
    std::string        getActiveLayout();
    void               updateLEDs();
    void               updateLEDs(uint32_t leds);

    bool               active  = false;
    bool               enabled = true;

    xkb_layout_index_t activeLayout        = 0;
    xkb_state*         xkbTranslationState = nullptr;

    std::string        hlName      = "";
    std::string        xkbFilePath = "";

    SStringRuleNames   currentRules;
    int                repeatRate        = 0;
    int                repeatDelay       = 0;
    int                numlockOn         = -1;
    bool               resolveBindsBySym = false;

    WP<IKeyboard>      self;
};
