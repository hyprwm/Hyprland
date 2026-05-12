#include <unistd.h>
#include <src/includes.hpp>
#include <sstream>
#include <any>
#include <cmath>

#define private public
#include <src/managers/input/InputManager.hpp>
#include <src/managers/PointerManager.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/helpers/Monitor.hpp>
#include <src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <src/desktop/rule/layerRule/LayerRuleEffectContainer.hpp>
#include <src/desktop/rule/windowRule/WindowRuleApplicator.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/Compositor.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/layout/LayoutManager.hpp>
#undef private

#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::Utils;
using namespace Hyprutils::String;

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static SDispatchResult test(std::string in) {
    return {.success = true};
}

// Trigger a snap move event for the active window
static SDispatchResult snapMove(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW->m_isFloating)
        return {.success = false, .error = "Window must be floating"};

    Vector2D pos  = PLASTWINDOW->m_realPosition->goal();
    Vector2D size = PLASTWINDOW->m_realSize->goal();

    g_layoutManager->performSnap(pos, size, PLASTWINDOW->layoutTarget(), MBIND_MOVE, -1, size);

    PLASTWINDOW->layoutTarget()->setPositionGlobal(CBox{pos, size});

    return {};
}

class CTestKeyboard : public IKeyboard {
  public:
    static SP<CTestKeyboard> create(bool isVirtual) {
        auto keeb           = SP<CTestKeyboard>(new CTestKeyboard());
        keeb->m_self        = keeb;
        keeb->m_isVirtual   = isVirtual;
        keeb->m_shareStates = !isVirtual;
        keeb->m_hlName      = "test-keyboard";
        keeb->m_deviceName  = "test-keyboard";
        return keeb;
    }

    virtual bool isVirtual() {
        return m_isVirtual;
    }

    virtual SP<Aquamarine::IKeyboard> aq() {
        return nullptr;
    }

    void sendKey(uint32_t key, bool pressed) {
        auto event = IKeyboard::SKeyEvent{
            .timeMs  = sc<uint32_t>(Time::millis(Time::steadyNow())),
            .keycode = key,
            .state   = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
        };
        updatePressed(event.keycode, pressed);
        m_keyboardEvents.key.emit(event);
    }

    void destroy() {
        m_events.destroy.emit();
    }

  private:
    bool m_isVirtual = false;
};

class CTestMouse : public IPointer {
  public:
    static SP<CTestMouse> create(bool isVirtual) {
        auto maus          = SP<CTestMouse>(new CTestMouse());
        maus->m_self       = maus;
        maus->m_isVirtual  = isVirtual;
        maus->m_deviceName = "test-mouse";
        maus->m_hlName     = "test-mouse";
        return maus;
    }

    virtual bool isVirtual() {
        return m_isVirtual;
    }

    virtual SP<Aquamarine::IPointer> aq() {
        return nullptr;
    }

    void destroy() {
        m_events.destroy.emit();
    }

  private:
    bool m_isVirtual = false;
};

SP<CTestMouse>         g_mouse;
SP<CTestKeyboard>      g_keyboard;

static SDispatchResult pressAlt(std::string in) {
    g_pInputManager->m_lastMods = in == "1" ? HL_MODIFIER_ALT : 0;

    return {.success = true};
}

