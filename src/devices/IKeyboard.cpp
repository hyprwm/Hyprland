#include "IKeyboard.hpp"
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../config/ConfigManager.hpp"
#include <sys/mman.h>
#include <aquamarine/input/Input.hpp>
#include <cstring>

#define LED_COUNT 3

constexpr static std::array<const char*, 8> MODNAMES = {
    XKB_MOD_NAME_SHIFT, XKB_MOD_NAME_CAPS, XKB_MOD_NAME_CTRL, XKB_MOD_NAME_ALT, XKB_MOD_NAME_NUM, "Mod3", XKB_MOD_NAME_LOGO, "Mod5",
};

constexpr static std::array<const char*, 3> LEDNAMES = {XKB_LED_NAME_NUM, XKB_LED_NAME_CAPS, XKB_LED_NAME_SCROLL};

//
uint32_t IKeyboard::getCapabilities() {
    return HID_INPUT_CAPABILITY_KEYBOARD;
}

eHIDType IKeyboard::getType() {
    return HID_TYPE_KEYBOARD;
}

IKeyboard::~IKeyboard() {
    events.destroy.emit();

    clearManuallyAllocd();
}

void IKeyboard::clearManuallyAllocd() {
    if (xkbStaticState)
        xkb_state_unref(xkbStaticState);

    if (xkbState)
        xkb_state_unref(xkbState);

    if (xkbKeymap)
        xkb_keymap_unref(xkbKeymap);

    if (xkbKeymapFD >= 0)
        close(xkbKeymapFD);

    if (xkbSymState)
        xkb_state_unref(xkbSymState);

    xkbSymState    = nullptr;
    xkbKeymap      = nullptr;
    xkbState       = nullptr;
    xkbStaticState = nullptr;
    xkbKeymapFD    = -1;
}

