#pragma once

#include "ITrackpadGesture.hpp"

namespace Overview {
    class IOverview;
}

class COverviewTrackpadGesture : public ITrackpadGesture {
  public:
    COverviewTrackpadGesture()          = default;
    virtual ~COverviewTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    const Overview::IOverview* m_overview    = nullptr;
    float                      m_distance    = 0.F;
    bool                       m_active      = false;
    bool                       m_interactive = false;
};
