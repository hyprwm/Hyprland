#include "../private.hpp"
#include "../globals.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

static SDispatchResult vkb(lua_State* L) {
    auto tkb0 = CTestKeyboard::create(false);
    auto tkb1 = CTestKeyboard::create(false);
    auto vkb0 = CTestKeyboard::create(true);

    g_pInputManager->newKeyboard(tkb0);
    g_pInputManager->newKeyboard(tkb1);
    g_pInputManager->newKeyboard(vkb0);

    CScopeGuard    x([&] {
        tkb0->destroy();
        tkb1->destroy();
        vkb0->destroy();
    });

    const auto&    PRESSED = g_pInputManager->getKeysFromAllKBs();
    const uint32_t TESTKEY = 1;

    tkb0->sendKey(TESTKEY, true);
    if (!std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected pressed key not found",
        };
    }

    tkb1->sendKey(TESTKEY, true);
    tkb0->sendKey(TESTKEY, false);
    if (!std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected pressed key not found (kb share state)",
        };
    }

    vkb0->sendKey(TESTKEY, true);
    tkb1->sendKey(TESTKEY, false);
    if (std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected released key found in pressed (vkb no share state)",
        };
    }

    return {};
}

static SDispatchResult scroll(lua_State* L) {
    const auto BY = (double)luaL_checknumber(L, 1);

    LOG(Log::DEBUG, "tester: scrolling by {}", BY);

    g_mouse->m_pointerEvents.axis.emit(IPointer::SAxisEvent{
        .delta         = BY,
        .deltaDiscrete = 120,
        .mouse         = true,
    });

    return {};
}

static SDispatchResult click(lua_State* L) {
    const auto BUTTON  = sc<uint32_t>(luaL_checkinteger(L, 1));
    const auto PRESSED = luaL_checkinteger(L, 2) != 0;

    LOG(Log::DEBUG, "tester: mouse button {} state {}", BUTTON, PRESSED);

    g_mouse->m_pointerEvents.button.emit(IPointer::SButtonEvent{
        .timeMs = sc<uint32_t>(Time::millis(Time::steadyNow())),
        .button = BUTTON,
        .state  = PRESSED ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
        .mouse  = true,
    });

    return {};
}

static SDispatchResult keybind(lua_State* L) {
    // 0 = release, 1 = press
    const auto PRESS = luaL_checkinteger(L, 1) != 0;
    // See src/devices/IKeyboard.hpp : eKeyboardModifiers for modifier bitmasks
    // 0 = none, eKeyboardModifiers is shifted to start at 1
    const auto MODIFIER = sc<uint32_t>(luaL_checkinteger(L, 2));
    // keycode
    const auto          KEY = sc<uint32_t>(luaL_checkinteger(L, 3)) - 8; // xkb offset

    Input::ModifierMask modifierMask = Input::HL_MODIFIER_NONE;
    if (MODIFIER > 0)
        modifierMask = sc<Input::ModifierMask>(1 << (MODIFIER - 1));
    g_pInputManager->m_lastMods = modifierMask;
    g_keyboard->sendKey(KEY, PRESS);

    return {};
}

static SDispatchResult keybind2(lua_State* L) {
    const auto          PRESS    = luaL_checkinteger(L, 1) != 0;
    const auto          MODIFIER = sc<uint32_t>(luaL_checkinteger(L, 2));
    const auto          KEY      = sc<uint32_t>(luaL_checkinteger(L, 3)) - 8;

    Input::ModifierMask modifierMask = Input::HL_MODIFIER_NONE;
    if (MODIFIER > 0)
        modifierMask = sc<Input::ModifierMask>(1 << (MODIFIER - 1));
    g_pInputManager->m_lastMods = modifierMask;
    g_keyboard2->sendKey(KEY, PRESS);

    return {};
}

static SDispatchResult keybindModmask(lua_State* L) {
    // 0 = release, 1 = press
    const auto PRESS = luaL_checkinteger(L, 1) != 0;
    // See src/devices/IKeyboard.hpp : eKeyboardModifiers for modifier bitmasks
    // 0 = none, eKeyboardModifiers is shifted to start at 1
    const auto MODIFIERMASK = sc<uint32_t>(luaL_checkinteger(L, 2));
    // keycode
    const auto KEY = sc<uint32_t>(luaL_checkinteger(L, 3)) - 8; // xkb offset

    g_pInputManager->m_lastMods = g_pInputManager->xkbModsToHyprland(g_keyboard, MODIFIERMASK);
    g_keyboard->setMods(MODIFIERMASK, 0, 0, 0);
    g_keyboard->sendKey(KEY, PRESS);

    return {};
}

