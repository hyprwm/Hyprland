#pragma once

#include "ITrackpadGesture.hpp"
#include "../../../../desktop/DesktopTypes.hpp"

class CWorkspaceSwipeGesture : public ITrackpadGesture {
  public:
    CWorkspaceSwipeGesture()          = default;
    virtual ~CWorkspaceSwipeGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

    virtual bool isDirectionSensitive();
};
