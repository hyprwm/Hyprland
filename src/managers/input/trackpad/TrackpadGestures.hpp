#pragma once

#include "../../../devices/IPointer.hpp"
#include "../../../input/Keys.hpp"

#include "gestures/ITrackpadGesture.hpp"
#include "GestureTypes.hpp"

#include <vector>
#include <expected>

class CTrackpadGestures {
  public:
    void                             clearGestures();
    std::expected<void, std::string> addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                float deltaScale, bool disableInhibit, uint32_t pointerButton = 0);
    std::expected<void, std::string> removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale, bool disableInhibit,
                                                   uint32_t pointerButton = 0);

    void                             gestureBegin(const IPointer::SSwipeBeginEvent& e);
    void                             gestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    void                             gestureEnd(const IPointer::SSwipeEndEvent& e);
    bool                             pointerGestureBegin(uint32_t button, uint32_t timeMs);
    void                             pointerGestureUpdate(uint32_t button, const IPointer::SSwipeUpdateEvent& e);
    void                             pointerGestureEnd(uint32_t button, const IPointer::SSwipeEndEvent& e);

    void                             gestureBegin(const IPointer::SPinchBeginEvent& e);
    void                             gestureUpdate(const IPointer::SPinchUpdateEvent& e);
    void                             gestureEnd(const IPointer::SPinchEndEvent& e);

    eTrackpadGestureDirection        dirForString(const std::string_view& s);
    const char*                      stringForDir(eTrackpadGestureDirection dir);

  private:
    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        size_t                    fingerCount      = 0;
        uint32_t                  pointerButton    = 0;
        Input::ModifierMask       modMask          = Input::HL_MODIFIER_NONE;
        eTrackpadGestureDirection direction        = TRACKPAD_GESTURE_DIR_NONE; // configured dir
        float                     deltaScale       = 1.F;
        bool                      disableInhibit   = false;
        eTrackpadGestureDirection currentDirection = TRACKPAD_GESTURE_DIR_NONE; // actual dir of that select swipe
    };

    std::vector<SP<SGestureData>> m_gestures;

    Vector2D                      m_currentTotalDelta   = {};
    SP<SGestureData>              m_activeGesture       = nullptr;
    bool                          m_gestureFindFailed   = false;
    uint32_t                      m_activePointerButton = 0;
};

inline UP<CTrackpadGestures> g_pTrackpadGestures = makeUnique<CTrackpadGestures>();
