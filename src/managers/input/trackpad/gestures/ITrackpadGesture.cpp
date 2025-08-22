#include "ITrackpadGesture.hpp"

// scale the pinch "scale" to match our imaginary delta units
constexpr const float PINCH_DELTA_SCALE = 400.F;

//
void ITrackpadGesture::begin(const STrackpadGestureBegin& e) {
    m_lastPinchScale = 1.F;
}

float ITrackpadGesture::distance(const STrackpadGestureBegin& e) {
    if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return e.direction == TRACKPAD_GESTURE_DIR_LEFT ? -e.swipe->delta.x : e.swipe->delta.x;
    if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)
        return e.direction == TRACKPAD_GESTURE_DIR_UP ? -e.swipe->delta.y : e.swipe->delta.y;
    if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)
        return e.swipe->delta.size();
    if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT) {
        const auto Δ     = e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN ? m_lastPinchScale - e.pinch->scale : e.pinch->scale - m_lastPinchScale;
        m_lastPinchScale = e.pinch->scale;
        return Δ * PINCH_DELTA_SCALE;
    }

    return e.swipe ? e.swipe->delta.size() : e.pinch->delta.size();
}

float ITrackpadGesture::distance(const STrackpadGestureUpdate& e) {
    if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return e.direction == TRACKPAD_GESTURE_DIR_LEFT ? -e.swipe->delta.x : e.swipe->delta.x;
    if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)
        return e.direction == TRACKPAD_GESTURE_DIR_UP ? -e.swipe->delta.y : e.swipe->delta.y;
    if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)
        return e.swipe->delta.size();
    if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT) {
        const auto Δ     = e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN ? m_lastPinchScale - e.pinch->scale : e.pinch->scale - m_lastPinchScale;
        m_lastPinchScale = e.pinch->scale;
        return Δ * PINCH_DELTA_SCALE;
    }

    return e.swipe ? e.swipe->delta.size() : e.pinch->delta.size();
}
