#include "Vector2D.hpp"
#include <algorithm>
#include <cmath>

Vector2D::Vector2D(double xx, double yy) {
    x = xx;
    y = yy;
}

Vector2D::Vector2D() {
    x = 0;
    y = 0;
}

Vector2D::~Vector2D() {}

double Vector2D::normalize() {
    // get max abs
    const auto max = abs(x) > abs(y) ? abs(x) : abs(y);

    x /= max;
    y /= max;

    return max;
}

Vector2D Vector2D::floor() {
    return Vector2D((int)x, (int)y);
}

Vector2D Vector2D::clamp(const Vector2D& min, const Vector2D& max) {
    return Vector2D(std::clamp(this->x, min.x, max.x == 0 ? INFINITY : max.x), std::clamp(this->y, min.y, max.y == 0 ? INFINITY : max.y));
}

double Vector2D::distance(const Vector2D& other) {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}
