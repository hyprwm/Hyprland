#pragma once

#include "../../../devices/IPointer.hpp"

#include "gestures/ITrackpadGesture.hpp"
#include "GestureTypes.hpp"

#include <vector>

class CTrackpadGestures {
  public:
    void                      clearGestures();
    void                      addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, uint32_t modMask);

    void                      gestureBegin(const IPointer::SSwipeBeginEvent& e);
    void                      gestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    void                      gestureEnd(const IPointer::SSwipeEndEvent& e);

    void                      gestureBegin(const IPointer::SPinchBeginEvent& e);
    void                      gestureUpdate(const IPointer::SPinchUpdateEvent& e);
    void                      gestureEnd(const IPointer::SPinchEndEvent& e);

    eTrackpadGestureDirection dirForString(const std::string_view& s);

  private:
    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        size_t                    fingerCount = 0;
        uint32_t                  modMask     = 0;
        eTrackpadGestureDirection direction   = TRACKPAD_GESTURE_DIR_NONE;
    };

    std::vector<SP<SGestureData>> m_gestures;

    SP<SGestureData>              m_activeGesture     = nullptr;
    bool                          m_gestureFindFailed = false;
};

inline UP<CTrackpadGestures> g_pTrackpadGestures = makeUnique<CTrackpadGestures>();