void IKeyboard::setKeymap(const SStringRuleNames& rules) {
    if (keymapOverridden) {
        Debug::log(LOG, "Ignoring setKeymap: keymap is overridden");
        return;
    }

    currentRules            = rules;
    xkb_rule_names XKBRULES = {
        .rules   = rules.rules.c_str(),
        .model   = rules.model.c_str(),
        .layout  = rules.layout.c_str(),
        .variant = rules.variant.c_str(),
        .options = rules.options.c_str(),
    };

    const auto CONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (!CONTEXT) {
        Debug::log(ERR, "setKeymap: CONTEXT null??");
        return;
    }

    clearManuallyAllocd();

    Debug::log(LOG, "Attempting to create a keymap for layout {} with variant {} (rules: {}, model: {}, options: {})", rules.layout, rules.variant, rules.rules, rules.model,
               rules.options);

    if (!xkbFilePath.empty()) {
        auto path = absolutePath(xkbFilePath, g_pConfigManager->configCurrentPath);

        if (FILE* const KEYMAPFILE = fopen(path.c_str(), "r"); !KEYMAPFILE)
            Debug::log(ERR, "Cannot open input:kb_file= file for reading");
        else {
            xkbKeymap = xkb_keymap_new_from_file(CONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
            fclose(KEYMAPFILE);
        }
    }

    if (!xkbKeymap)
        xkbKeymap = xkb_keymap_new_from_names(CONTEXT, &XKBRULES, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!xkbKeymap) {
        g_pConfigManager->addParseError("Invalid keyboard layout passed. ( rules: " + rules.rules + ", model: " + rules.model + ", variant: " + rules.variant +
                                        ", options: " + rules.options + ", layout: " + rules.layout + " )");

        Debug::log(ERR, "Keyboard layout {} with variant {} (rules: {}, model: {}, options: {}) couldn't have been loaded.", rules.layout, rules.variant, rules.rules, rules.model,
                   rules.options);
        memset(&XKBRULES, 0, sizeof(XKBRULES));

        currentRules.rules   = "";
        currentRules.model   = "";
        currentRules.variant = "";
        currentRules.options = "";
        currentRules.layout  = "us";

        xkbKeymap = xkb_keymap_new_from_names(CONTEXT, &XKBRULES, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    updateXKBTranslationState(xkbKeymap);

    const auto NUMLOCKON = g_pConfigManager->getDeviceInt(hlName, "numlock_by_default", "input:numlock_by_default");

    if (NUMLOCKON == 1) {
        // lock numlock
        const auto IDX = xkb_map_mod_get_index(xkbKeymap, XKB_MOD_NAME_NUM);

        if (IDX != XKB_MOD_INVALID)
            modifiersState.locked |= (uint32_t)1 << IDX;

        // 0 to avoid mods getting stuck if depressed during reload
        updateModifiers(0, 0, modifiersState.locked, modifiersState.group);
    }

    for (size_t i = 0; i < LEDNAMES.size(); ++i) {
        ledIndexes.at(i) = xkb_map_led_get_index(xkbKeymap, LEDNAMES.at(i));
        Debug::log(LOG, "xkb: LED index {} (name {}) got index {}", i, LEDNAMES.at(i), ledIndexes.at(i));
    }

    for (size_t i = 0; i < MODNAMES.size(); ++i) {
        modIndexes.at(i) = xkb_map_mod_get_index(xkbKeymap, MODNAMES.at(i));
        Debug::log(LOG, "xkb: Mod index {} (name {}) got index {}", i, MODNAMES.at(i), modIndexes.at(i));
    }

    updateKeymapFD();

    xkb_context_unref(CONTEXT);

    g_pSeatManager->updateActiveKeyboardData();
}

void IKeyboard::updateKeymapFD() {
    Debug::log(LOG, "Updating keymap fd for keyboard {}", deviceName);

    if (xkbKeymapFD >= 0)
        close(xkbKeymapFD);
    xkbKeymapFD = -1;

    auto cKeymapStr = xkb_keymap_get_as_string(xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    xkbKeymapString = cKeymapStr;
    free(cKeymapStr);

    int rw, ro;
    if (!allocateSHMFilePair(xkbKeymapString.length() + 1, &rw, &ro))
        Debug::log(ERR, "IKeyboard: failed to allocate shm pair for the keymap");
    else {
        auto keymapFDDest = mmap(nullptr, xkbKeymapString.length() + 1, PROT_READ | PROT_WRITE, MAP_SHARED, rw, 0);
        close(rw);
        if (keymapFDDest == MAP_FAILED) {
            Debug::log(ERR, "IKeyboard: failed to mmap a shm pair for the keymap");
            close(ro);
        } else {
            memcpy(keymapFDDest, xkbKeymapString.c_str(), xkbKeymapString.length());
            munmap(keymapFDDest, xkbKeymapString.length() + 1);
            xkbKeymapFD = ro;
        }
    }

    Debug::log(LOG, "Updated keymap fd to {}", xkbKeymapFD);
}

void IKeyboard::updateXKBTranslationState(xkb_keymap* const keymap) {

    if (xkbStaticState)
        xkb_state_unref(xkbStaticState);

    if (xkbState)
        xkb_state_unref(xkbState);

    if (xkbSymState)
        xkb_state_unref(xkbSymState);

    xkbState       = nullptr;
    xkbStaticState = nullptr;
    xkbSymState    = nullptr;

    if (keymap) {
        Debug::log(LOG, "Updating keyboard {:x}'s translation state from a provided keymap", (uintptr_t)this);
        xkbStaticState = xkb_state_new(keymap);
        xkbState       = xkb_state_new(keymap);
        xkbSymState    = xkb_state_new(keymap);
        return;
    }

    const auto KEYMAP     = xkbKeymap;
    const auto STATE      = xkbState;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    const auto PCONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    for (uint32_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE) == 1) {
            Debug::log(LOG, "Updating keyboard {:x}'s translation state from an active index {}", (uintptr_t)this, i);

            CVarList       keyboardLayouts(currentRules.layout, 0, ',');
            CVarList       keyboardModels(currentRules.model, 0, ',');
            CVarList       keyboardVariants(currentRules.variant, 0, ',');

            xkb_rule_names rules = {.rules = "", .model = "", .layout = "", .variant = "", .options = ""};

            std::string    layout, model, variant;
            layout  = keyboardLayouts[i % keyboardLayouts.size()];
            model   = keyboardModels[i % keyboardModels.size()];
            variant = keyboardVariants[i % keyboardVariants.size()];

            rules.layout  = layout.c_str();
            rules.model   = model.c_str();
            rules.variant = variant.c_str();

            auto KEYMAP = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 1, fallback without model/variant");
                rules.model   = "";
                rules.variant = "";
                KEYMAP        = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 2, fallback to us");
                rules.layout = "us";
                KEYMAP       = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            xkbState       = xkb_state_new(KEYMAP);
            xkbStaticState = xkb_state_new(KEYMAP);
            xkbSymState    = xkb_state_new(KEYMAP);

            xkb_keymap_unref(KEYMAP);
            xkb_context_unref(PCONTEXT);

            return;
        }
    }

    Debug::log(LOG, "Updating keyboard {:x}'s translation state from an unknown index", (uintptr_t)this);

    xkb_rule_names rules = {
        .rules   = currentRules.rules.c_str(),
        .model   = currentRules.model.c_str(),
        .layout  = currentRules.layout.c_str(),
        .variant = currentRules.variant.c_str(),
        .options = currentRules.options.c_str(),
    };

    const auto NEWKEYMAP = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkbState       = xkb_state_new(NEWKEYMAP);
    xkbStaticState = xkb_state_new(NEWKEYMAP);
    xkbSymState    = xkb_state_new(NEWKEYMAP);

    xkb_keymap_unref(NEWKEYMAP);
    xkb_context_unref(PCONTEXT);
}

std::string IKeyboard::getActiveLayout() {
    const auto KEYMAP     = xkbKeymap;
    const auto STATE      = xkbState;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    for (uint32_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE) == 1) {
            const auto LAYOUTNAME = xkb_keymap_layout_get_name(KEYMAP, i);

            if (LAYOUTNAME)
                return std::string(LAYOUTNAME);
            return "error";
        }
    }

    return "none";
}

