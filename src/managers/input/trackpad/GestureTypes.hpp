#pragma once

#include <hyprutils/math/Vector2D.hpp>

#include <cstdint>

enum eTrackpadGestureDirection : uint8_t {
    TRACKPAD_GESTURE_DIR_NONE = 0,
    TRACKPAD_GESTURE_DIR_SWIPE,
    TRACKPAD_GESTURE_DIR_LEFT,
    TRACKPAD_GESTURE_DIR_RIGHT,
    TRACKPAD_GESTURE_DIR_UP,
    TRACKPAD_GESTURE_DIR_DOWN,
    TRACKPAD_GESTURE_DIR_VERTICAL,
    TRACKPAD_GESTURE_DIR_HORIZONTAL,
    TRACKPAD_GESTURE_DIR_PINCH,
    TRACKPAD_GESTURE_DIR_PINCH_OUT,
    TRACKPAD_GESTURE_DIR_PINCH_IN,
};

const char*               gestureDirectionToString(eTrackpadGestureDirection direction);
eTrackpadGestureDirection gestureDirectionAxis(eTrackpadGestureDirection direction);
bool                      gestureDirectionOvershadows(eTrackpadGestureDirection existing, eTrackpadGestureDirection candidate);

struct SGestureSwipeDirection {
    eTrackpadGestureDirection axis;
    eTrackpadGestureDirection direction;
};

SGestureSwipeDirection gestureSwipeDirectionForDelta(const Hyprutils::Math::Vector2D& delta);