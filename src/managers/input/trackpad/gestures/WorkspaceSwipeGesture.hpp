#pragma once

#include "ITrackpadGesture.hpp"
#include "../../../../desktop/DesktopTypes.hpp"

namespace Overview {
    class IOverview;
}

class CWorkspaceSwipeGesture : public ITrackpadGesture {
  public:
    CWorkspaceSwipeGesture()          = default;
    virtual ~CWorkspaceSwipeGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

    virtual bool isDirectionSensitive();

  private:
    Overview::IOverview* m_overview           = nullptr;
    bool                 m_overviewMoveActive = false;
};
