#include "Box.hpp"

#include <limits>
#include <algorithm>

wlr_box CBox::wlr() {
    CBox rounded = roundInternal();
    m_bWlrBox    = wlr_box{(int)rounded.x, (int)rounded.y, (int)rounded.w, (int)rounded.h};
    return m_bWlrBox;
}

wlr_box* CBox::pWlr() {
    CBox rounded = roundInternal();
    m_bWlrBox    = wlr_box{(int)rounded.x, (int)rounded.y, (int)rounded.w, (int)rounded.h};
    return &m_bWlrBox;
}

CBox& CBox::scale(double scale) {
    x *= scale;
    y *= scale;
    w *= scale;
    h *= scale;

    return *this;
}

CBox& CBox::scale(const Vector2D& scale) {
    x *= scale.x;
    y *= scale.y;
    w *= scale.x;
    h *= scale.y;

    return *this;
}

CBox& CBox::translate(const Vector2D& vec) {
    x += vec.x;
    y += vec.y;

    return *this;
}

Vector2D CBox::middle() const {
    return Vector2D{x + w / 2.0, y + h / 2.0};
}

bool CBox::containsPoint(const Vector2D& vec) const {
    return VECINRECT(vec, x, y, x + w, y + h);
}

bool CBox::empty() const {
    return w == 0 || h == 0;
}

CBox& CBox::applyFromWlr() {
    x = m_bWlrBox.x;
    y = m_bWlrBox.y;
    w = m_bWlrBox.width;
    h = m_bWlrBox.height;

    return *this;
}

CBox& CBox::round() {
    float newW = x + w - std::round(x);
    float newH = y + h - std::round(y);
    x          = std::round(x);
    y          = std::round(y);
    w          = std::round(newW);
    h          = std::round(newH);

    return *this;
}

CBox& CBox::transform(const wl_output_transform t, double w, double h) {
    wlr_box_transform(&m_bWlrBox, pWlr(), t, w, h);
    applyFromWlr();

    return *this;
}

CBox& CBox::addExtents(const SWindowDecorationExtents& e) {
    x -= e.topLeft.x;
    y -= e.topLeft.y;
    w += e.topLeft.x + e.bottomRight.x;
    h += e.topLeft.y + e.bottomRight.y;

    return *this;
}

CBox& CBox::scaleFromCenter(double scale) {
    double oldW = w, oldH = h;

    w *= scale;
    h *= scale;

    x -= (w - oldW) / 2.0;
    y -= (h - oldH) / 2.0;

    return *this;
}

CBox& CBox::expand(const double& value) {
    x -= value;
    y -= value;
    w += value * 2.0;
    h += value * 2.0;

    return *this;
}

CBox& CBox::noNegativeSize() {
    std::clamp(w, 0.0, std::numeric_limits<double>::infinity());
    std::clamp(h, 0.0, std::numeric_limits<double>::infinity());

    return *this;
}

CBox& CBox::intersection(const CBox other) {
    const float newTop    = std::max(y, other.y);
    const float newBottom = std::min(y + h, other.y + other.h);
    const float newLeft   = std::max(x, other.x);
    const float newRight  = std::min(x + w, other.x + other.w);
    y                     = newTop;
    x                     = newLeft;
    w                     = newRight - newLeft;
    h                     = newBottom - newTop;

    if (w <= 0 || h <= 0) {
        w = 0;
        h = 0;
    }

    return *this;
}

CBox CBox::roundInternal() {
    float newW = x + w - std::floor(x);
    float newH = y + h - std::floor(y);

    return CBox{std::floor(x), std::floor(y), std::floor(newW), std::floor(newH)};
}

CBox CBox::copy() const {
    return CBox{*this};
}

Vector2D CBox::pos() const {
    return {x, y};
}

Vector2D CBox::size() const {
    return {w, h};
}

SWindowDecorationExtents CBox::extentsFrom(const CBox& small) {
    return {{small.x - x, small.y - y}, {w - small.w - (small.x - x), h - small.h - (small.y - y)}};
}