static SDispatchResult setMods(lua_State* L) {
    const auto        KBINDEX   = sc<uint32_t>(luaL_checkinteger(L, 1));
    const auto        DEPRESSED = sc<uint32_t>(luaL_checkinteger(L, 2));
    const auto        LATCHED   = sc<uint32_t>(luaL_checkinteger(L, 3));
    const auto        LOCKED    = sc<uint32_t>(luaL_checkinteger(L, 4));
    const auto        GROUP     = sc<uint32_t>(luaL_checkinteger(L, 5));

    SP<CTestKeyboard> kb = (KBINDEX == 0) ? g_keyboard : g_keyboard2;
    kb->setMods(DEPRESSED, LATCHED, LOCKED, GROUP);

    return {};
}

static SDispatchResult pressAlt(lua_State* L) {
    g_pInputManager->m_lastMods = luaL_checkinteger(L, 1) != 0 ? Input::HL_MODIFIER_ALT : Input::HL_MODIFIER_NONE;

    return {.success = true};
}

static SDispatchResult registerKeyboardEventRecorder(lua_State* L) {
    if (!g_keyboardEventRecorder)
        g_keyboardEventRecorder = makeShared<CKeyboardEventRecorder>();
    else
        g_pSeatManager->m_keyboardEventHandlers.remove(g_keyboardEventRecorder);

    g_keyboardEventRecorder->m_events.clear();
    g_pSeatManager->m_keyboardEventHandlers.push(g_keyboardEventRecorder);
    return {};
}

static SDispatchResult removeKeyboardEventRecorder(lua_State* L) {
    if (g_keyboardEventRecorder)
        g_pSeatManager->m_keyboardEventHandlers.remove(g_keyboardEventRecorder);

    return {};
}

static SDispatchResult expectKeyboardEvents(lua_State* L) {
    if (!g_keyboardEventRecorder)
        return {.success = false, .error = "Keyboard event recorder has not been registered"};

    const int ARGS = lua_gettop(L);
    if (ARGS % 2 != 0)
        luaL_error(L, "expected keycode/state pairs");

    std::vector<CKeyboardEventRecorder::SEvent> expected;
    expected.reserve(ARGS / 2);
    for (int i = 1; i <= ARGS; i += 2)
        expected.emplace_back(CKeyboardEventRecorder::SEvent{
            .keycode = sc<uint32_t>(luaL_checkinteger(L, i)),
            .state   = sc<wl_keyboard_key_state>(luaL_checkinteger(L, i + 1)),
        });

    if (g_keyboardEventRecorder->m_events.size() != expected.size())
        return {.success = false, .error = std::format("Expected {} keyboard events, recorded {}", expected.size(), g_keyboardEventRecorder->m_events.size())};

    for (size_t i = 0; i < expected.size(); ++i) {
        const auto& ACTUAL = g_keyboardEventRecorder->m_events[i];
        if (ACTUAL.keycode == expected[i].keycode && ACTUAL.state == expected[i].state)
            continue;

        return {.success = false,
                .error   = std::format("Keyboard event {}: expected keycode {} state {}, recorded keycode {} state {}", i, expected[i].keycode, sc<uint32_t>(expected[i].state),
                                       ACTUAL.keycode, sc<uint32_t>(ACTUAL.state))};
    }

    return {};
}

REGISTER_UNIT(devices) {
    registerLuaFn<vkb>("vkb");
    registerLuaFn<pressAlt>("alt");
    registerLuaFn<scroll>("scroll");
    registerLuaFn<click>("click");
    registerLuaFn<keybind>("keybind");
    registerLuaFn<keybind2>("keybind2");
    registerLuaFn<keybindModmask>("keybind_modmask");
    registerLuaFn<setMods>("set_mods");
    registerLuaFn<registerKeyboardEventRecorder>("register_keyboard_event_recorder");
    registerLuaFn<removeKeyboardEventRecorder>("remove_keyboard_event_recorder");
    registerLuaFn<expectKeyboardEvents>("expect_keyboard_events");
}