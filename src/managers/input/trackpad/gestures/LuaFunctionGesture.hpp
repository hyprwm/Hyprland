#pragma once

#include "ITrackpadGesture.hpp"

class CLuaFunctionGesture : public ITrackpadGesture {
  public:
    CLuaFunctionGesture(int ref);
    virtual ~CLuaFunctionGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    int m_luaFnId = 0;
};
