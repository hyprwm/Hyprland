#include "IKeyboard.hpp"
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../managers/input/InputManager.hpp"

uint32_t IKeyboard::getCapabilities() {
    return HID_INPUT_CAPABILITY_KEYBOARD;
}

eHIDType IKeyboard::getType() {
    return HID_TYPE_KEYBOARD;
}

IKeyboard::~IKeyboard() {
    events.destroy.emit();

    if (!xkbTranslationState)
        return;

    xkb_state_unref(xkbTranslationState);
    xkbTranslationState = nullptr;
}

void IKeyboard::updateXKBTranslationState(xkb_keymap* const keymap) {

    if (xkbTranslationState)
        xkb_state_unref(xkbTranslationState);

    if (keymap) {
        Debug::log(LOG, "Updating keyboard {:x}'s translation state from a provided keymap", (uintptr_t)this);
        xkbTranslationState = xkb_state_new(keymap);
        return;
    }

    const auto WLRKB      = wlr();
    const auto KEYMAP     = WLRKB->keymap;
    const auto STATE      = WLRKB->xkb_state;
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

            xkbTranslationState = xkb_state_new(KEYMAP);

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

    xkbTranslationState = xkb_state_new(NEWKEYMAP);

    xkb_keymap_unref(NEWKEYMAP);
    xkb_context_unref(PCONTEXT);
}

std::string IKeyboard::getActiveLayout() {
    const auto WLRKB      = wlr();
    const auto KEYMAP     = WLRKB->keymap;
    const auto STATE      = WLRKB->xkb_state;
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

void IKeyboard::updateLEDs() {
    auto keyboard = wlr();

    if (!keyboard || keyboard->xkb_state == nullptr)
        return;

    uint32_t leds = 0;
    for (uint32_t i = 0; i < WLR_LED_COUNT; ++i) {
        if (xkb_state_led_index_is_active(keyboard->xkb_state, keyboard->led_indexes[i]))
            leds |= (1 << i);
    }

    updateLEDs(leds);
}

void IKeyboard::updateLEDs(uint32_t leds) {
    auto keyboard = wlr();

    if (!keyboard || keyboard->xkb_state == nullptr)
        return;

    if (isVirtual() && g_pInputManager->shouldIgnoreVirtualKeyboard(self.lock()))
        return;

    wlr_keyboard_led_update(keyboard, leds);
}
