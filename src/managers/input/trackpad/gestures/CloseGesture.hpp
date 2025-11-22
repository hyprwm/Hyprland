#pragma once

#include "ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"

class CCloseTrackpadGesture : public ITrackpadGesture {
  public:
    CCloseTrackpadGesture()          = default;
    virtual ~CCloseTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    PHLWINDOWREF m_window;

    Vector2D     m_posFrom, m_posTo, m_sizeFrom, m_sizeTo;
    float        m_alphaFrom = 0.F, m_alphaTo = 0.F;

    float        m_lastDelta = 0.F;
};
