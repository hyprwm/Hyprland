#include "IKeyboard.hpp"
#include "../defines.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../config/ConfigManager.hpp"
#include <sys/mman.h>
#include <aquamarine/input/Input.hpp>
#include <cstring>

using namespace Hyprutils::OS;

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
    m_events.destroy.emit();

    clearManuallyAllocd();
}

void IKeyboard::clearManuallyAllocd() {
    if (m_xkbStaticState)
        xkb_state_unref(m_xkbStaticState);

    if (m_xkbState)
        xkb_state_unref(m_xkbState);

    if (m_xkbKeymap)
        xkb_keymap_unref(m_xkbKeymap);

    if (m_xkbSymState)
        xkb_state_unref(m_xkbSymState);

    m_xkbSymState    = nullptr;
    m_xkbKeymap      = nullptr;
    m_xkbState       = nullptr;
    m_xkbStaticState = nullptr;
    m_xkbKeymapFD.reset();
    m_xkbKeymapV1FD.reset();
}

void IKeyboard::setKeymap(const SStringRuleNames& rules) {
    if (m_keymapOverridden) {
        Debug::log(LOG, "Ignoring setKeymap: keymap is overridden");
        return;
    }

    m_currentRules          = rules;
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

    if (!m_xkbFilePath.empty()) {
        auto path = absolutePath(m_xkbFilePath, g_pConfigManager->m_configCurrentPath);

        if (FILE* const KEYMAPFILE = fopen(path.c_str(), "r"); !KEYMAPFILE)
            Debug::log(ERR, "Cannot open input:kb_file= file for reading");
        else {
            m_xkbKeymap = xkb_keymap_new_from_file(CONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
            fclose(KEYMAPFILE);
        }
    }

    if (!m_xkbKeymap)
        m_xkbKeymap = xkb_keymap_new_from_names2(CONTEXT, &XKBRULES, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!m_xkbKeymap) {
        g_pConfigManager->addParseError("Invalid keyboard layout passed. ( rules: " + rules.rules + ", model: " + rules.model + ", variant: " + rules.variant +
                                        ", options: " + rules.options + ", layout: " + rules.layout + " )");

        Debug::log(ERR, "Keyboard layout {} with variant {} (rules: {}, model: {}, options: {}) couldn't have been loaded.", rules.layout, rules.variant, rules.rules, rules.model,
                   rules.options);
        memset(&XKBRULES, 0, sizeof(XKBRULES));

        m_currentRules.rules   = "";
        m_currentRules.model   = "";
        m_currentRules.variant = "";
        m_currentRules.options = "";
        m_currentRules.layout  = "us";

        m_xkbKeymap = xkb_keymap_new_from_names2(CONTEXT, &XKBRULES, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    updateXKBTranslationState(m_xkbKeymap);

    const auto NUMLOCKON = g_pConfigManager->getDeviceInt(m_hlName, "numlock_by_default", "input:numlock_by_default");

    if (NUMLOCKON == 1) {
        // lock numlock
        const auto IDX = xkb_map_mod_get_index(m_xkbKeymap, XKB_MOD_NAME_NUM);

        if (IDX != XKB_MOD_INVALID)
            m_modifiersState.locked |= sc<uint32_t>(1) << IDX;

        // 0 to avoid mods getting stuck if depressed during reload
        updateModifiers(0, 0, m_modifiersState.locked, m_modifiersState.group);
    }

    for (size_t i = 0; i < std::min(LEDNAMES.size(), m_ledIndexes.size()); ++i) {
        m_ledIndexes[i] = xkb_map_led_get_index(m_xkbKeymap, LEDNAMES[i]);
        Debug::log(LOG, "xkb: LED index {} (name {}) got index {}", i, LEDNAMES[i], m_ledIndexes[i]);
    }

    for (size_t i = 0; i < std::min(MODNAMES.size(), m_modIndexes.size()); ++i) {
        m_modIndexes[i] = xkb_map_mod_get_index(m_xkbKeymap, MODNAMES[i]);
        Debug::log(LOG, "xkb: Mod index {} (name {}) got index {}", i, MODNAMES[i], m_modIndexes[i]);
    }

    updateKeymapFD();

    xkb_context_unref(CONTEXT);

    g_pSeatManager->updateActiveKeyboardData();
}

void IKeyboard::updateKeymapFD() {
    Debug::log(LOG, "Updating keymap fd for keyboard {}", m_deviceName);

    if (m_xkbKeymapFD.isValid())
        m_xkbKeymapFD.reset();

    if (m_xkbKeymapV1FD.isValid())
        m_xkbKeymapV1FD.reset();

    auto cKeymapStr   = xkb_keymap_get_as_string(m_xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V2);
    m_xkbKeymapString = cKeymapStr;
    free(cKeymapStr);
    auto cKeymapV1Str   = xkb_keymap_get_as_string(m_xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    m_xkbKeymapV1String = cKeymapV1Str;
    free(cKeymapV1Str);

    CFileDescriptor rw, ro, rwV1, roV1;
    if (!allocateSHMFilePair(m_xkbKeymapString.length() + 1, rw, ro))
        Debug::log(ERR, "IKeyboard: failed to allocate shm pair for the keymap");
    else if (!allocateSHMFilePair(m_xkbKeymapV1String.length() + 1, rwV1, roV1)) {
        ro.reset();
        rw.reset();
        Debug::log(ERR, "IKeyboard: failed to allocate shm pair for keymap V1");
    } else {
        auto keymapFDDest   = mmap(nullptr, m_xkbKeymapString.length() + 1, PROT_READ | PROT_WRITE, MAP_SHARED, rw.get(), 0);
        auto keymapV1FDDest = mmap(nullptr, m_xkbKeymapV1String.length() + 1, PROT_READ | PROT_WRITE, MAP_SHARED, rwV1.get(), 0);
        rw.reset();
        rwV1.reset();

        if (keymapFDDest == MAP_FAILED || keymapV1FDDest == MAP_FAILED) {
            Debug::log(ERR, "IKeyboard: failed to mmap a shm pair for the keymap");
            ro.reset();
            roV1.reset();
        } else {
            memcpy(keymapFDDest, m_xkbKeymapString.c_str(), m_xkbKeymapString.length());
            munmap(keymapFDDest, m_xkbKeymapString.length() + 1);
            m_xkbKeymapFD = std::move(ro);
            memcpy(keymapV1FDDest, m_xkbKeymapV1String.c_str(), m_xkbKeymapV1String.length());
            munmap(keymapV1FDDest, m_xkbKeymapV1String.length() + 1);
            m_xkbKeymapV1FD = std::move(roV1);
        }
    }

    Debug::log(LOG, "Updated keymap fd to {}, keymap V1 to: {}", m_xkbKeymapFD.get(), m_xkbKeymapV1FD.get());
}

void IKeyboard::updateXKBTranslationState(xkb_keymap* const keymap) {

    if (m_xkbStaticState)
        xkb_state_unref(m_xkbStaticState);

    if (m_xkbState)
        xkb_state_unref(m_xkbState);

    if (m_xkbSymState)
        xkb_state_unref(m_xkbSymState);

    m_xkbState       = nullptr;
    m_xkbStaticState = nullptr;
    m_xkbSymState    = nullptr;

    if (keymap) {
        Debug::log(LOG, "Updating keyboard {:x}'s translation state from a provided keymap", rc<uintptr_t>(this));
        m_xkbStaticState = xkb_state_new(keymap);
        m_xkbState       = xkb_state_new(keymap);
        m_xkbSymState    = xkb_state_new(keymap);
        return;
    }

    const auto KEYMAP     = m_xkbKeymap;
    const auto STATE      = m_xkbState;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    const auto PCONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    for (uint32_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE) == 1) {
            Debug::log(LOG, "Updating keyboard {:x}'s translation state from an active index {}", rc<uintptr_t>(this), i);

            CVarList       keyboardLayouts(m_currentRules.layout, 0, ',');
            CVarList       keyboardModels(m_currentRules.model, 0, ',');
            CVarList       keyboardVariants(m_currentRules.variant, 0, ',');

            xkb_rule_names rules = {.rules = "", .model = "", .layout = "", .variant = "", .options = ""};

            std::string    layout, model, variant;
            layout  = keyboardLayouts[i % keyboardLayouts.size()];
            model   = keyboardModels[i % keyboardModels.size()];
            variant = keyboardVariants[i % keyboardVariants.size()];

            rules.layout  = layout.c_str();
            rules.model   = model.c_str();
            rules.variant = variant.c_str();

            auto KEYMAP = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 1, fallback without model/variant");
                rules.model   = "";
                rules.variant = "";
                KEYMAP        = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 2, fallback to us");
                rules.layout = "us";
                KEYMAP       = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            m_xkbState       = xkb_state_new(KEYMAP);
            m_xkbStaticState = xkb_state_new(KEYMAP);
            m_xkbSymState    = xkb_state_new(KEYMAP);

            xkb_keymap_unref(KEYMAP);
            xkb_context_unref(PCONTEXT);

            return;
        }
    }

    Debug::log(LOG, "Updating keyboard {:x}'s translation state from an unknown index", rc<uintptr_t>(this));

    xkb_rule_names rules = {
        .rules   = m_currentRules.rules.c_str(),
        .model   = m_currentRules.model.c_str(),
        .layout  = m_currentRules.layout.c_str(),
        .variant = m_currentRules.variant.c_str(),
        .options = m_currentRules.options.c_str(),
    };

    const auto NEWKEYMAP = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);

    m_xkbState       = xkb_state_new(NEWKEYMAP);
    m_xkbStaticState = xkb_state_new(NEWKEYMAP);
    m_xkbSymState    = xkb_state_new(NEWKEYMAP);

    xkb_keymap_unref(NEWKEYMAP);
    xkb_context_unref(PCONTEXT);
}

std::optional<xkb_layout_index_t> IKeyboard::getActiveLayoutIndex() {
    const auto KEYMAP     = m_xkbKeymap;
    const auto STATE      = m_xkbState;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    for (xkb_layout_index_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE) == 1)
            return i;
    }

    return {};
}

