#pragma once

#include "../../../../devices/IPointer.hpp"
#include "../GestureTypes.hpp"

class ITrackpadGesture {
  public:
    virtual ~ITrackpadGesture() = default;

    struct STrackpadGestureBegin {
        // this has update because we wait for the delta
        const IPointer::SSwipeUpdateEvent* swipe     = nullptr;
        const IPointer::SPinchUpdateEvent* pinch     = nullptr;
        eTrackpadGestureDirection          direction = TRACKPAD_GESTURE_DIR_NONE;
    };

    struct STrackpadGestureUpdate {
        const IPointer::SSwipeUpdateEvent* swipe     = nullptr;
        const IPointer::SPinchUpdateEvent* pinch     = nullptr;
        eTrackpadGestureDirection          direction = TRACKPAD_GESTURE_DIR_NONE;
    };

    struct STrackpadGestureEnd {
        const IPointer::SSwipeEndEvent* swipe     = nullptr;
        const IPointer::SPinchEndEvent* pinch     = nullptr;
        eTrackpadGestureDirection       direction = TRACKPAD_GESTURE_DIR_NONE;
    };

    virtual void  begin(const STrackpadGestureBegin& e);
    virtual void  update(const STrackpadGestureUpdate& e) = 0;
    virtual void  end(const STrackpadGestureEnd& e)       = 0;

    virtual float distance(const STrackpadGestureBegin& e);
    virtual float distance(const STrackpadGestureUpdate& e);

    virtual bool isDirectionSensitive();

  protected:
    float m_lastPinchScale = 1.F;
};