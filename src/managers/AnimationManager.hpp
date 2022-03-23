#pragma once

#include "../defines.hpp"
#include <list>

class CAnimationManager {
public:

    void            tick();

private:
    bool            deltaSmallToFlip(const Vector2D& a, const Vector2D& b);
    bool            deltazero(const Vector2D& a, const Vector2D& b);
    double          parabolic(double, double, double);
};

inline std::unique_ptr<CAnimationManager> g_pAnimationManager;