static SDispatchResult simulateGesture(std::string in) {
    CVarList data(in);

    uint32_t fingers = 3;
    try {
        fingers = std::stoul(data[1]);
    } catch (...) { return {.success = false}; }

    if (data[0] == "down") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {0, 300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (data[0] == "up") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {0, -300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (data[0] == "left") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {-300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    }

    return {.success = true};
}

static SDispatchResult pinchUpdate(std::string in) {
    CVarList data(in);
    uint32_t fingers = 2;
    double   scale   = 1.0;
    Vector2D delta   = {};
    double   rotation{};

    if (data.size() < 2)
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<uint32_t>(data[0]); n)
        fingers = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<double>(data[1]); n)
        scale = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (data.size() > 2) {
        if (const auto n = strToNumber<double>(data[2]); n)
            delta.x = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    if (data.size() > 3) {
        if (const auto n = strToNumber<double>(data[3]); n)
            delta.y = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    if (data.size() > 4) {
        if (const auto n = strToNumber<double>(data[4]); n)
            rotation = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{
        .fingers  = fingers,
        .delta    = delta,
        .scale    = scale,
        .rotation = rotation,
    });

    return {};
}

static SDispatchResult pinchEnd(std::string in) {
    g_pTrackpadGestures->gestureEnd(IPointer::SPinchEndEvent{});

    return {};
}

static SDispatchResult expectCursorZoom(std::string in) {
    CVarList data(in);
    float    expected = 1.F;
    float    delta    = 0.01F;

    if (data.size() < 1)
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<float>(data[0]); n)
        expected = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (data.size() > 1) {
        if (const auto n = strToNumber<float>(data[1]); n)
            delta = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromVector(g_pInputManager->getMouseCoordsInternal());

    if (!PMONITOR)
        return {.success = false, .error = "No monitor under cursor"};

    const auto actual = PMONITOR->m_cursorZoom->value();

    if (std::abs(actual - expected) > delta)
        return {.success = false, .error = std::format("Expected cursor zoom {} ± {}, got {}", expected, delta, actual)};

    return {};
}

static SDispatchResult vkb(std::string in) {
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

static SDispatchResult scroll(std::string in) {
    double by;
    try {
        by = std::stod(in);
    } catch (...) { return SDispatchResult{.success = false, .error = "invalid input"}; }

    Log::logger->log(Log::DEBUG, "tester: scrolling by {}", by);

    g_mouse->m_pointerEvents.axis.emit(IPointer::SAxisEvent{
        .delta         = by,
        .deltaDiscrete = 120,
        .mouse         = true,
    });

    return {};
}

static SDispatchResult click(std::string in) {
    CVarList2 data(std::move(in));

    uint32_t  button;
    bool      pressed;
    try {
        button  = std::stoul(std::string{data[0]});
        pressed = std::stoul(std::string{data[1]}) == 1;
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    Log::logger->log(Log::DEBUG, "tester: mouse button {} state {}", button, pressed);

    g_mouse->m_pointerEvents.button.emit(IPointer::SButtonEvent{
        .timeMs = sc<uint32_t>(Time::millis(Time::steadyNow())),
        .button = button,
        .state  = pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
        .mouse  = true,
    });

    return {};
}

static SDispatchResult keybind(std::string in) {
    CVarList2 data(std::move(in));
    // 0 = release, 1 = press
    bool press;
    // See src/devices/IKeyboard.hpp : eKeyboardModifiers for modifier bitmasks
    // 0 = none, eKeyboardModifiers is shifted to start at 1
    uint32_t modifier;
    // keycode
    uint32_t key;
    try {
        press    = std::stoul(std::string{data[0]}) == 1;
        modifier = std::stoul(std::string{data[1]});
        key      = std::stoul(std::string{data[2]}) - 8; // xkb offset
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    uint32_t modifierMask = 0;
    if (modifier > 0)
        modifierMask = 1 << (modifier - 1);
    g_pInputManager->m_lastMods = modifierMask;
    g_keyboard->sendKey(key, press);

    return {};
}

static Desktop::Rule::CWindowRuleEffectContainer::storageType windowRuleIDX = 0;

//
static SDispatchResult addWindowRule(std::string in) {
    windowRuleIDX = Desktop::Rule::windowEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::windowEffects()->registerEffect("plugin_rule") != windowRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkWindowRule(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->m_ruleApplicator->m_otherProps.props.contains(windowRuleIDX))
        return {.success = false, .error = "No rule"};

    if (PLASTWINDOW->m_ruleApplicator->m_otherProps.props[windowRuleIDX]->effect != "effect")
        return {.success = false, .error = "Effect isn't \"effect\""};

    return {};
}

static Desktop::Rule::CLayerRuleEffectContainer::storageType layerRuleIDX = 0;

static SDispatchResult                                       addLayerRule(std::string in) {
    layerRuleIDX = Desktop::Rule::layerEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::layerEffects()->registerEffect("plugin_rule") != layerRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkLayerRule(std::string in) {
    if (g_pCompositor->m_layers.size() != 3)
        return {.success = false, .error = "Layers under test not here"};

    for (const auto& layer : g_pCompositor->m_layers) {
        if (layer->m_namespace == "rule-layer") {

            if (!layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "No rule"};

            if (layer->m_ruleApplicator->m_otherProps.props[layerRuleIDX]->effect != "effect")
                return {.success = false, .error = "Effect isn't \"effect\""};

        } else if (layer->m_namespace == "norule-layer") {

            if (layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "Rule even though it shouldn't"};

        } else
            return {.success = false, .error = "Unrecognized layer"};
    }

    return {};
}

static SDispatchResult checkPointerFocusLayer(std::string in) {
    const auto POINTERSURF = g_pSeatManager->m_state.pointerFocus.lock();

    if (!POINTERSURF)
        return {.success = false, .error = "No pointer focus"};

    const auto HLSURF = Desktop::View::CWLSurface::fromResource(POINTERSURF);
    const auto VIEW   = HLSURF ? HLSURF->view() : nullptr;
    const auto LAYER  = Desktop::View::CLayerSurface::fromView(VIEW);

    if (!LAYER) {
        const auto WINDOW = g_pCompositor->getWindowFromSurface(POINTERSURF);
        if (WINDOW)
            return {.success = false, .error = std::format("Pointer focus is a window surface with class '{}'", WINDOW->m_class)};

        return {.success = false, .error = std::format("Pointer focus is not a layer surface, view type is {}", VIEW ? sc<int>(VIEW->type()) : -1)};
    }

    if (LAYER->m_namespace != in)
        return {.success = false, .error = std::format("Pointer focus layer namespace is '{}', expected '{}'", LAYER->m_namespace, in)};

    return {};
}

static SDispatchResult setPointerFocusLayer(std::string in) {
    for (const auto& layer : g_pCompositor->m_layers) {
        if (layer->m_namespace != in)
            continue;

        const auto SURFACE = layer->wlSurface() ? layer->wlSurface()->resource() : nullptr;
        if (!SURFACE)
            return {.success = false, .error = std::format("Layer '{}' has no surface", in)};

        const auto LOCAL = layer->m_geometry.size() / 2.0;

        g_pSeatManager->setPointerFocus(SURFACE, LOCAL);
        g_pSeatManager->sendPointerMotion(Time::millis(Time::steadyNow()), LOCAL);
        return {};
    }

    return {.success = false, .error = std::format("No layer with namespace '{}'", in)};
}

static SDispatchResult floatingFocusOnFullscreen(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->m_isFloating)
        return {.success = false, .error = "Window must be floating"};

    if (PLASTWINDOW->alphaTotal() != 1.F)
        return {.success = false, .error = "floating window doesnt restore it opacity when focused on fullscreen workspace"};

    if (!PLASTWINDOW->m_createdOverFullscreen)
        return {.success = false, .error = "floating window doesnt get flagged as createdOverFullscreen"};

    return {};
}

static int luaResult(lua_State* L, const SDispatchResult& result) {
    if (result.success)
        return 0;

    lua_pushstring(L, result.error.empty() ? "plugin function failed" : result.error.c_str());
    return lua_error(L);
}

static int luaTest(lua_State* L) {
    return luaResult(L, ::test(""));
}

static int luaSnapMove(lua_State* L) {
    return luaResult(L, ::snapMove(""));
}

static int luaVkb(lua_State* L) {
    return luaResult(L, ::vkb(""));
}

static int luaAlt(lua_State* L) {
    return luaResult(L, ::pressAlt(std::to_string((int)luaL_checkinteger(L, 1))));
}

static int luaGesture(lua_State* L) {
    const auto direction = std::string{luaL_checkstring(L, 1)};
    const auto fingers   = (int)luaL_optinteger(L, 2, 3);
    return luaResult(L, ::simulateGesture(std::format("{},{}", direction, fingers)));
}

static int luaPinchUpdate(lua_State* L) {
    std::string in = std::format("{},{}", (int)luaL_checkinteger(L, 1), (double)luaL_checknumber(L, 2));

    if (lua_gettop(L) > 2)
        in += std::format(",{}", (double)luaL_checknumber(L, 3));
    if (lua_gettop(L) > 3)
        in += std::format(",{}", (double)luaL_checknumber(L, 4));
    if (lua_gettop(L) > 4)
        in += std::format(",{}", (double)luaL_checknumber(L, 5));

    return luaResult(L, ::pinchUpdate(in));
}

static int luaPinchEnd(lua_State* L) {
    return luaResult(L, ::pinchEnd(""));
}

static int luaExpectCursorZoom(lua_State* L) {
    const auto expected = (double)luaL_checknumber(L, 1);

    if (lua_gettop(L) > 1)
        return luaResult(L, ::expectCursorZoom(std::format("{},{}", expected, (double)luaL_checknumber(L, 2))));

    return luaResult(L, ::expectCursorZoom(std::format("{}", expected)));
}

static int luaScroll(lua_State* L) {
    return luaResult(L, ::scroll(std::to_string((double)luaL_checknumber(L, 1))));
}

static int luaClick(lua_State* L) {
    const auto button  = (int)luaL_checkinteger(L, 1);
    const auto pressed = (int)luaL_checkinteger(L, 2);
    return luaResult(L, ::click(std::format("{},{}", button, pressed)));
}

static int luaKeybind(lua_State* L) {
    const auto press    = (int)luaL_checkinteger(L, 1);
    const auto modifier = (int)luaL_checkinteger(L, 2);
    const auto key      = (int)luaL_checkinteger(L, 3);
    return luaResult(L, ::keybind(std::format("{},{},{}", press, modifier, key)));
}

static int luaAddWindowRule(lua_State* L) {
    return luaResult(L, ::addWindowRule(""));
}

static int luaCheckWindowRule(lua_State* L) {
    return luaResult(L, ::checkWindowRule(""));
}

static int luaAddLayerRule(lua_State* L) {
    return luaResult(L, ::addLayerRule(""));
}

static int luaCheckLayerRule(lua_State* L) {
    return luaResult(L, ::checkLayerRule(""));
}

static int luaCheckPointerFocusLayer(lua_State* L) {
    return luaResult(L, ::checkPointerFocusLayer(luaL_checkstring(L, 1)));
}

static int luaSetPointerFocusLayer(lua_State* L) {
    return luaResult(L, ::setPointerFocusLayer(luaL_checkstring(L, 1)));
}

static int luaFloatingFocusOnFullscreen(lua_State* L) {
    return luaResult(L, ::floatingFocusOnFullscreen(""));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    auto addLuaFn = [](const std::string& name, PLUGIN_LUA_FN fn) {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
            Log::logger->log(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
    };

    addLuaFn("test", ::luaTest);
    addLuaFn("snapmove", ::luaSnapMove);
    addLuaFn("vkb", ::luaVkb);
    addLuaFn("alt", ::luaAlt);
    addLuaFn("gesture", ::luaGesture);
    addLuaFn("pinch_update", ::luaPinchUpdate);
    addLuaFn("pinch_end", ::luaPinchEnd);
    addLuaFn("expect_cursor_zoom", ::luaExpectCursorZoom);
    addLuaFn("scroll", ::luaScroll);
    addLuaFn("click", ::luaClick);
    addLuaFn("keybind", ::luaKeybind);
    addLuaFn("add_window_rule", ::luaAddWindowRule);
    addLuaFn("check_window_rule", ::luaCheckWindowRule);
    addLuaFn("add_layer_rule", ::luaAddLayerRule);
    addLuaFn("check_layer_rule", ::luaCheckLayerRule);
    addLuaFn("check_pointer_focus_layer", ::luaCheckPointerFocusLayer);
    addLuaFn("set_pointer_focus_layer", ::luaSetPointerFocusLayer);
    addLuaFn("floating_focus_on_fullscreen", ::luaFloatingFocusOnFullscreen);

    // init mouse
    g_mouse = CTestMouse::create(false);
    g_pInputManager->newMouse(g_mouse);

    // init keyboard
    g_keyboard = CTestKeyboard::create(false);
    g_pInputManager->newKeyboard(g_keyboard);

    return {"hyprtestplugin", "hyprtestplugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_mouse->destroy();
    g_mouse.reset();
    g_keyboard->destroy();
    g_keyboard.reset();
}
