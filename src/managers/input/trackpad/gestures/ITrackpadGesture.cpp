#include "ITrackpadGesture.hpp"

// scale the pinch "scale" to match our imaginary delta units
constexpr const float PINCH_DELTA_SCALE         = 400.F;
constexpr const float PINCH_DELTA_SCALE_OUT_ADD = 1.6F;

//
void ITrackpadGesture::begin(const STrackpadGestureBegin& e) {
    m_lastPinchScale = 1.F;
    m_scale          = e.scale;
}

float ITrackpadGesture::distance(const STrackpadGestureBegin& e) {
    if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)
        return m_scale * (e.direction == TRACKPAD_GESTURE_DIR_LEFT ? -e.swipe->delta.x : e.swipe->delta.x);
    if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)
        return m_scale * (e.direction == TRACKPAD_GESTURE_DIR_UP ? -e.swipe->delta.y : e.swipe->delta.y);
    if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)
        return m_scale * (e.swipe->delta.size());
    if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT) {
        const auto Δ     = m_lastPinchScale - e.pinch->scale;
        m_lastPinchScale = e.pinch->scale;
        return m_scale * ((e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN ? -Δ : Δ * PINCH_DELTA_SCALE_OUT_ADD) * PINCH_DELTA_SCALE);
    }

    return m_scale * (e.swipe ? e.swipe->delta.size() : e.pinch->delta.size());
}

float ITrackpadGesture::distance(const STrackpadGestureUpdate& e) {
    return ITrackpadGesture::distance(STrackpadGestureBegin{
        .swipe     = e.swipe,
        .pinch     = e.pinch,
        .direction = e.direction,
        .scale     = e.scale,
    });
}

bool ITrackpadGesture::isDirectionSensitive() {
    return false;
}
