#pragma once

#include <math.h>

class Vector2D {
  public:
    Vector2D(double, double);
    Vector2D();
    ~Vector2D();

    double x = 0;
    double y = 0;

    // returns the scale
    double   normalize();

    Vector2D operator+(const Vector2D& a) const {
        return Vector2D(this->x + a.x, this->y + a.y);
    }
    Vector2D operator-(const Vector2D& a) const {
        return Vector2D(this->x - a.x, this->y - a.y);
    }
    Vector2D operator*(const float& a) const {
        return Vector2D(this->x * a, this->y * a);
    }
    Vector2D operator/(const float& a) const {
        return Vector2D(this->x / a, this->y / a);
    }

    bool operator==(const Vector2D& a) const {
        return a.x == x && a.y == y;
    }

    bool operator!=(const Vector2D& a) const {
        return a.x != x || a.y != y;
    }

    Vector2D operator*(const Vector2D& a) const {
        return Vector2D(this->x * a.x, this->y * a.y);
    }

    Vector2D operator/(const Vector2D& a) const {
        return Vector2D(this->x / a.x, this->y / a.y);
    }
    
    double   distance(const Vector2D& other);

    Vector2D clamp(const Vector2D& min, const Vector2D& max = Vector2D());

    Vector2D floor();
};
