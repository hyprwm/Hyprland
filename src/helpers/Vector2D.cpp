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

Vector2D::Vector2D(const Hyprlang::VEC2& ref) {
    x = ref.x;
    y = ref.y;
}

Vector2D::~Vector2D() {}

double Vector2D::normalize() {
    // get max abs
    const auto max = std::abs(x) > std::abs(y) ? std::abs(x) : std::abs(y);

    x /= max;
    y /= max;

    return max;
}

Vector2D Vector2D::floor() const {
    return Vector2D(std::floor(x), std::floor(y));
}

Vector2D Vector2D::round() const {
    return Vector2D(std::round(x), std::round(y));
}

Vector2D Vector2D::clamp(const Vector2D& min, const Vector2D& max) const {
    return Vector2D(std::clamp(this->x, min.x, max.x < min.x ? INFINITY : max.x), std::clamp(this->y, min.y, max.y < min.y ? INFINITY : max.y));
}

double Vector2D::distance(const Vector2D& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}

double Vector2D::size() const {
    return std::sqrt(x * x + y * y);
}

Vector2D Vector2D::getComponentMax(const Vector2D& other) const {
    return Vector2D(std::max(this->x, other.x), std::max(this->y, other.y));
}
