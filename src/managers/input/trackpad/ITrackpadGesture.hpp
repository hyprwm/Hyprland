#pragma once

#include "../../../devices/IPointer.hpp"

class ITrackpadGesture {
  public:
    virtual ~ITrackpadGesture() = default;

    struct STrackpadGestureBegin {
        // this has update because we wait for the delta
        const IPointer::SSwipeUpdateEvent* swipe = nullptr;
        const IPointer::SPinchUpdateEvent* pinch = nullptr;
    };

    struct STrackpadGestureUpdate {
        const IPointer::SSwipeUpdateEvent* swipe = nullptr;
        const IPointer::SPinchUpdateEvent* pinch = nullptr;
    };

    struct STrackpadGestureEnd {
        const IPointer::SSwipeEndEvent* swipe = nullptr;
        const IPointer::SPinchEndEvent* pinch = nullptr;
    };

    virtual void begin(const STrackpadGestureBegin& e)   = 0;
    virtual void update(const STrackpadGestureUpdate& e) = 0;
    virtual void end(const STrackpadGestureEnd& e)       = 0;
};