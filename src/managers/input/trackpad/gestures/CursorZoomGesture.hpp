#pragma once

#include "ITrackpadGesture.hpp"

class CCursorZoomTrackpadGesture : public ITrackpadGesture {
  public:
    CCursorZoomTrackpadGesture(const std::string& zoomLevel, const std::string& mode);
    virtual ~CCursorZoomTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    float              m_zoomValue = 1.0;
    inline static bool m_zoomed    = false;

    enum eMode : uint8_t {
        MODE_TOGGLE = 0,
        MODE_MULT,
    };

    eMode m_mode = MODE_TOGGLE;
};
