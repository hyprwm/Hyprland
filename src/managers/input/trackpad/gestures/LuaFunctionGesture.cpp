#include "LuaFunctionGesture.hpp"

#include "../../../../config/lua/ConfigManager.hpp"
#include "../../../../config/lua/LuaCaller.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static const char* directionToString(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_HORIZONTAL: return "HORIZONTAL";
        case TRACKPAD_GESTURE_DIR_VERTICAL: return "VERTICAL";
        case TRACKPAD_GESTURE_DIR_LEFT: return "LEFT";
        case TRACKPAD_GESTURE_DIR_RIGHT: return "RIGHT";
        case TRACKPAD_GESTURE_DIR_UP: return "UP";
        case TRACKPAD_GESTURE_DIR_DOWN: return "DOWN";
        case TRACKPAD_GESTURE_DIR_SWIPE: return "SWIPE";
        case TRACKPAD_GESTURE_DIR_PINCH: return "PINCH";
        case TRACKPAD_GESTURE_DIR_PINCH_IN: return "PINCH_IN";
        case TRACKPAD_GESTURE_DIR_PINCH_OUT: return "PINCH_OUT";
        default: return "NONE";
    }
}

static void pushVec2(lua_State* L, const Vector2D& vec) {
    lua_newtable(L);
    lua_pushnumber(L, vec.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, vec.y);
    lua_setfield(L, -2, "y");
}

static void pushGestureBase(lua_State* L, const char* phase, eTrackpadGestureDirection direction) {
    lua_newtable(L);
    lua_pushstring(L, phase);
    lua_setfield(L, -2, "phase");
    lua_pushstring(L, directionToString(direction));
    lua_setfield(L, -2, "direction");
}

static int pushGestureEvent(lua_State* L, const ITrackpadGesture::STrackpadGestureBegin& e) {
    pushGestureBase(L, "start", e.direction);

    if (e.swipe) {
        lua_pushstring(L, "swipe");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.swipe->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushinteger(L, e.swipe->fingers);
        lua_setfield(L, -2, "fingers");
        pushVec2(L, e.swipe->delta);
        lua_setfield(L, -2, "delta");
    } else if (e.pinch) {
        lua_pushstring(L, "pinch");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.pinch->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushinteger(L, e.pinch->fingers);
        lua_setfield(L, -2, "fingers");
        pushVec2(L, e.pinch->delta);
        lua_setfield(L, -2, "delta");
        lua_pushnumber(L, e.pinch->scale);
        lua_setfield(L, -2, "scale");
        lua_pushnumber(L, e.pinch->rotation);
        lua_setfield(L, -2, "rotation");
    }

    return 1;
}

static int pushGestureEvent(lua_State* L, const ITrackpadGesture::STrackpadGestureUpdate& e) {
    pushGestureBase(L, "update", e.direction);

    if (e.swipe) {
        lua_pushstring(L, "swipe");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.swipe->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushinteger(L, e.swipe->fingers);
        lua_setfield(L, -2, "fingers");
        pushVec2(L, e.swipe->delta);
        lua_setfield(L, -2, "delta");
    } else if (e.pinch) {
        lua_pushstring(L, "pinch");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.pinch->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushinteger(L, e.pinch->fingers);
        lua_setfield(L, -2, "fingers");
        pushVec2(L, e.pinch->delta);
        lua_setfield(L, -2, "delta");
        lua_pushnumber(L, e.pinch->scale);
        lua_setfield(L, -2, "scale");
        lua_pushnumber(L, e.pinch->rotation);
        lua_setfield(L, -2, "rotation");
    }

    return 1;
}

static int pushGestureEvent(lua_State* L, const ITrackpadGesture::STrackpadGestureEnd& e) {
    pushGestureBase(L, "end", e.direction);

    if (e.swipe) {
        lua_pushstring(L, "swipe");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.swipe->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushboolean(L, e.swipe->cancelled);
        lua_setfield(L, -2, "cancelled");
    } else if (e.pinch) {
        lua_pushstring(L, "pinch");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.pinch->timeMs);
        lua_setfield(L, -2, "time_ms");
        lua_pushboolean(L, e.pinch->cancelled);
        lua_setfield(L, -2, "cancelled");
    }

    return 1;
}

CLuaFunctionGesture::CLuaFunctionGesture(Config::Lua::CLuaRef&& fn) : m_luaEndFn(std::move(fn)), m_legacyEndOnly(true) {
    ;
}

CLuaFunctionGesture::CLuaFunctionGesture(Config::Lua::CLuaRef&& start, Config::Lua::CLuaRef&& update, Config::Lua::CLuaRef&& end) :
    m_luaStartFn(std::move(start)), m_luaUpdateFn(std::move(update)), m_luaEndFn(std::move(end)) {
    ;
}

void CLuaFunctionGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    const auto MGR = Config::Lua::mgr();
    if (!MGR || !m_luaStartFn.state())
        return;

    MGR->caller().call(m_luaStartFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hl.gesture start callback", [&](lua_State* L) { return pushGestureEvent(L, e); });
}

void CLuaFunctionGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    const auto MGR = Config::Lua::mgr();
    if (!MGR || !m_luaUpdateFn.state())
        return;

    MGR->caller().call(m_luaUpdateFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hl.gesture update callback", [&](lua_State* L) { return pushGestureEvent(L, e); });
}

void CLuaFunctionGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    const auto MGR = Config::Lua::mgr();
    if (!MGR || !m_luaEndFn.state())
        return;

    // fires once, like a bind
    if (m_legacyEndOnly)
        MGR->caller().call(m_luaEndFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "hl.gesture callback");
    else
        MGR->caller().call(m_luaEndFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hl.gesture finish callback", [&](lua_State* L) { return pushGestureEvent(L, e); });
}
