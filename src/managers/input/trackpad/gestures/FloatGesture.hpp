#pragma once

#include "ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"

class CFloatTrackpadGesture : public ITrackpadGesture {
  public:
    CFloatTrackpadGesture(const std::string_view& mode);
    virtual ~CFloatTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    PHLWINDOWREF m_window;

    Vector2D     m_posFrom, m_posTo, m_sizeFrom, m_sizeTo;

    float        m_lastDelta = 0;

    enum eMode : uint8_t {
        FLOAT_MODE_TOGGLE = 0,
        FLOAT_MODE_FLOAT,
        FLOAT_MODE_TILE,
    };

    eMode m_mode = FLOAT_MODE_TOGGLE;
};
