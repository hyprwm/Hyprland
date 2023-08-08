#pragma once

#include "Region.hpp"
#include "Vector2D.hpp"
#include <array>

#define DAMAGE_RING_LENGTH 2

class CDamageRing {
  public:
    CRegion getDamage(int age);
    void    setBounds(const Vector2D& max);
    void    damageEntire();
    void    addDamage(const CRegion& damage, bool blur = false);
    void    rotate();
    bool    empty();

  private:
    Vector2D                                m_vBounds;
    std::array<CRegion, DAMAGE_RING_LENGTH> m_aDamageFrames;
    std::array<CRegion, DAMAGE_RING_LENGTH> m_aBlurDamageFrames;
};