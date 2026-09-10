#include "private.hpp"
#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

SP<CTestMouse>             g_mouse;
SP<CTestKeyboard>          g_keyboard;
SP<CTestKeyboard>          g_keyboard2;
SP<CKeyboardEventRecorder> g_keyboardEventRecorder;

static SDispatchResult     test(lua_State* L) {
    return {.success = true};
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    registerLuaFn<test>("test");

    for (const auto& UNIT : testFunctions)
        UNIT.registerFns();

    g_mouse = CTestMouse::create(false);
    g_pInputManager->newMouse(g_mouse);

    g_keyboard = CTestKeyboard::create(false);
    g_pInputManager->newKeyboard(g_keyboard);

    g_keyboard2 = CTestKeyboard::create(false);
    g_pInputManager->newKeyboard(g_keyboard2);

    return {"hyprtestplugin", "hyprtestplugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_keyboardEventRecorder)
        g_pSeatManager->m_keyboardEventHandlers.remove(g_keyboardEventRecorder);
    g_keyboardEventRecorder.reset();
    g_mouse->destroy();
    g_mouse.reset();
    g_keyboard->destroy();
    g_keyboard.reset();
    g_keyboard2->destroy();
    g_keyboard2.reset();
}
