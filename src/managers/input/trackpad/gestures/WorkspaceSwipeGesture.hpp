#pragma once

#include "../ITrackpadGesture.hpp"
#include "../../../../desktop/DesktopTypes.hpp"

class CWorkspaceSwipeGesture : public ITrackpadGesture {
  public:
    CWorkspaceSwipeGesture()          = default;
    virtual ~CWorkspaceSwipeGesture() = default;

    virtual void begin(const IPointer::SSwipeUpdateEvent& e);
    virtual void update(const IPointer::SSwipeUpdateEvent& e);
    virtual void end(const IPointer::SSwipeEndEvent& e);
};