#pragma once

#include <cmath>
#include <format>
#include "../macros.hpp"

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
    Vector2D operator-() const {
        return Vector2D(-this->x, -this->y);
    }
    Vector2D operator*(const double& a) const {
        return Vector2D(this->x * a, this->y * a);
    }
    Vector2D operator/(const double& a) const {
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

    bool operator>(const Vector2D& a) const {
        return this->x > a.x && this->y > a.y;
    }

    bool operator<(const Vector2D& a) const {
        return this->x < a.x && this->y < a.y;
    }
    Vector2D& operator+=(const Vector2D& a) {
        this->x += a.x;
        this->y += a.y;
        return *this;
    }
    Vector2D& operator-=(const Vector2D& a) {
        this->x -= a.x;
        this->y -= a.y;
        return *this;
    }
    Vector2D& operator*=(const Vector2D& a) {
        this->x *= a.x;
        this->y *= a.y;
        return *this;
    }
    Vector2D& operator/=(const Vector2D& a) {
        this->x /= a.x;
        this->y /= a.y;
        return *this;
    }
    Vector2D& operator*=(const double& a) {
        this->x *= a;
        this->y *= a;
        return *this;
    }
    Vector2D& operator/=(const double& a) {
        this->x /= a;
        this->y /= a;
        return *this;
    }

    double   distance(const Vector2D& other) const;
    double   size() const;
    Vector2D clamp(const Vector2D& min, const Vector2D& max = Vector2D()) const;

    Vector2D floor() const;
    Vector2D round() const;

    Vector2D getComponentMax(const Vector2D& other) const;
};

/**
    format specification
    - 'j', as json array
    - 'X', same as std::format("{}x{}", vec.x, vec.y)
    - number, floating point precision, use `0` to format as integer
*/
template <typename CharT>
struct std::formatter<Vector2D, CharT> : std::formatter<CharT> {
    bool        formatJson = false;
    bool        formatX    = false;
    std::string precision  = "";
    FORMAT_PARSE(FORMAT_FLAG('j', formatJson) //
                 FORMAT_FLAG('X', formatX)    //
                 FORMAT_NUMBER(precision),
                 Vector2D)

    template <typename FormatContext>
    auto format(const Vector2D& vec, FormatContext& ctx) const {
        std::string formatString = precision.empty() ? "{}" : std::format("{{:.{}f}}", precision);

        if (formatJson)
            formatString = std::format("[{0}, {0}]", formatString);
        else if (formatX)
            formatString = std::format("{0}x{0}", formatString);
        else
            formatString = std::format("[Vector2D: x: {0}, y: {0}]", formatString);
        try {
            string buf = std::vformat(formatString, std::make_format_args(vec.x, vec.y));
            return std::format_to(ctx.out(), "{}", buf);
        } catch (std::format_error& e) { return std::format_to(ctx.out(), "[{}, {}]", vec.x, vec.y); }
    }
};
