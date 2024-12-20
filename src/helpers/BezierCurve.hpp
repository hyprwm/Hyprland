#pragma once

#include <vector>
#include <array>
#include <vector>
#include "math/Math.hpp"

constexpr int   BAKEDPOINTS    = 255;
constexpr float INVBAKEDPOINTS = 1.f / BAKEDPOINTS;

// an implementation of a cubic bezier curve
// might do better later
class CBezierCurve {
  public:
    // sets up the bezier curve.
    // this EXCLUDES the 0,0 and 1,1 points,
    void  setup(std::vector<Vector2D>* points);

    float getYForT(float const& t) const;
    float getXForT(float const& t) const;
    float getYForPoint(float const& x) const;

  private:
    // this INCLUDES the 0,0 and 1,1 points.
    std::vector<Vector2D>             m_vPoints;

    std::array<Vector2D, BAKEDPOINTS> m_aPointsBaked;
};
