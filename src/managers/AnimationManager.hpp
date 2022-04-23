#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/AnimatedVariable.hpp"

class CAnimationManager {
public:

    void            tick();

    std::list<CAnimatedVariable*> m_lAnimatedVariables;

private:
    bool            deltaSmallToFlip(const Vector2D& a, const Vector2D& b);
    bool            deltaSmallToFlip(const CColor& a, const CColor& b);
    bool            deltaSmallToFlip(const float& a, const float& b);
    bool            deltazero(const Vector2D& a, const Vector2D& b);
    bool            deltazero(const CColor& a, const CColor& b);
    bool            deltazero(const float& a, const float& b);
    double          parabolic(const double, const double, const double);
    CColor          parabolic(const double, const CColor&, const CColor&);
};

inline std::unique_ptr<CAnimationManager> g_pAnimationManager;