std::string IKeyboard::getActiveLayout() {
    const auto KEYMAP     = m_xkbKeymap;
    const auto STATE      = m_xkbState;
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
    if (m_xkbState == nullptr)
        return {};

    uint32_t leds = 0;
    for (uint32_t i = 0; i < std::min(sc<size_t>(LED_COUNT), m_ledIndexes.size()); ++i) {
        if (xkb_state_led_index_is_active(m_xkbState, m_ledIndexes[i]))
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
    if (!m_xkbState)
        return;

    if (isVirtual() && g_pInputManager->shouldIgnoreVirtualKeyboard(m_self.lock()))
        return;

    if (!aq())
        return;

    aq()->updateLEDs(leds);
}

uint32_t IKeyboard::getModifiers() {
    uint32_t modMask = m_modifiersState.depressed | m_modifiersState.latched;
    uint32_t mods    = 0;
    for (size_t i = 0; i < m_modIndexes.size(); ++i) {
        if (m_modIndexes[i] == XKB_MOD_INVALID)
            continue;

        if (!(modMask & (1 << m_modIndexes[i])))
            continue;

        mods |= (1 << i);
    }

    return mods;
}

void IKeyboard::updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    if (!m_xkbState)
        return;

    xkb_state_update_mask(m_xkbState, depressed, latched, locked, 0, 0, group);

    if (m_xkbSymState)
        xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, group);

    if (!updateModifiersState())
        return;

    m_keyboardEvents.modifiers.emit(SModifiersEvent{
        .depressed = m_modifiersState.depressed,
        .latched   = m_modifiersState.latched,
        .locked    = m_modifiersState.locked,
        .group     = m_modifiersState.group,
    });

    updateLEDs();
}

