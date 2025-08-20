#pragma once

#include "../../../devices/IPointer.hpp"

class ITrackpadGesture {
  public:
    virtual ~ITrackpadGesture() = default;

    virtual void begin(const IPointer::SSwipeUpdateEvent& e)  = 0; // this has update because we wait for the delta
    virtual void update(const IPointer::SSwipeUpdateEvent& e) = 0;
    virtual void end(const IPointer::SSwipeEndEvent& e)       = 0;
};