#include "GestureTypes.hpp"

#include <cmath>

const char* gestureDirectionToString(eTrackpadGestureDirection direction) {
    switch (direction) {
        case TRACKPAD_GESTURE_DIR_NONE: return "NONE";
        case TRACKPAD_GESTURE_DIR_HORIZONTAL: return "HORIZONTAL";
        case TRACKPAD_GESTURE_DIR_VERTICAL: return "VERTICAL";
        case TRACKPAD_GESTURE_DIR_LEFT: return "LEFT";
        case TRACKPAD_GESTURE_DIR_RIGHT: return "RIGHT";
        case TRACKPAD_GESTURE_DIR_UP: return "UP";
        case TRACKPAD_GESTURE_DIR_DOWN: return "DOWN";
        case TRACKPAD_GESTURE_DIR_SWIPE: return "SWIPE";
        case TRACKPAD_GESTURE_DIR_PINCH: return "PINCH";
        case TRACKPAD_GESTURE_DIR_PINCH_IN: return "PINCH_IN";
        case TRACKPAD_GESTURE_DIR_PINCH_OUT: return "PINCH_OUT";
        default: return "ERROR";
    }
    return "ERROR";
}

eTrackpadGestureDirection gestureDirectionAxis(eTrackpadGestureDirection direction) {
    switch (direction) {
        case TRACKPAD_GESTURE_DIR_UP:
        case TRACKPAD_GESTURE_DIR_DOWN:
        case TRACKPAD_GESTURE_DIR_VERTICAL: return TRACKPAD_GESTURE_DIR_VERTICAL;
        case TRACKPAD_GESTURE_DIR_LEFT:
        case TRACKPAD_GESTURE_DIR_RIGHT:
        case TRACKPAD_GESTURE_DIR_HORIZONTAL: return TRACKPAD_GESTURE_DIR_HORIZONTAL;
        case TRACKPAD_GESTURE_DIR_SWIPE: return TRACKPAD_GESTURE_DIR_SWIPE;
        case TRACKPAD_GESTURE_DIR_PINCH:
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
        case TRACKPAD_GESTURE_DIR_PINCH_OUT: return TRACKPAD_GESTURE_DIR_PINCH;
        default: return TRACKPAD_GESTURE_DIR_NONE;
    }
}

bool gestureDirectionOvershadows(eTrackpadGestureDirection existing, eTrackpadGestureDirection candidate) {
    const auto AXIS = gestureDirectionAxis(candidate);
    return existing == AXIS || existing == candidate ||
        ((AXIS == TRACKPAD_GESTURE_DIR_VERTICAL || AXIS == TRACKPAD_GESTURE_DIR_HORIZONTAL) && existing == TRACKPAD_GESTURE_DIR_SWIPE);
}

SGestureSwipeDirection gestureSwipeDirectionForDelta(const Hyprutils::Math::Vector2D& delta) {
    const auto AXIS = std::abs(delta.x) > std::abs(delta.y) ? TRACKPAD_GESTURE_DIR_HORIZONTAL : TRACKPAD_GESTURE_DIR_VERTICAL;

    if (AXIS == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return {.axis = AXIS, .direction = delta.x < 0 ? TRACKPAD_GESTURE_DIR_LEFT : TRACKPAD_GESTURE_DIR_RIGHT};

    return {.axis = AXIS, .direction = delta.y < 0 ? TRACKPAD_GESTURE_DIR_UP : TRACKPAD_GESTURE_DIR_DOWN};
}