bool IKeyboard::updateModifiersState() {
    if (!m_xkbState)
        return false;

    auto depressed = xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_DEPRESSED);
    auto latched   = xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LATCHED);
    auto locked    = xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LOCKED);
    auto group     = xkb_state_serialize_layout(m_xkbState, XKB_STATE_LAYOUT_EFFECTIVE);

    if (depressed == m_modifiersState.depressed && latched == m_modifiersState.latched && locked == m_modifiersState.locked && group == m_modifiersState.group)
        return false;

    m_modifiersState.depressed = depressed;
    m_modifiersState.latched   = latched;
    m_modifiersState.locked    = locked;
    m_modifiersState.group     = group;

    return true;
}

void IKeyboard::updateXkbStateWithKey(uint32_t xkbKey, bool pressed) {
    xkb_state_update_key(m_xkbState, xkbKey, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);

    if (updateModifiersState()) {
        if (m_xkbSymState)
            xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, m_modifiersState.group);

        m_keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = m_modifiersState.depressed,
            .latched   = m_modifiersState.latched,
            .locked    = m_modifiersState.locked,
            .group     = m_modifiersState.group,
        });
    }
}

bool IKeyboard::updatePressed(uint32_t key, bool pressed) {
    const auto contains = getPressed(key);

    if (contains && pressed)
        return false;
    if (!contains && !pressed)
        return false;

    if (contains)
        std::erase(m_pressed, key);
    else
        m_pressed.emplace_back(key);

    return true;
}

bool IKeyboard::getPressed(uint32_t key) {
    return std::ranges::contains(m_pressed, key);
}

bool IKeyboard::shareStates() {
    return m_shareStates;
}

void IKeyboard::setShareStatesAuto(bool shareStates) {
    if (m_shareStatesAuto)
        m_shareStates = shareStates;
}