std::optional<uint32_t> IKeyboard::getLEDs() {
    if (xkbState == nullptr)
        return {};

    uint32_t leds = 0;
    for (uint32_t i = 0; i < LED_COUNT; ++i) {
        if (xkb_state_led_index_is_active(xkbState, ledIndexes.at(i)))
            leds |= (1 << i);
    }

    return leds;
}

void IKeyboard::updateLEDs() {
    std::optional<uint32_t> leds = getLEDs();

    if (!leds.has_value())
        return;

    updateLEDs(leds.value());
}

void IKeyboard::updateLEDs(uint32_t leds) {
    if (!xkbState)
        return;

    if (isVirtual() && g_pInputManager->shouldIgnoreVirtualKeyboard(self.lock()))
        return;

    if (!aq())
        return;

    aq()->updateLEDs(leds);
}

uint32_t IKeyboard::getModifiers() {
    uint32_t modMask = modifiersState.depressed | modifiersState.latched;
    uint32_t mods    = 0;
    for (size_t i = 0; i < modIndexes.size(); ++i) {
        if (modIndexes.at(i) == XKB_MOD_INVALID)
            continue;

        if (!(modMask & (1 << modIndexes.at(i))))
            continue;

        mods |= (1 << i);
    }

    return mods;
}

void IKeyboard::updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!xkbState)
        return;

    xkb_state_update_mask(xkbState, depressed, latched, locked, 0, 0, group);

    if (xkbSymState)
        xkb_state_update_mask(xkbSymState, 0, 0, 0, 0, 0, group);

    if (!updateModifiersState())
        return;

    keyboardEvents.modifiers.emit(SModifiersEvent{
        .depressed = modifiersState.depressed,
        .latched   = modifiersState.latched,
        .locked    = modifiersState.locked,
        .group     = modifiersState.group,
    });

    updateLEDs();
}

bool IKeyboard::updateModifiersState() {
    if (!xkbState)
        return false;

    auto depressed = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_DEPRESSED);
    auto latched   = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LATCHED);
    auto locked    = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LOCKED);
    auto group     = xkb_state_serialize_layout(xkbState, XKB_STATE_LAYOUT_EFFECTIVE);

    if (depressed == modifiersState.depressed && latched == modifiersState.latched && locked == modifiersState.locked && group == modifiersState.group)
        return false;

    modifiersState.depressed = depressed;
    modifiersState.latched   = latched;
    modifiersState.locked    = locked;
    modifiersState.group     = group;

    return true;
}

void IKeyboard::updateXkbStateWithKey(uint32_t xkbKey, bool pressed) {

    const auto contains = std::find(pressedXKB.begin(), pressedXKB.end(), xkbKey) != pressedXKB.end();

    if (contains && pressed)
        return;
    if (!contains && !pressed)
        return;

    if (contains)
        std::erase(pressedXKB, xkbKey);
    else
        pressedXKB.emplace_back(xkbKey);

    xkb_state_update_key(xkbState, xkbKey, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);

    if (updateModifiersState()) {
        if (xkbSymState)
            xkb_state_update_mask(xkbSymState, 0, 0, 0, 0, 0, modifiersState.group);

        keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = modifiersState.depressed,
            .latched   = modifiersState.latched,
            .locked    = modifiersState.locked,
            .group     = modifiersState.group,
        });
    }
}
