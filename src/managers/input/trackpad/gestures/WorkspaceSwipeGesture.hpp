#pragma once

#include "ITrackpadGesture.hpp"
#include "../../../../desktop/DesktopTypes.hpp"
#include <cstdint>

class CWorkspaceSwipeGesture : public ITrackpadGesture {
  public:
    CWorkspaceSwipeGesture() = default;
    virtual ~CWorkspaceSwipeGesture();

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

    virtual bool isDirectionSensitive();

  private:
    void     cancel();

    uint64_t m_sessionID = 0;
};
