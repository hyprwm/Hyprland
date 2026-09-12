#pragma once

#include "../../../devices/IPointer.hpp"
#include "../../../input/Keys.hpp"
#include "gestures/ITrackpadGesture.hpp"
#include "GestureTypes.hpp"

#include <expected>
#include <vector>

class CTrackpadGestures {
  public:
    void                             clearGestures();
    std::expected<void, std::string> addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                float deltaScale, bool disableInhibit);
    std::expected<void, std::string> removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale, bool disableInhibit);

    void                             gestureBegin(const IPointer::SSwipeBeginEvent& e);
    void                             gestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    void                             gestureEnd(const IPointer::SSwipeEndEvent& e);

    void                             gestureBegin(const IPointer::SPinchBeginEvent& e);
    void                             gestureUpdate(const IPointer::SPinchUpdateEvent& e);
    void                             gestureEnd(const IPointer::SPinchEndEvent& e);

    eTrackpadGestureDirection        dirForString(const std::string_view& s);

  private:
    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        size_t                    fingerCount      = 0;
        Input::ModifierMask       modMask          = Input::HL_MODIFIER_NONE;
        eTrackpadGestureDirection direction        = TRACKPAD_GESTURE_DIR_NONE;
        float                     deltaScale       = 1.F;
        bool                      disableInhibit   = false;
        eTrackpadGestureDirection currentDirection = TRACKPAD_GESTURE_DIR_NONE;
    };

    std::vector<SP<SGestureData>> m_gestures;
    Vector2D                      m_currentTotalDelta = {};
    SP<SGestureData>              m_activeGesture     = nullptr;
    bool                          m_gestureFindFailed = false;
};

inline UP<CTrackpadGestures> g_pTrackpadGestures = makeUnique<CTrackpadGestures>();
