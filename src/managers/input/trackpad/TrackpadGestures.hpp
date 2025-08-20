#pragma once

#include "../../../devices/IPointer.hpp"

#include "ITrackpadGesture.hpp"

#include <vector>

class CTrackpadGestures {
  public:
    enum eTrackpadGestureDirection : uint8_t {
        TRACKPAD_GESTURE_DIR_NONE = 0,
        TRACKPAD_GESTURE_DIR_LEFT,
        TRACKPAD_GESTURE_DIR_RIGHT,
        TRACKPAD_GESTURE_DIR_UP,
        TRACKPAD_GESTURE_DIR_DOWN,
        TRACKPAD_GESTURE_DIR_VERTICAL,
        TRACKPAD_GESTURE_DIR_HORIZONTAL,
    };

    void                      clearGestures();
    void                      addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction);

    void                      gestureBegin(const IPointer::SSwipeBeginEvent& e);
    void                      gestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    void                      gestureEnd(const IPointer::SSwipeEndEvent& e);

    eTrackpadGestureDirection dirForString(const std::string_view& s);

  private:
    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        size_t                    fingerCount = 0;
        eTrackpadGestureDirection direction   = CTrackpadGestures::TRACKPAD_GESTURE_DIR_NONE;
    };

    std::vector<SP<SGestureData>> m_gestures;

    SP<SGestureData>              m_activeGesture     = nullptr;
    bool                          m_gestureFindFailed = false;
};

inline UP<CTrackpadGestures> g_pTrackpadGestures = makeUnique<CTrackpadGestures>();
