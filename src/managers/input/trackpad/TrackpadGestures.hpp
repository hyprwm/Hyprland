#pragma once

#include "../../../devices/IPointer.hpp"
#include "../../../input/Keys.hpp"
#include "gestures/ITrackpadGesture.hpp"
#include "GestureTypes.hpp"

#include <optional>

namespace Keybinds {
    class CBind;
}

#include <vector>
#include <expected>

enum eGestureDeltaSource : uint8_t {
    GESTURE_DELTA_SOURCE_NONE = 0,
    GESTURE_DELTA_SOURCE_MOUSE,
    GESTURE_DELTA_SOURCE_WHEEL,
    GESTURE_DELTA_SOURCE_FINGER,
    GESTURE_DELTA_SOURCE_CONTINUOUS,
    GESTURE_DELTA_SOURCE_WHEEL_TILT,
};

class CTrackpadGestures {
  public:
    void                             clearGestures();
    std::expected<void, std::string> addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                float deltaScale, bool disableInhibit, std::string trigger = {}, eGestureDeltaSource source = GESTURE_DELTA_SOURCE_NONE);
    std::expected<void, std::string> removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale, bool disableInhibit,
                                                   std::string trigger = {}, eGestureDeltaSource source = GESTURE_DELTA_SOURCE_NONE);

    void                             gestureBegin(const IPointer::SSwipeBeginEvent& e);
    void                             gestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    void                             gestureEnd(const IPointer::SSwipeEndEvent& e);
    void                             pointerGestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    bool                             axisGestureUpdate(const IPointer::SAxisEvent& e);
    void                             pointerGestureEnd(bool cancelled = false);
    bool                             pointerGestureActive() const;

    void                             gestureBegin(const IPointer::SPinchBeginEvent& e);
    void                             gestureUpdate(const IPointer::SPinchUpdateEvent& e);
    void                             gestureEnd(const IPointer::SPinchEndEvent& e);

    eTrackpadGestureDirection        dirForString(const std::string_view& s);
    const char*                      stringForDir(eTrackpadGestureDirection dir);

  private:
    bool                             pointerGestureBegin(const std::string& trigger, Input::ModifierMask modMask);
    std::expected<void, std::string> addTriggerBind(const std::string& trigger, Input::ModifierMask modMask);
    void                             removeTriggerBind(const std::string& trigger, Input::ModifierMask modMask);
    bool                             hasGestureForTrigger(const std::string& trigger, Input::ModifierMask modMask) const;
    bool                             hasGestureForSource(eGestureDeltaSource source) const;
    void                             swipeUpdate(const IPointer::SSwipeUpdateEvent& e, eGestureDeltaSource source = GESTURE_DELTA_SOURCE_NONE);

    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        size_t                    fingerCount = 0;
        std::string               trigger; // empty for native touchpad gestures
        eGestureDeltaSource       source           = GESTURE_DELTA_SOURCE_NONE;
        Input::ModifierMask       modMask          = Input::HL_MODIFIER_NONE;
        eTrackpadGestureDirection direction        = TRACKPAD_GESTURE_DIR_NONE; // configured dir
        float                     deltaScale       = 1.F;
        bool                      disableInhibit   = false;
        eTrackpadGestureDirection currentDirection = TRACKPAD_GESTURE_DIR_NONE; // actual dir of that select swipe
    };

    std::vector<SP<SGestureData>>    m_gestures;
    std::vector<SP<Keybinds::CBind>> m_triggerBinds;

    Vector2D                         m_currentTotalDelta = {};
    SP<SGestureData>                 m_activeGesture     = nullptr;
    bool                             m_gestureFindFailed = false;
    std::string                      m_activeTrigger;
    Input::ModifierMask              m_activeModMask     = Input::HL_MODIFIER_NONE;
    eGestureDeltaSource              m_activeDeltaSource = GESTURE_DELTA_SOURCE_NONE;
};

inline UP<CTrackpadGestures> g_pTrackpadGestures = makeUnique<CTrackpadGestures>();
