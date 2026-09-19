#pragma once

#include "../../../devices/IPointer.hpp"
#include "../../../input/Keys.hpp"
#include "gestures/ITrackpadGesture.hpp"
#include "GestureTypes.hpp"

#include <expected>
#include <optional>
#include <vector>

namespace Keybinds {
    class CBind;
}

enum eGestureDeltaSource : uint8_t {
    GESTURE_DELTA_SOURCE_NONE = 0,
    GESTURE_DELTA_SOURCE_MOUSE,
    GESTURE_DELTA_SOURCE_WHEEL,
    GESTURE_DELTA_SOURCE_FINGER,
    GESTURE_DELTA_SOURCE_CONTINUOUS,
    GESTURE_DELTA_SOURCE_WHEEL_TILT,
};

class CTriggeredGestures {
  public:
    void                             clearGestures();
    std::expected<void, std::string> addGesture(UP<ITrackpadGesture>&& gesture, std::string trigger, eGestureDeltaSource source, eTrackpadGestureDirection direction,
                                                Input::ModifierMask modMask, float deltaScale, bool disableInhibit);
    std::expected<void, std::string> removeGesture(const std::string& trigger, eGestureDeltaSource source, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                   float deltaScale, bool disableInhibit);

    void                             pointerGestureUpdate(const IPointer::SSwipeUpdateEvent& e);
    bool                             axisGestureUpdate(const IPointer::SAxisEvent& e);
    void                             pointerGestureEnd(bool cancelled = false);
    bool                             pointerGestureActive() const;

  private:
    bool                             pointerGestureBegin(const std::string& trigger, Input::ModifierMask modMask);
    std::expected<void, std::string> addTriggerBind(const std::string& trigger, Input::ModifierMask modMask);
    void                             removeTriggerBind(const std::string& trigger, Input::ModifierMask modMask);
    bool                             hasGestureForTrigger(const std::string& trigger, Input::ModifierMask modMask) const;
    bool                             hasGestureForSource(eGestureDeltaSource source) const;
    void                             swipeUpdate(const IPointer::SSwipeUpdateEvent& e, eGestureDeltaSource source);

    struct SGestureData {
        UP<ITrackpadGesture>      gesture;
        std::string               trigger;
        eGestureDeltaSource       source           = GESTURE_DELTA_SOURCE_NONE;
        Input::ModifierMask       modMask          = Input::HL_MODIFIER_NONE;
        eTrackpadGestureDirection direction        = TRACKPAD_GESTURE_DIR_NONE;
        float                     deltaScale       = 1.F;
        bool                      disableInhibit   = false;
        eTrackpadGestureDirection currentDirection = TRACKPAD_GESTURE_DIR_NONE;
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

inline UP<CTriggeredGestures> g_pTriggeredGestures = makeUnique<CTriggeredGestures>();
