#include "LuaFunctionGesture.hpp"

#include "../../../../config/lua/ConfigManager.hpp"

CLuaFunctionGesture::CLuaFunctionGesture(int ref) : m_luaFnId(ref) {
    ;
}

void CLuaFunctionGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ; // intentionally blank
}

void CLuaFunctionGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    ; // intentionally blank
}

void CLuaFunctionGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!Config::Lua::mgr())
        return;

    Config::Lua::mgr()->callLuaFn(m_luaFnId);
}
