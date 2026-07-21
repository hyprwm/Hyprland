#include "LuaFunctionGesture.hpp"

#include "../../../../config/lua/ConfigManager.hpp"

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

CLuaFunctionGesture::CLuaFunctionGesture(int ref) : m_luaEndFnId(ref), m_legacyEndOnly(true) {
    ;
}

CLuaFunctionGesture::CLuaFunctionGesture(int startRef, int updateRef, int endRef) : m_luaStartFnId(startRef), m_luaUpdateFnId(updateRef), m_luaEndFnId(endRef) {
    ;
}

void CLuaFunctionGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    const auto MGR = Config::Lua::mgr();
    if (!MGR)
        return;

    MGR->callLuaFn(m_luaStartFnId, [&](lua_State* L) { return pushGestureEvent(L, e); }, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "gesture start callback");
}

void CLuaFunctionGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    const auto MGR = Config::Lua::mgr();
    if (!MGR)
        return;

    MGR->callLuaFn(m_luaUpdateFnId, [&](lua_State* L) { return pushGestureEvent(L, e); }, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "gesture update callback");
}

void CLuaFunctionGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    const auto MGR = Config::Lua::mgr();
    if (!MGR)
        return;

    if (m_legacyEndOnly)
        MGR->callLuaFn(m_luaEndFnId);
    else
        MGR->callLuaFn(m_luaEndFnId, [&](lua_State* L) { return pushGestureEvent(L, e); }, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "gesture end callback");
}
