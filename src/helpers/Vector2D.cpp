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

double Vector2D::dot(const Vector2D& other) const {
    return x * other.x + y * other.y;
}

double Vector2D::distance(const Vector2D& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}

double Vector2D::distanceFromSegment(const Vector2D& p1, const Vector2D& p2) const {
    Vector2D segmentVector = p2 - p1;

    // Vector from p1 to the point
    Vector2D pointVector = *this - p1;

    // Calculate the projection of pointVector onto segmentVector
    double t = pointVector.dot(segmentVector) / segmentVector.dot(segmentVector);

    // Check if the closest point is within the segment
    if (t < 0.0) {
        // Closest point is beyond p1
        return this->distance(p1);
    } else if (t > 1.0) {
        // Closest point is beyond p2
        return this->distance(p2);
    } else {
        // Closest point is within the segment
        Vector2D closestPoint = p1 + segmentVector * t;
        return this->distance(closestPoint);
    }
}

bool Vector2D::inTriangle(const Vector2D& p1, const Vector2D& p2, const Vector2D& p3) const {
    const auto a = ((p2.y - p3.y) * (x - p3.x) + (p3.x - p2.x) * (y - p3.y)) / ((p2.y - p3.y) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.y - p3.y));
    const auto b = ((p3.y - p1.y) * (x - p3.x) + (p1.x - p3.x) * (y - p3.y)) / ((p2.y - p3.y) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.y - p3.y));
    const auto c = 1 - a - b;

    return 0 <= a && a <= 1 && 0 <= b && b <= 1 && 0 <= c && c <= 1;
}

double Vector2D::size() const {
    return std::sqrt(x * x + y * y);
}
