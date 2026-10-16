#pragma once

#include "ITrackpadGesture.hpp"
#include "../../../../config/lua/LuaRef.hpp"

class CLuaFunctionGesture : public ITrackpadGesture {
  public:
    explicit CLuaFunctionGesture(Config::Lua::CLuaRef&& fn);
    CLuaFunctionGesture(Config::Lua::CLuaRef&& start, Config::Lua::CLuaRef&& update, Config::Lua::CLuaRef&& end);
    virtual ~CLuaFunctionGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    Config::Lua::CLuaRef m_luaStartFn;
    Config::Lua::CLuaRef m_luaUpdateFn;
    Config::Lua::CLuaRef m_luaEndFn;
    bool                 m_legacyEndOnly = false;
};
