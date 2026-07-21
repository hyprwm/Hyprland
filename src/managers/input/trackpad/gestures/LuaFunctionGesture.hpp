#pragma once

#include "ITrackpadGesture.hpp"

class CLuaFunctionGesture : public ITrackpadGesture {
  public:
    CLuaFunctionGesture(int ref);
    CLuaFunctionGesture(int startRef, int updateRef, int endRef);
    virtual ~CLuaFunctionGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    int  m_luaStartFnId  = -2;
    int  m_luaUpdateFnId = -2;
    int  m_luaEndFnId    = -2;
    bool m_legacyEndOnly = false;
};
