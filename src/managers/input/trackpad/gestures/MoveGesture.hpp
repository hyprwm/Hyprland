#pragma once

#include "../ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"

class CMoveTrackpadGesture : public ITrackpadGesture {
  public:
    CMoveTrackpadGesture()          = default;
    virtual ~CMoveTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    PHLWINDOWREF m_window;
    Vector2D     m_lastDelta;
};
