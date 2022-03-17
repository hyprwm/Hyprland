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
    double normalize();

    Vector2D operator+(Vector2D a) {
        return Vector2D(this->x + a.x, this->y + a.y);
    }
    Vector2D operator-(Vector2D a) {
        return Vector2D(this->x - a.x, this->y - a.y);
    }
    Vector2D operator*(float a) {
        return Vector2D(this->x * a, this->y * a);
    }
    Vector2D operator/(float a) {
        return Vector2D(this->x / a, this->y / a);
    }

    bool operator==(Vector2D& a) {
        return a.x == x && a.y == y;
    }

    bool operator!=(Vector2D& a) {
        return a.x != x || a.y != y;
    }


    Vector2D floor();
};