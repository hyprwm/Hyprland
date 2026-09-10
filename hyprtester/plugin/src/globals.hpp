#pragma once

#include <src/plugins/PluginAPI.hpp>

#include <src/helpers/time/Time.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/managers/input/trackpad/gestures/ITrackpadGesture.hpp>

#include <src/debug/log/Logger.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string>
#include <vector>

inline HANDLE PHANDLE = nullptr;

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

    void setMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        updateModifiers(depressed, latched, locked, group);
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

class CKeyboardEventRecorder : public IKeyboardEventHandler {
  public:
    struct SEvent {
        uint32_t              keycode = 0;
        wl_keyboard_key_state state   = WL_KEYBOARD_KEY_STATE_RELEASED;
    };

    virtual void onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard>) override {
        m_events.emplace_back(SEvent{
            .keycode = event.keycode,
            .state   = event.state,
        });
    }

    std::vector<SEvent> m_events;
};

struct SPinchScaleEvents {
    size_t             beginCount = 0;
    size_t             endCount   = 0;
    float              beginScale = 0.F;
    float              endScale   = 0.F;
    std::vector<float> updateScales;
};

class CPinchScaleRecorder : public ITrackpadGesture {
  public:
    CPinchScaleRecorder(SP<SPinchScaleEvents> events) : m_events(std::move(events)) {
        ;
    }

    virtual void begin(const STrackpadGestureBegin& event) override {
        ++m_events->beginCount;
        m_events->beginScale = event.scale;
        ITrackpadGesture::begin(event);
    }

    virtual void update(const STrackpadGestureUpdate& event) override {
        m_events->updateScales.emplace_back(event.scale);
    }

    virtual void end(const STrackpadGestureEnd& event) override {
        ++m_events->endCount;
        m_events->endScale = event.scale;
    }

  private:
    SP<SPinchScaleEvents> m_events;
};

using PLUGIN_LUA_RESULT_FN = SDispatchResult (*)(lua_State* L);

inline int luaResult(lua_State* L, const SDispatchResult& result) {
    if (result.success)
        return 0;

    lua_pushstring(L, result.error.empty() ? "plugin function failed" : result.error.c_str());
    return lua_error(L);
}

inline void registerLuaFn(const std::string& name, PLUGIN_LUA_FN fn) {
    if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
        LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
}

template <PLUGIN_LUA_RESULT_FN FN>
inline void registerLuaFn(const std::string& name) {
    struct SAdapter {
        static int call(lua_State* L) {
            return luaResult(L, FN(L));
        }
    };

    registerLuaFn(name, SAdapter::call);
}

struct SUnit {
    const char* name;
    void (*registerFns)();
};

inline std::vector<SUnit> testFunctions;

#define REGISTER_UNIT(NAME)                                                                                                                                                        \
    static void       Unit_##NAME();                                                                                                                                               \
    static const bool s_register_unit_##NAME = [] {                                                                                                                                \
        testFunctions.emplace_back(SUnit{#NAME, Unit_##NAME});                                                                                                                     \
        return 1;                                                                                                                                                                  \
    }();                                                                                                                                                                           \
    static void Unit_##NAME()

extern SP<CTestMouse>             g_mouse;
extern SP<CTestKeyboard>          g_keyboard;
extern SP<CTestKeyboard>          g_keyboard2;
extern SP<CKeyboardEventRecorder> g_keyboardEventRecorder;
