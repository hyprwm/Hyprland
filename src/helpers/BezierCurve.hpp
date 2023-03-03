#pragma once

#include "../defines.hpp"
#include <deque>

constexpr int   BAKEDPOINTS    = 255;
constexpr float INVBAKEDPOINTS = 1.f / BAKEDPOINTS;

// an implementation of a cubic bezier curve
// might do better later
// TODO: n-point curves
class CBezierCurve {
  public:
    // sets up the bezier curve.
    // this EXCLUDES the 0,0 and 1,1 points,
    void  setup(std::vector<Vector2D>* points);

    float getYForT(float t);
    float getXForT(float t);
    float getYForPoint(float x);

  private:
    // this INCLUDES the 0,0 and 1,1 points.
    std::deque<Vector2D>              m_dPoints;

    std::array<Vector2D, BAKEDPOINTS> m_aPointsBaked;
};