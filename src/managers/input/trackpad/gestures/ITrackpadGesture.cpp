#include "ITrackpadGesture.hpp"

float ITrackpadGesture::distance(const STrackpadGestureBegin& e) {
    if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return e.direction == TRACKPAD_GESTURE_DIR_LEFT ? -e.swipe->delta.x : e.swipe->delta.x;
    if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)
        return e.direction == TRACKPAD_GESTURE_DIR_UP ? -e.swipe->delta.y : e.swipe->delta.y;
    if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)
        return e.swipe->delta.size();
    if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT)
        return e.pinch->scale * 100.F; // 100 is a reasonable scale

    return e.swipe ? e.swipe->delta.size() : e.pinch->delta.size();
}

float ITrackpadGesture::distance(const STrackpadGestureUpdate& e) {
    if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return e.direction == TRACKPAD_GESTURE_DIR_LEFT ? -e.swipe->delta.x : e.swipe->delta.x;
    if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)
        return e.direction == TRACKPAD_GESTURE_DIR_UP ? -e.swipe->delta.y : e.swipe->delta.y;
    if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)
        return e.swipe->delta.size();
    if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT)
        return e.pinch->scale * 100.F; // 100 is a reasonable scale

    return e.swipe ? e.swipe->delta.size() : e.pinch->delta.size();
}
